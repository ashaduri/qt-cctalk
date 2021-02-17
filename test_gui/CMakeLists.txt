
option(APP_BUILD_GUI_TEST "Build GUI test" ON)
if (NOT APP_BUILD_GUI_TEST)
    set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL true)
endif()


# Source files
set(gui_SOURCES
	app_settings.cpp
	app_settings.h
	cctalk_tools.h
	gui_application.cpp
	gui_application.h
	gui_main.cpp
	logging_tools.h
	main_tools.h
	main_window.h
	main_window.cpp
	main_window.ui
)

add_executable(test_gui ${gui_SOURCES})

target_link_libraries(test_gui
	PRIVATE
		compiler_warnings
		cctalk
		cctalk_helpers
		Qt5::Widgets
		# Qt5::Gui
)

target_include_directories(
	test_gui
		PRIVATE
			${CMAKE_SOURCE_DIR}
)

