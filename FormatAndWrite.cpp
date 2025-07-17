#include <Windows.h>        // Основные функции WinAPI
#include <iostream>         // Ввод/вывод через wcout/wcerr
#include <string>           // std::wstring
#include <vector>           
#include <winioctl.h>       // IOCTL_* - работа с дисками
#include <shlwapi.h>        // Работа с путями строками и форматированием
#include <shlobj_core.h>    // SHFormatDrive - форматирование томов

#pragma comment(lib, "Ole32.lib")     // Связывание с библиотеками
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Shlwapi.lib")

using namespace std;

// Вывод последней ошибки WinAPI с пояснением
void PrintLastError(const wchar_t* msg) {
    DWORD err = GetLastError();
    if (err == 0) {
        wcerr << msg << L": нет ошибки\n";
        return;
    }

    wchar_t* errMsg = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&errMsg, 0, nullptr
    );

    wcerr << msg << L": код ошибки " << err << L" - " << (errMsg ? errMsg : L"") << L"\n";

    if (errMsg)
        LocalFree(errMsg);
}

// Проверка, запущена ли программа от имени администратора
bool IsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&NtAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin != FALSE;
}

// Проверка, что диск достаточно велик (чтобы диск умещался)
bool CheckDiskSize(HANDLE hDisk, ULONGLONG minSizeBytes) {
    DISK_GEOMETRY_EX geometry = {};
    DWORD bytesReturned;
    if (!DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, nullptr, 0, &geometry, sizeof(geometry), &bytesReturned, nullptr)) {
        PrintLastError(L"IOCTL_DISK_GET_DRIVE_GEOMETRY_EX не выполнен");
        return false;
    }

    if (geometry.DiskSize.QuadPart < minSizeBytes) {
        wcerr << L"Диск слишком мал. Минимум " << (minSizeBytes / (1024 * 1024)) << L" МБ\n";
        return false;
    }
    return true;
}

// Инициализация диска mbr с одним разделом
bool InitializeDiskMBR(HANDLE hDisk) {
    DISK_GEOMETRY_EX geometry = {};
    DWORD bytesReturned;
    if (!DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, nullptr, 0, &geometry, sizeof(geometry), &bytesReturned, nullptr)) {
        PrintLastError(L"IOCTL_DISK_GET_DRIVE_GEOMETRY_EX не выполнен");
        return false;
    }

    // cмещение начала раздела (1 МБ для выравнивания)
    ULONGLONG partitionStart = 1024 * 1024;
    ULONGLONG partitionLength = geometry.DiskSize.QuadPart > partitionStart ?
        geometry.DiskSize.QuadPart - partitionStart : 0;

    if (partitionLength == 0) {
        wcerr << L"Размер диска слишком мал для создания раздела\n";
        return false;
    }

    // Инициализируем сектор MBR размером 512 байт
    BYTE mbr[512] = {};
    mbr[510] = 0x55;  // Завершающая сигнатура MBR
    mbr[511] = 0xAA;

    // заполняется здесь одна запись раздела в MBR (16 байт с байта 446)
    BYTE* partEntry = mbr + 446;
    partEntry[0] = 0x80; // Аативный раздел
    partEntry[4] = 0x07; // что NTFS

    DWORD startSector = static_cast<DWORD>(partitionStart / 512);
    DWORD lengthSectors = static_cast<DWORD>(partitionLength / 512);

    // Запись LBA начала и длины раздела
    memcpy(partEntry + 8, &startSector, 4);
    memcpy(partEntry + 12, &lengthSectors, 4);

    // Установка позиции в начало диска
    LARGE_INTEGER offset = {};
    offset.QuadPart = 0;
    if (!SetFilePointerEx(hDisk, offset, nullptr, FILE_BEGIN)) {
        PrintLastError(L"SetFilePointerEx для MBR не выполнен");
        return false;
    }

    // запись MBR на диск
    DWORD written = 0;
    if (!WriteFile(hDisk, mbr, sizeof(mbr), &written, nullptr) || written != sizeof(mbr)) {
        PrintLastError(L"запись на MBR не выполнена");
        return false;
    }

    // Сброс буферов
    if (!FlushFileBuffers(hDisk)) {
        PrintLastError(L"буфер не выполнен");
        return false;
    }

    return true;
}

// назначение буквы диска созданному разделу
bool AssignDriveLetter(int diskNumber, wchar_t& outLetter) {
    WCHAR volumeName[MAX_PATH];
    HANDLE findHandle = FindFirstVolumeW(volumeName, ARRAYSIZE(volumeName));
    if (findHandle == INVALID_HANDLE_VALUE) {
        PrintLastError(L"FindFirstVolumeW не выполнен");
        return false;
    }

    bool assigned = false;
    do {
        int volDiskNumber = -1;

        // мткрываем том
        HANDLE hVol = CreateFileW(volumeName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hVol == INVALID_HANDLE_VALUE){
            continue;
        }

        // Определяем номер физического диска, на котором находится том
        VOLUME_DISK_EXTENTS extents = {};
        DWORD bytesReturned = 0;
        bool success = DeviceIoControl(hVol, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr, 0, &extents, sizeof(extents), &bytesReturned, nullptr) != FALSE;

        CloseHandle(hVol);

            if (!success || extents.NumberOfDiskExtents == 0) continue;
        volDiskNumber = extents.Extents[0].DiskNumber;
        if (volDiskNumber != diskNumber) continue;

        // Проверка - может, у тома уже есть буква
        WCHAR mountPoints[1024] = {};
        DWORD returnLength = 0;
        if (GetVolumePathNamesForVolumeNameW(volumeName, mountPoints, ARRAYSIZE(mountPoints), &returnLength)) {
            if (wcslen(mountPoints) > 0) {
            outLetter = mountPoints[0];
            FindVolumeClose(findHandle);
            return true;
            }
        }

        // Назначаем первую свободную букву от D до Z
        for (wchar_t letter = L'D'; letter <= L'Z'; ++letter) {
            wchar_t rootPath[] = L"A:\\";
            rootPath[0] = letter;
            if (GetDriveTypeW(rootPath) == DRIVE_NO_ROOT_DIR) {
                if (SetVolumeMountPointW(rootPath, volumeName)) {
                                outLetter = letter;
                                assigned = true;
                                break;
                }
            }
        }
        if (assigned) break;

    } while (FindNextVolumeW(findHandle, volumeName, ARRAYSIZE(volumeName)));

    FindVolumeClose(findHandle);
    return assigned;
}

// Форматирование раздела с помощью SHFormatDrive
bool FormatPartition(wchar_t driveLetter) {
    int result = SHFormatDrive(nullptr, driveLetter - L'A', SHFMT_ID_DEFAULT, SHFMT_OPT_FULL);
    if (result == SHFMT_ERROR || result == SHFMT_CANCEL) {
        wcerr << L"Ошибка форматированиякод: " << result << L"\n";
        return false;
    }
    return true;
}

// Точка входа
int wmain(int argc, wchar_t* argv[]) {
    if (!IsAdmin()) {
        wcerr << L"Запустите программу администратором\n";
        return 1;
    }

    if (argc != 2) {
        wcerr << L"Использование: FormatMBRDisk <номер_диска>\n";
        return 1;
    }

    int diskNum = _wtoi(argv[1]);
    if (diskNum < 0) {
        wcerr << L"Некорректный номер диска\n";
        return 1;
    }

    wstring diskPath = L"\\\\.\\PhysicalDrive" + to_wstring(diskNum);

    // Открываем диск
    HANDLE hDisk = CreateFileW(diskPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

    if (hDisk == INVALID_HANDLE_VALUE) {
        PrintLastError(L"Не удалось открыть диск");
        return 1;
    }

    // Проверяем, что диск достаточно велик
    if (!CheckDiskSize(hDisk, 3ULL * 1024 * 1024)) {
        CloseHandle(hDisk);
        return 1;
    }

    // Пишем MBR с одним разделом
    if (!InitializeDiskMBR(hDisk)) {
        wcerr << L"Не удалось инициализировать MBR-диск\n";
        CloseHandle(hDisk);
        return 1;
    }

    CloseHandle(hDisk);

    // Ждем, пока система пересканирует диск
    Sleep(3000);

    // Назначаем букву новому разделу
    wchar_t assignedLetter = 0;
    if (!AssignDriveLetter(diskNum, assignedLetter)) {
        wcerr << L"Не удалось назначить букву диска\n";
        return 1;
    }

    wcout << L"Буква диска: " << assignedLetter << L"\n";

    if (!FormatPartition(assignedLetter)) {
        wcerr << L"Не удалось отформатировать раздел\n";
        return 1;
    }

    wcout << L"Готово.\n";
    return 0;
}
