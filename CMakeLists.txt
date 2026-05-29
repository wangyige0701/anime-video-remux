cmake_minimum_required(VERSION 3.15)
project(remux LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FFMPEG_ROOT "D:/libs/ffmpeg/ffmpeg-7.1.1-full_build-shared")

include_directories("${FFMPEG_ROOT}/include")
link_directories("${FFMPEG_ROOT}/lib")

# ==========
# Node addon 头文件路径
# ==========
execute_process(
    COMMAND node -p "require('node-addon-api').include"
    OUTPUT_VARIABLE NODE_ADDON_API_DIR
)

# 去掉换行
string(REPLACE "\n" "" NODE_ADDON_API_DIR "${NODE_ADDON_API_DIR}")
# 去掉引号
string(REPLACE "\"" "" NODE_ADDON_API_DIR "${NODE_ADDON_API_DIR}")

message(STATUS "node-addon-api include: ${NODE_ADDON_API_DIR}")

add_definitions(-DNAPI_VERSION=8)

# =========
# 引入需要的库
# =========
add_library(remux SHARED
    src/Remux.cpp
    src/JsHttpResponse.cpp
    src/remux_napi.cpp
)

# Node addon 后缀 & 前缀
set_target_properties(remux PROPERTIES
    PREFIX ""
    SUFFIX ".node"
)

target_include_directories(remux PRIVATE
    ${NODE_ADDON_API_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/node
)

# libnode.dll.a 是从 node.exe 编译而来
target_link_libraries(remux
    avformat
    avcodec
    avutil
    swscale
    swresample
    ${CMAKE_CURRENT_SOURCE_DIR}/libnode.dll.a
)

# =========================
# Copy FFmpeg DLLs
# =========================
add_custom_command(TARGET remux POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${FFMPEG_ROOT}/bin"
    $<TARGET_FILE_DIR:remux>
)

