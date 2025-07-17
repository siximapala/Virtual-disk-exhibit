#include <Windows.h>     // Основной заголовок для WinAPI
#include <Virtdisk.h>    // Заголовок для работы с виртуальными дисками (VHD/VHDX)
#include <iostream>    
#include <iomanip>       // Для форматирования вывода
#include <sstream>       // Для создания строки GUID
#include <vector>

#pragma comment(lib, "Ole32.lib") 

// Функция для перевода структуры GUID в строку
std::wstring GuidToString(const GUID& guid) {
    std::wstringstream ss;
    ss << std::uppercase << std::hex << std::setfill(L'0')
       << std::setw(8) << guid.Data1 << L"-"
       << std::setw(4) << guid.Data2 << L"-"
       << std::setw(4) << guid.Data3 << L"-"
       << std::setw(2) << static_cast<int>(guid.Data4[0])
       << std::setw(2) << static_cast<int>(guid.Data4[1]) << L"-"
       << std::setw(2) << static_cast<int>(guid.Data4[2])
       << std::setw(2) << static_cast<int>(guid.Data4[3])
       << std::setw(2) << static_cast<int>(guid.Data4[4])
       << std::setw(2) << static_cast<int>(guid.Data4[5])
       << std::setw(2) << static_cast<int>(guid.Data4[6])
       << std::setw(2) << static_cast<int>(guid.Data4[7]);
    return ss.str();
}

int wmain(int argc, wchar_t* argv[]) {
    // Проверка количества аргументов (ожидается только путь до .vhdx файла)
    if (argc != 2) {
        std::wcerr << L"Usage: MontageVHD <path_to_vhdx>\n";
        return 1;
    }

    HANDLE vh = nullptr;

    // Открываем виртуальный диск (только для чтения)
    HRESULT hr = OpenVirtualDisk(
        nullptr, // Используем настройки по умолчанию
        argv[1], // Путь до .vhdx файла
        VIRTUAL_DISK_ACCESS_ATTACH_RO, 
        OPEN_VIRTUAL_DISK_FLAG_NONE,   // Без дополнительных флагов
        nullptr,                       // Нет параметров открытия
        &vh
    );

    if (FAILED(hr)) {
        std::wcerr << L"OpenVirtualDisk failed: 0x" << std::hex << hr << L"\n";
        return 1; // Не удалось открыть диск
    }

    // присоединяем виртуальный диск к системе
    hr = AttachVirtualDisk(
        vh,
        nullptr, // Без дополнительных параметров безопасности
        ATTACH_VIRTUAL_DISK_FLAG_PERMANENT_LIFETIME, // Диск останется подключён до выключения системы
        0,
        nullptr,
        nullptr
    );

    if (FAILED(hr)) {
        std::wcerr << L"AttachVirtualDisk failed: 0x" << std::hex << hr << L"\n";
        CloseHandle(vh);
        return 1; // Не удалось подключить диск
    }

    // Получаем информацию о виртуальном диске
    GET_VIRTUAL_DISK_INFO info = {};
    DWORD infoSize = sizeof(info);
    info.Version = GET_VIRTUAL_DISK_INFO_VIRTUAL_DISK_ID; // Запрашиваем конкретную информацию - ID диска

    hr = GetVirtualDiskInformation(vh, &infoSize, &info, nullptr);
    if (SUCCEEDED(hr)) {
        std::wcout << L"Disk ID: " << GuidToString(info.VirtualDiskId) << L"\n"; // Выводим GUID
    } else {
        std::wcerr << L"GetVirtualDiskInformation failed: 0x" << std::hex << hr << L"\n";
    }

    CloseHandle(vh);

    // Просто для наглядности выводим список всех дисков через PowerShell
    std::wcout << L"\nChecking disks via PowerShell...\n";
    system("powershell -Command \"Get-Disk | Format-Table Number, FriendlyName, Size\"");

    return 0;
}
