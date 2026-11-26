cmake_minimum_required(VERSION 3.20)
project(ClassicDefenderUI VERSION 1.1.0 LANGUAGES CXX RC)

if(NOT WIN32 AND NOT MINGW)
    message(FATAL_ERROR "ClassicDefenderUI targets Windows only (MSVC or MinGW-w64).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Single source of truth for the version: project(... VERSION x.y.z) above.
configure_file(src/Version.h.in ${CMAKE_BINARY_DIR}/generated/Version.h @ONLY)
configure_file(cmake/version.rc.in ${CMAKE_BINARY_DIR}/generated/version.rc @ONLY)

set(APP_SOURCES
    src/main.cpp
    src/AppTheme.cpp
    src/Settings.cpp
    src/ScanScope.cpp
    src/DefenderWmi.cpp
    src/ui/Paint.cpp
    src/ui/MainWindow.cpp
    src/ui/OptionsDialog.cpp
    src/ui/ReportsView.cpp
    src/ui/ProgressWindow.cpp
    src/ui/FolderPicker.cpp
    ${CMAKE_BINARY_DIR}/generated/version.rc
    src/resources.rc)

add_executable(ClassicDefenderUI WIN32 ${APP_SOURCES})

target_include_directories(ClassicDefenderUI PRIVATE src ${CMAKE_BINARY_DIR}/generated)

target_compile_definitions(ClassicDefenderUI PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00)

# wbemuuid supplies CLSID_WbemLocator / IID_IWbemLocator, oleaut32 the VARIANT helpers.
target_link_libraries(ClassicDefenderUI PRIVATE
    user32
    gdi32
    comctl32
    comdlg32
    ole32
    oleaut32
    uuid
    wbemuuid
    shell32
    shlwapi
    advapi32)

if(MSVC)
    target_compile_options(ClassicDefenderUI PRIVATE /W4 /permissive- /utf-8)
    # UAC elevation so the WMI provider returns real data instead of WBEM_E_ACCESS_DENIED.
    target_link_options(ClassicDefenderUI PRIVATE
        "/MANIFEST:EMBED"
        "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
else()
    target_compile_options(ClassicDefenderUI PRIVATE -Wall -Wextra)
    # -municode selects wWinMainCRTStartup, the wide entry point MSVC uses by
    # default; -static removes the libstdc++/libgcc DLL dependency.
    target_link_options(ClassicDefenderUI PRIVATE -municode -static)
    # MinGW has no /MANIFESTUAC: the manifest is compiled in as resource 1/24.
    target_sources(ClassicDefenderUI PRIVATE src/manifest.rc)
endif()

option(BUILD_TESTING_TOOLS "Build the console test harnesses" OFF)
if(BUILD_TESTING_TOOLS)
    enable_testing()
    add_executable(theme_test tools/theme_test.cpp src/AppTheme.cpp src/Settings.cpp)
    target_include_directories(theme_test PRIVATE src)
    target_compile_definitions(theme_test PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
    target_link_libraries(theme_test PRIVATE advapi32)
    if(NOT MSVC)
        target_link_options(theme_test PRIVATE -static)
    endif()
    add_test(NAME theme_test COMMAND theme_test)

    add_executable(scope_test tools/scope_test.cpp src/ScanScope.cpp)
    target_include_directories(scope_test PRIVATE src)
    target_compile_definitions(scope_test PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
    if(NOT MSVC)
        target_link_options(scope_test PRIVATE -static)
    endif()
    add_test(NAME scope_test COMMAND scope_test)
endif()
