

# Source files
set(cctalk_SOURCES
	bill_validator_device.h
	cctalk_device.cpp
	cctalk_device.h
	cctalk_link_controller.cpp
	cctalk_link_controller.h
	coin_acceptor_device.h
	serial_worker.cpp
	serial_worker.h
)

add_library(cctalk STATIC ${cctalk_SOURCES})

target_link_libraries(cctalk
	PUBLIC
		Qt5::Concurrent
		Qt5::SerialPort
	PRIVATE
		compiler_warnings
)


add_subdirectory(helpers)
