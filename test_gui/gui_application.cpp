/**************************************************************************
Copyright: (C) 2014 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#include <iostream>  // cerr
#include <cstdlib>  // std::abort
#include <exception>  // std::exception

#include <QtGlobal>
#include <QDir>
#include <QVector>
#include <QApplication>
#include <QWindow>

#include "gui_application.h"
#include "main_window.h"
#include "app_settings.h"
#include "cctalk/helpers/debug_qt_bridge.h"
#include "cctalk/helpers/debug.h"



GuiApplication::GuiApplication(int& par_argc, char**& par_argv)
		: QApplication(par_argc, par_argv)
{
	// Needed for settings
	QCoreApplication::setOrganizationName(QStringLiteral("Qt-ccTalk"));
	QCoreApplication::setApplicationName(QStringLiteral("Qt-ccTalk GUI"));

	QApplication::setQuitOnLastWindowClosed(false);  // better manage it explicitly

	QObject::connect(this, &QApplication::aboutToQuit, this, &GuiApplication::quitCleanup);
}



GuiApplication::~GuiApplication() = default;



int GuiApplication::run()
{
	// Set levels that will abort the program if printing to them.
	debug_set_abort_on_levels(debug_level::fatal);

	// Set default destinations for specified levels.
	debug_set_default_dests(debug_level::all, DEBUG_CONSOLE);

	// Output to cerr, to avoid buffering.
	debug_set_console_stream(&std::cerr);

	// Set the default application name to send along the message.
	debug_set_application_name("qt-cctalk_gui");

	// Use libdebug for Qt's messages.
	qInstallMessageHandler(debug_qt5_message_handler);

	debug_out_info(DBG_FUNC_MSG << "GUI test starting...");

	// Useful if the resources cannot be found
	debug_out_dump("Current directory is \"" << QDir::current().path() << "\".");

	// Load application settings
	AppSettings::init();

	// The main window, construct and show it.
	main_window_.reset(new MainWindow());
	main_window_->show();

	// The Main Loop
	debug_out_info("Entering main loop.");

	int status = QApplication::exec();

	if (status == 0) {  // don't print if it's an error, because the application may be in unstable state
		debug_out_info("Main loop exited.");
	}

	// Don't put cleanup code here, windows is silly, we may not even leave the exec() call.
	// Use quitCleanup() instead.

	return status;
}



void GuiApplication::quit()
{
	// Exit the main loop
	debug_out_dump(DBG_FUNC_MSG << "Trying to exit the main loop...");

	QCoreApplication::exit(0);

	// Don't destroy windows here - we may be in one of their callbacks.

	// No cleanup here - use quitCleanup() instead.
}



void GuiApplication::quitCleanup()
{
	// Put all the post-main-loop stuff here.
	// No interactive stuff allowed.

	// delete main window
	main_window_.reset();

	// Write unwritten settings
	AppSettings::sync();
}







/// @}
