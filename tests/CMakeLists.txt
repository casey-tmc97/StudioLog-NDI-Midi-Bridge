find_package(Qt6 REQUIRED COMPONENTS Test Core)

# ── Shared source files used by multiple tests ────────────────────────────────
set(LTC_SOURCES
    "${CMAKE_SOURCE_DIR}/src/ltc/FrameValidator.cpp"
    "${CMAKE_SOURCE_DIR}/src/ltc/FrameRateDetector.cpp"
)

set(TEST_INCLUDE_DIRS
    "${CMAKE_SOURCE_DIR}/src"
)

# ── test_mtc_timing ───────────────────────────────────────────────────────────
# Tests MTCTypes.h and SMPTETimecode::advanceByTwo() — header-only, no extra sources needed
add_executable(test_mtc_timing test_mtc_timing.cpp)
target_include_directories(test_mtc_timing PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_mtc_timing PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_mtc_timing COMMAND test_mtc_timing)

# ── test_ltc_decoder ──────────────────────────────────────────────────────────
# Tests AudioRingBuffer (header-only) and FrameRateDetector
add_executable(test_ltc_decoder
    test_ltc_decoder.cpp
    ${LTC_SOURCES}
)
target_include_directories(test_ltc_decoder PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_ltc_decoder PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_ltc_decoder COMMAND test_ltc_decoder)

# ── test_frame_validator ──────────────────────────────────────────────────────
# Tests FrameValidator and MTCTypes helpers
add_executable(test_frame_validator
    test_frame_validator.cpp
    ${LTC_SOURCES}
)
target_include_directories(test_frame_validator PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_frame_validator PRIVATE Qt6::Core Qt6::Test)
add_test(NAME test_frame_validator COMMAND test_frame_validator)
