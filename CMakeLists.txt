cmake_minimum_required(VERSION 3.20)

# Static CRT selection through CMAKE_MSVC_RUNTIME_LIBRARY.
cmake_policy(SET CMP0091 NEW)

project(SnipPin
    VERSION 1.0.0
    DESCRIPTION "Pin windows and live snippets always on top (Windows 7 - 11)"
    HOMEPAGE_URL "https://github.com/rafsbo/SnipPin"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    set(_snippin_default_app ON)
else()
    set(_snippin_default_app OFF)
endif()

option(SNIPPIN_BUILD_APP "Build the Windows application" ${_snippin_default_app})
option(SNIPPIN_BUILD_UIACCESS "Also build SnipPinUIA.exe (uiAccess manifest, must be signed)" ON)
option(SNIPPIN_BUILD_TESTS "Build the unit tests for the platform-independent logic" ON)

# ---------------------------------------------------------------- compiler setup
if(MSVC)
    # Static CRT: the exe runs on a clean Windows 7 SP1 without redistributables.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

function(snippin_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus /EHsc)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
    endif()
endfunction()

# ---------------------------------------------------------------- portable logic
# No Windows headers: unit-testable on any platform.
add_library(snippin_logic STATIC
    src/logic/Geometry.cpp
    src/logic/Geometry.h
    src/logic/Hotkey.cpp
    src/logic/Hotkey.h
    src/logic/Identity.cpp
    src/logic/Identity.h)
target_include_directories(snippin_logic PUBLIC src)
snippin_set_warnings(snippin_logic)

# ---------------------------------------------------------------- application
if(SNIPPIN_BUILD_APP)
    if(NOT WIN32)
        message(FATAL_ERROR "SNIPPIN_BUILD_APP requires a Windows target (MSVC or MinGW-w64).")
    endif()
    enable_language(RC)

    configure_file(res/Version.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h @ONLY)

    set(SNIPPIN_APP_SOURCES
        src/main.cpp
        src/app/App.cpp
        src/app/App.h
        src/app/HotkeyManager.cpp
        src/app/HotkeyManager.h
        src/app/MouseHook.cpp
        src/app/MouseHook.h
        src/app/Strings.h
        src/app/TrayIcon.cpp
        src/app/TrayIcon.h
        src/core/RecoveryJournal.cpp
        src/core/RecoveryJournal.h
        src/core/StashService.cpp
        src/core/StashService.h
        src/core/WindowTracker.cpp
        src/core/WindowTracker.h
        src/core/ZOrderGuard.cpp
        src/core/ZOrderGuard.h
        src/platform/ApiShim.cpp
        src/platform/ApiShim.h
        src/platform/Log.cpp
        src/platform/Log.h
        src/platform/Settings.cpp
        src/platform/Settings.h
        src/platform/Win32.h
        src/platform/WinUtil.cpp
        src/platform/WinUtil.h
        src/snippet/RegionSelector.cpp
        src/snippet/RegionSelector.h
        src/snippet/SnippetManager.cpp
        src/snippet/SnippetManager.h
        src/snippet/SnippetWindow.cpp
        src/snippet/SnippetWindow.h
        src/ui/SettingsDialog.cpp
        src/ui/SettingsDialog.h
        src/ui/StashPanel.cpp
        src/ui/StashPanel.h
        src/ui/WindowPicker.cpp
        src/ui/WindowPicker.h)

    # Compiled once, linked into both executables.
    add_library(snippin_app_objects OBJECT ${SNIPPIN_APP_SOURCES})
    target_include_directories(snippin_app_objects PUBLIC
        src
        res
        ${CMAKE_CURRENT_BINARY_DIR}/generated)
    target_compile_definitions(snippin_app_objects PUBLIC
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
        WINVER=0x0601 _WIN32_WINNT=0x0601)
    target_link_libraries(snippin_app_objects PUBLIC snippin_logic)
    snippin_set_warnings(snippin_app_objects)

    set(SNIPPIN_SYSTEM_LIBS user32 gdi32 comctl32 dwmapi shell32 shlwapi ole32 oleaut32 advapi32 uuid)

    function(snippin_add_executable target uiaccess)
        add_executable(${target} WIN32 $<TARGET_OBJECTS:snippin_app_objects> res/SnipPin.rc)
        target_link_libraries(${target} PRIVATE snippin_logic ${SNIPPIN_SYSTEM_LIBS})
        target_include_directories(${target} PRIVATE res ${CMAKE_CURRENT_BINARY_DIR}/generated)
        if(uiaccess)
            # Applies to SnipPin.rc of this target only (selects the uiAccess manifest).
            target_compile_definitions(${target} PRIVATE SNIPPIN_UIACCESS)
        endif()
        if(MSVC)
            # The manifest is embedded by SnipPin.rc; keep the linker from adding a second one.
            target_link_options(${target} PRIVATE /MANIFEST:NO "/SUBSYSTEM:WINDOWS,6.01")
        else()
            target_link_options(${target} PRIVATE -municode -static -static-libgcc -static-libstdc++
                $<$<CONFIG:Release,MinSizeRel>:-s>)
        endif()
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
            RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug
            RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin/RelWithDebInfo
            RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/bin/MinSizeRel)
    endfunction()

    snippin_add_executable(SnipPin OFF)
    if(SNIPPIN_BUILD_UIACCESS)
        snippin_add_executable(SnipPinUIA ON)
    endif()

    install(TARGETS SnipPin RUNTIME DESTINATION .)
    if(SNIPPIN_BUILD_UIACCESS)
        install(TARGETS SnipPinUIA RUNTIME DESTINATION .)
    endif()
    install(FILES README.md LICENSE DESTINATION .)
endif()

# ---------------------------------------------------------------- tests
if(SNIPPIN_BUILD_TESTS)
    enable_testing()
    add_executable(snippin_tests
        tests/TestMain.cpp
        tests/TestHarness.h
        tests/GeometryTests.cpp
        tests/HotkeyTests.cpp
        tests/IdentityTests.cpp)
    target_link_libraries(snippin_tests PRIVATE snippin_logic)
    target_include_directories(snippin_tests PRIVATE tests)
    snippin_set_warnings(snippin_tests)
    add_test(NAME snippin_tests COMMAND snippin_tests)
endif()
