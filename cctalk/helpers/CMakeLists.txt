

# Source files
set(cctalk_helpers_SOURCES
	async_serializer.cpp
	async_serializer.h
	debug.cpp
	debug.h
	debug_qt_bridge.h
)

add_library(cctalk_helpers STATIC ${cctalk_helpers_SOURCES})

target_link_libraries(cctalk_helpers
	PUBLIC
		Qt5::Core
	PRIVATE
		compiler_warnings
)

