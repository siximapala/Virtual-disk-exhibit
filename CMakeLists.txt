cmake_minimum_required(VERSION 3.15)
project(VHDX_Tools LANGUAGES CXX)

# Сборка по умолчанию (Ninja или Make) и единая папка для всех exe
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/release)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Линковка системных библиотек напрямую (избегаем find_library NOTFOUND)
set(VIRTDISK_LIB VirtDisk)
set(OLE32_LIB ole32)

# Создание VHDX
add_executable(CreateVHD CreateVHD.cpp)
target_link_libraries(CreateVHD PRIVATE ${VIRTDISK_LIB} ${OLE32_LIB})

# Монтирование VHDX (переименовано)
add_executable(MontageVHD MontageVHD.cpp)
target_link_libraries(MontageVHD PRIVATE ${VIRTDISK_LIB} ${OLE32_LIB})

# Форматирование и запись тестового файла
add_executable(FormatAndWrite FormatAndWrite.cpp)
target_link_libraries(FormatAndWrite PRIVATE ${OLE32_LIB})

# Для Windows: задать _WIN32_WINNT
if (WIN32)
  foreach(target CreateVHD MontageVHD FormatAndWrite)
    target_compile_definitions(${target} PRIVATE _WIN32_WINNT=0x0A00)
  endforeach()
endif()