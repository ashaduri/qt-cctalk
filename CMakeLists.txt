
cmake_minimum_required(VERSION 3.1)

# set the project name
project(qt-cctalk VERSION 0.1.0)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(CMAKE_VERSION VERSION_LESS "3.7.0")
	set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

find_package(Qt5 COMPONENTS Core Concurrent SerialPort Gui Widgets REQUIRED)


# Enable warnings
add_library(compiler_warnings INTERFACE)
target_compile_options(compiler_warnings INTERFACE
	$<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
		-Wall -Wextra>
	$<$<CXX_COMPILER_ID:MSVC>:
		/W4>
)


add_subdirectory(cctalk)
add_subdirectory(test_gui)

