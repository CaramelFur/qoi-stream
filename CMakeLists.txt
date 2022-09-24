project(qoi-stream
  LANGUAGES C
)

cmake_minimum_required(VERSION 3.5.1)

set(CMAKE_C_COMPILER gcc)

add_definitions("-Wall" "-Wextra" "-Wconversion" "-Wpedantic" "-O3" "-std=c99")

include_directories(${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include)
file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.c)

add_executable(${PROJECT_NAME} ${SRC_FILES})
