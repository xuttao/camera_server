cmake_minimum_required(VERSION 3.5)
project("camera_server")

SET(CMAKE_CXX_STANDARD 14)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_LINUX "build on linux platform" ON)
#编译笔记本地面卡模式
option(BUILD_DESKTOP "build desktop platform" OFF)
#编译双v7模式
option(BUILD_DOUBLE_V7 "build double v7 platform" ON)

# add_definitions(-D_RTP)

# add_definitions(-D_UAVLIB)

add_definitions(-D__RTP_NO_CRYPTO__)

# include_directories(/media/xtt/hdd/code_open/uvgRTP/install/backup/include)
# link_libraries(/media/xtt/hdd/code_open/uvgRTP/install/backup/lib/libuvgrtp.a)

include_directories(3rd/uvgRTP/install/include)
link_directories(3rd/uvgRTP/install/lib)

# include_directories(3rd/UAVLIB/include)
# link_directories(3rd/UAVLIB/lib)

if(BUILD_LINUX)
    add_definitions(-D_UNIX)
else()
    add_definitions(-D_CONSOLE)
endif()

if(BUILD_DESKTOP)
	add_definitions(-D_DESKTOP)
elseif(BUILD_DOUBLE_V7)
	add_definitions(-D_DOUBLE_V7)
endif()

message(STATUS "Current CXX standard is ${CMAKE_CXX_STANDARD}")

SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb -std=c++14 -fPIC")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall -std=c++14 -fPIC")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -O0 -Wall -g -ggdb -std=c11 -fPIC")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O3 -Wall -std=c11 -fPIC")

message(STATUS "CMAKE_C_FLAGS_DEBUG is ${CMAKE_C_FLAGS_DEBUG}")
message(STATUS "CMAKE_C_FLAGS_RELEASE is ${CMAKE_C_FLAGS_RELEASE}")
message(STATUS "CMAKE_CXX_FLAGS_DEBUG is ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_CXX_FLAGS_RELEASE is ${CMAKE_CXX_FLAGS_RELEASE}")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rd/ffmpeg4.4/include)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rd/ffmpeg4.4/lib)

if(BUILD_LINUX)
	set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/opencv4.5/lib/cmake")
else()
	set(CMAKE_PREFIX_PATH "C:/3rd/opencv4.5/build/x64/vc15/lib" "C:/3rd/Qt5.12.9/5.12.9/msvc2017_64/lib/cmake")
endif()

message(STATUS "Looking for Opencv...")
find_package(OpenCV REQUIRED)
if(${OpenCV_FOUND})
        message(STATUS "Found Opencv" ${OpenCV_VERSION})
        include_directories(${OpenCV_INCLUDE_DIRS})
        link_libraries(${OpenCV_LIBS})
else()
        message(STATUS "Couldn't find Opencv")
endif()

# message(STATUS "Looking for Qt...")
# find_package(Qt5 REQUIRED Widgets Core Gui Test) #REQUIRED表示一定要找到包，否则停止cmake
# if (${Qt5_FOUND})
#     include_directories(${Qt5_INCLUDE_DIRS})
#     # link_libraries(${Qt5_LIBS})
#     message(STATUS "Found Qt " ${Qt5_VERSION})
# else()
#     message(STATUS "Couldn't find Qt")
# endif()

# link_libraries(
#     Qt5::Core
#     Qt5::Test 
#     Qt5::Gui 
#     Qt5::Widgets
# )
# if(BUILD_DESKTOP)
# 	qt5_wrap_ui(ui_list camera_demo.ui)
# elseif(BUILD_DOUBLE_V7)
# 	qt5_wrap_ui(ui_list camera_demo_2v7.ui)
# endif()
# qt5_wrap_cpp(moc_list camera_demo.h)
# qt5_add_resources(qrc_list camera_demo.qrc)

include_directories(./)
aux_source_directory(./ src_list)

if(BUILD_LINUX)
	link_libraries(pthread)
	#include_directories(/media/xtt/hdd/ca/xdma_code/xdma_fpga/src)
	#link_directories(/media/xtt/hdd/ca/xdma_code/xdma_fpga/build)
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../xdma_fpga/src)
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../xdma_fpga/build)
else()
	include_directories(E:/code_sync/xdma_fpga/src)
	link_directories(E:/code_sync/xdma_fpga/build/Debug)
endif()

if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
    add_executable(${PROJECT_NAME}d ${src_list})
    target_link_libraries(${PROJECT_NAME}d fpgacontrol_xdmad avcodec avformat avutil swresample swscale libuvgrtp.a)
    message(STATUS "Debug Model")
else()
    add_executable(${PROJECT_NAME} ${src_list})
	target_link_libraries(${PROJECT_NAME} fpgacontrol_xdma avcodec avformat avutil swresample swscale libuvgrtp.a)
    # target_link_libraries(${PROJECT_NAME} fpgacontrol_xdma UAV_MOT_LIB avcodec avformat avutil swresample swscale libuvgrtp.a)
    message(STATUS "Release Model")
endif()
