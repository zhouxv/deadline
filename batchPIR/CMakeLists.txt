cmake_minimum_required(VERSION 3.0)

project(VectorizedBatchPIR)

# Set the build directory
# 设置构建目录
# set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
# set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

include_directories(header)

# Set the output directory
# 设置输出目录
# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Get all source files under src
# 获取src目录下的所有源文件
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")

# Create executable target and add source files
# 创建可执行目标并添加源文件
add_executable(vectorized_batch_pir ${SOURCES})

# Set include directories
# 设置包含目录
target_include_directories(vectorized_batch_pir PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Set C++ standard
# 设置C++标准
set_target_properties(vectorized_batch_pir PROPERTIES CXX_STANDARD 11)

# Add seal library
# 添加seal库
find_package(SEAL 4.1 REQUIRED)
target_link_libraries(vectorized_batch_pir SEAL::seal)

# Add compiler flags for optimization
# 添加优化的编译器标志
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(vectorized_batch_pir PRIVATE "-O3")
endif()