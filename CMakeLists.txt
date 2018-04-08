cmake_minimum_required(VERSION 3.9)
project(structured_bn)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 99)
include_directories(src lib/psdd lib/psdd/lib/googletest/googletest/include lib/psdd/lib/googletest/googlemock/include lib/psdd/lib/libsdd/ lib/psdd/lib/json/include/nlohmann lib/psdd/lib/json/include)
add_subdirectory(lib/psdd)

file(GLOB PSDD_TEST_SRC_FILES "test/*.cpp")
file(GLOB LIB_PSDD_SRC_FILES "src/*.h" "src/*.cpp" "src/*.c")
add_executable(structured_bn_test ${PSDD_TEST_SRC_FILES})
add_library(structured_bn ${LIB_PSDD_SRC_FILES})
target_link_libraries(structured_bn_test structured_bn psdd  gtest gtest_main psdd sdd nlohmann_json)
target_link_libraries(structured_bn psdd sdd nlohmann_json)
