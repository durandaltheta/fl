project(fl_ut)

add_subdirectory(googletest)

add_executable(fl_ut
    test.cpp
)

target_link_libraries(fl_ut gtest gtest_main)
