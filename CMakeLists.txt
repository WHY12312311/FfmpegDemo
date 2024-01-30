cmake_minimum_required(VERSION 3.27)
project(DemoPlayer)

set(CMAKE_CXX_STANDARD 17)

add_executable(DemoPlayer main.cpp
        DemoDecode.h
        SDLPlayer.h)


# 找到所需要的库
find_package(PkgConfig REQUIRED)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(SWSCALE REQUIRED libswscale)
pkg_check_modules(SWRESAMPLE REQUIRED libswresample)
pkg_check_modules(SDL2 REQUIRED sdl2)

# 包含目录和链接库
include_directories(${AVCODEC_INCLUDE_DIRS} ${AVFORMAT_INCLUDE_DIRS} ${SWSCALE_INCLUDE_DIRS} ${SWRESAMPLE_INCLUDE_DIRS}
        %{AVUTIL_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})
target_link_libraries(DemoPlayer ${AVCODEC_LDFLAGS} ${AVFORMAT_LDFLAGS} ${SWSCALE_LDFLAGS} ${SWRESAMPLE_LDFLAGS}
        ${AVUTIL_LDFLAGS} ${SDL2_LDFLAGS})
