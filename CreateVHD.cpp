#include <Windows.h>           // Основной заголовок для Windows API
#include <Virtdisk.h>          // Заголовок для работы с виртуальными дисками
#include <iostream>            // Для вывода в консоль
#include <locale>              // Язык
#include <filesystem>          // Для работы с путями и файлами
#include <string>

#pragma comment(lib, "Ole32.lib")   
#pragma comment(lib, "VirtDisk.lib")

namespace fs = std::filesystem;       // Что бы не писать каждый раз std::filesystem

// GUID майкрософт для виртуальных дисков (VHDX)
static const GUID VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT = 
{ 0xEC984AEC, 0xA0F9, 0x47E9, {0x90, 0x1F, 0x71, 0x41, 0x5A, 0x66, 0x34, 0x5B} };

// проверка, существует ли, файл
bool FileExists(const wchar_t* path) {
    DWORD attrib = GetFileAttributesW(path);
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

// вывод ошибок Windows
void PrintError(DWORD errorCode) {
    LPWSTR message_buffer = nullptr;
    DWORD chars = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, errorCode, 0, (LPWSTR)&message_buffer, 0, nullptr
    );
    if (chars > 0) {
        std::wcerr << L"[ERROR 0x" << std::hex << errorCode << L"] " << message_buffer;
        LocalFree(message_buffer);
    } else {
        std::wcerr << L"Unknown error: 0x" << std::hex << errorCode;
    }
}

// Функция создания VHDX-диска
DWORD CreateDisk(PCWSTR virtualDiskFilePath, HANDLE* handle) {
    // Указываем, что создаем VHDX диск от майкрософт
    VIRTUAL_STORAGE_TYPE storageType = {
        VIRTUAL_STORAGE_TYPE_DEVICE_VHDX,
        VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT
    };

    // Ннастройки создаваемого виртуального диска
    CREATE_VIRTUAL_DISK_PARAMETERS parameters = {};
    parameters.Version = CREATE_VIRTUAL_DISK_VERSION_1; // Используем первую версию структуры
    parameters.Version1.MaximumSize = 3 * 1024ULL * 1024ULL;// Размер 3 МБ
    parameters.Version1.BlockSizeInBytes = CREATE_VIRTUAL_DISK_PARAMETERS_DEFAULT_BLOCK_SIZE; 
    parameters.Version1.SectorSizeInBytes = CREATE_VIRTUAL_DISK_PARAMETERS_DEFAULT_SECTOR_SIZE;

    // создание самого виртуального диска
    DWORD result = ::CreateVirtualDisk(
        &storageType, virtualDiskFilePath, VIRTUAL_DISK_ACCESS_CREATE,
  NULL, CREATE_VIRTUAL_DISK_FLAG_NONE, 0, &parameters, NULL, handle
    );

    // Если чтото пошло не так - покажем ошибку
    if (result != ERROR_SUCCESS) {
        std::wcerr << L"Ошибка создания диска: ";
    PrintError(result);
    }return result;
}

int wmain(int argc, wchar_t* argv[]) {
    // Настраиваем кодировку консоли под кириллицу
    SetConsoleOutputCP(1251);
    SetConsoleCP(1251);
    std::locale::global(std::locale("Russian"));  // Устанавливаем русскую локаль
    std::wcout.imbue(std::locale());              // Чтобы wcout работал с русским текстом

    // Проверка на корректное количество аргументов
    if (argc != 2) {    
        std::wcerr << L"Usage: CreateVHDX <path>\n";
        return 1;
    }

    // Получаем абсолютный путь до VHDX-файла
    fs::path vhdPath = fs::absolute(argv[1]);

    // Проверяем, что расширение файла - .vhdx
    if (vhdPath.extension() != L".vhdx") {
        std::wcerr << L"файл должен иметь расширение .vhdx\n";
        return 1;
    }

    // Если директории нет то сделаем её
    if (!fs::exists(vhdPath.parent_path())) {
        fs::create_directories(vhdPath.parent_path());
    }

    HANDLE hVhd = INVALID_HANDLE_VALUE;

    // Пытаемся создать диск
    DWORD ret = CreateDisk(vhdPath.c_str(), &hVhd);

    // проверяем результат
    if (ret == ERROR_SUCCESS) {
        std::wcout << L"virtual диск успешно создан\n";
        if (hVhd != INVALID_HANDLE_VALUE) {
            CloseHandle(hVhd);
        }
        std::wcout << L"Путь: " << vhdPath << L"\n";
    }
    
    return ret == ERROR_SUCCESS ? 0 : 1; // возвращаем код завершения
}
