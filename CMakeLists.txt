cmake_minimum_required(VERSION 3.25)
project(StudioLogNDIMIDIBridge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/src/ui")

# ── Qt 6 ─────────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# ── RtMidi (via vcpkg) ───────────────────────────────────────────────────────
find_package(rtmidi CONFIG REQUIRED)

# ── libltc (git submodule) ───────────────────────────────────────────────────
add_subdirectory(third_party/libltc EXCLUDE_FROM_ALL)

# ── Vizrt NDI Advanced SDK ───────────────────────────────────────────────────
set(NDI_SDK_DIR "${CMAKE_SOURCE_DIR}/third_party/NDI"
    CACHE PATH "Path to the Vizrt NDI Advanced SDK root directory")

if(NOT EXISTS "${NDI_SDK_DIR}/include/Processing.NDI.Lib.h")
    message(FATAL_ERROR
        "NDI SDK not found at ${NDI_SDK_DIR}.\n"
        "Download the NDI SDK from https://ndi.video/for-developers/ndi-sdk/ "
        "and unzip it to third_party/NDI/ so that "
        "third_party/NDI/include/Processing.NDI.Lib.h exists.\n"
        "Override with -DNDI_SDK_DIR=<path> if installed elsewhere.")
endif()

# ── Sources ──────────────────────────────────────────────────────────────────
set(APP_SOURCES
    src/main.cpp
    src/app/Application.h      src/app/Application.cpp
    src/app/AppState.h         src/app/AppState.cpp
    src/app/Settings.h         src/app/Settings.cpp
    src/ndi/NDIDiscovery.h     src/ndi/NDIDiscovery.cpp
    src/ndi/NDIReceiver.h      src/ndi/NDIReceiver.cpp
    src/ndi/AudioRingBuffer.h
    src/ltc/LTCDecoder.h       src/ltc/LTCDecoder.cpp
    src/ltc/FrameValidator.h   src/ltc/FrameValidator.cpp
    src/ltc/FrameRateDetector.h src/ltc/FrameRateDetector.cpp
    src/midi/MIDIOutput.h      src/midi/MIDIOutput.cpp
    src/midi/MTCGenerator.h    src/midi/MTCGenerator.cpp
    src/midi/MTCTypes.h
    src/ui/MainWindow.h        src/ui/MainWindow.cpp  src/ui/MainWindow.ui
    src/ui/TrayIcon.h          src/ui/TrayIcon.cpp
    src/ui/SetupPanel.h        src/ui/SetupPanel.cpp
    src/util/HighResTimer.h    src/util/HighResTimer.cpp
    src/util/Logger.h          src/util/Logger.cpp
    src/util/PlatformInit.h    src/util/PlatformInit.cpp
)

set(APP_RESOURCES resources/studiolog-ndi-midi-bridge.qrc)

add_executable(StudioLogNDIMIDIBridge WIN32 MACOSX_BUNDLE
    ${APP_SOURCES}
    ${APP_RESOURCES}
)

target_include_directories(StudioLogNDIMIDIBridge PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${NDI_SDK_DIR}/include"
)

target_link_libraries(StudioLogNDIMIDIBridge PRIVATE
    Qt6::Core
    Qt6::Widgets
    RtMidi::rtmidi
    ltc
)

# ── Platform-specific NDI linkage ─────────────────────────────────────────────
if(WIN32)
    # Standard NDI SDK uses capitalized Lib\ and Bin\ on Windows
    target_link_libraries(StudioLogNDIMIDIBridge PRIVATE
        "${NDI_SDK_DIR}/Lib/x64/Processing.NDI.Lib.x64.lib"
        winmm   # timeBeginPeriod / timeEndPeriod
    )
    target_compile_definitions(StudioLogNDIMIDIBridge PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
    )
    # Copy NDI runtime DLL next to the executable
    add_custom_command(TARGET StudioLogNDIMIDIBridge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${NDI_SDK_DIR}/Bin/x64/Processing.NDI.Lib.x64.dll"
            "$<TARGET_FILE_DIR:StudioLogNDIMIDIBridge>"
        COMMENT "Copying NDI runtime DLL"
    )
    # Deploy Qt DLLs (debug or release) so the exe is self-contained in the build dir.
    # Qt6::windeployqt is available via find_package(Qt6) — uses the right Qt installation.
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "${Qt6_DIR}/../../../bin"
        NO_DEFAULT_PATH)
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET StudioLogNDIMIDIBridge POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                $<IF:$<CONFIG:Debug>,--debug,--release>
                --no-translations
                "$<TARGET_FILE:StudioLogNDIMIDIBridge>"
            COMMENT "Deploying Qt runtime DLLs"
        )
    endif()
elseif(APPLE)
    target_link_libraries(StudioLogNDIMIDIBridge PRIVATE
        "${NDI_SDK_DIR}/lib/macOS/libndi.dylib"
        "-framework CoreMIDI"
        "-framework CoreFoundation"
        "-framework CoreAudio"
    )
    set_target_properties(StudioLogNDIMIDIBridge PROPERTIES
        MACOSX_BUNDLE_BUNDLE_IDENTIFIER "org.texasmusiccafe.studiolog-ndi-midi-bridge"
        MACOSX_BUNDLE_BUNDLE_VERSION    ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_GUI_IDENTIFIER    "org.texasmusiccafe.studiolog-ndi-midi-bridge"
    )
endif()

# ── CPack packaging ───────────────────────────────────────────────────────────
set(CPACK_PACKAGE_NAME             "StudioLog NDI MIDI Bridge")
set(CPACK_PACKAGE_VENDOR           "Texas Music Cafe")
set(CPACK_PACKAGE_VERSION          ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "NDI LTC → MTC bridge for live broadcast")
set(CPACK_RESOURCE_FILE_LICENSE    "${CMAKE_SOURCE_DIR}/LICENSE")

if(WIN32)
    set(CPACK_GENERATOR NSIS)
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "StudioLogNDIMIDIBridge.exe")
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        "ExecWait '\\\"$INSTDIR\\\\StudioLogNDIMIDIBridge.exe\\\" --check-loopmidi'")
elseif(APPLE)
    set(CPACK_GENERATOR DragNDrop)
endif()

include(CPack)

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests)
