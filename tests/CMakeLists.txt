find_package(Qt6 REQUIRED COMPONENTS Test Core)

# ── Shared source files used by multiple tests ────────────────────────────────
set(LTC_SOURCES
    "${CMAKE_SOURCE_DIR}/src/ltc/FrameValidator.cpp"
    "${CMAKE_SOURCE_DIR}/src/ltc/FrameRateDetector.cpp"
)

set(TEST_INCLUDE_DIRS
    "${CMAKE_SOURCE_DIR}/src"
)

# ── Helper: copy Qt6Test(d).dll next to each test executable on Windows ───────
# windeployqt only runs for the main app target and does not cover test
# executables, so Qt6Test.dll (the QtTest runtime) must be copied manually.
# $<TARGET_FILE:Qt6::Test> resolves to Qt6Testd.dll in Debug and Qt6Test.dll
# in Release, matching the configuration automatically.
function(deploy_qt_test_dll target)
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:Qt6::Test>
                $<TARGET_FILE_DIR:${target}>
            COMMENT "Copying Qt6Test DLL for ${target}"
        )
    endif()
endfunction()

# ── test_mtc_timing ───────────────────────────────────────────────────────────
# Tests MTCTypes.h and SMPTETimecode::advanceByTwo() — header-only, no extra sources needed
add_executable(test_mtc_timing test_mtc_timing.cpp)
target_include_directories(test_mtc_timing PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_mtc_timing PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_mtc_timing COMMAND test_mtc_timing)
deploy_qt_test_dll(test_mtc_timing)

# ── test_ltc_decoder ──────────────────────────────────────────────────────────
# Tests AudioRingBuffer (header-only) and FrameRateDetector
add_executable(test_ltc_decoder
    test_ltc_decoder.cpp
    ${LTC_SOURCES}
)
target_include_directories(test_ltc_decoder PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_ltc_decoder PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_ltc_decoder COMMAND test_ltc_decoder)
deploy_qt_test_dll(test_ltc_decoder)

# ── test_frame_validator ──────────────────────────────────────────────────────
# Tests FrameValidator and MTCTypes helpers
add_executable(test_frame_validator
    test_frame_validator.cpp
    ${LTC_SOURCES}
)
target_include_directories(test_frame_validator PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_frame_validator PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_frame_validator COMMAND test_frame_validator)
deploy_qt_test_dll(test_frame_validator)
