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
// #include "version.h"



namespace {

	/// Application instance
	GuiApplication* s_application_instance = 0;

}



class GuiApplicationImpl : public QApplication {
	public:

		/// Constructor
		GuiApplicationImpl(int& argc, char**& argv)
				: QApplication(argc, argv)
		{ }

		/// Virtual destructor
		virtual ~GuiApplicationImpl()
		{ }

		/// This adds exception handling to QCoreApplication::notify().
		virtual bool notify(QObject* receiver, QEvent* ev) override
		{
			// Qt doesn't support throwing exceptions from signals handlers,
			// so we have to catch them here. Catching them allows at least
			// some stack unwinding.
			try {
				return QApplication::notify(receiver, ev);
			}
			// unhandled exception from event handler
			catch(std::exception& e) {
				// don't use anything other than cerr here, it's the most safe option.
				std::cerr << "GuiApplicationImpl::notify(): Unhandled exception: " << e.what() << std::endl;
			#if (defined DEBUG_BUILD && DEBUG_BUILD)  // Debug build
				std::abort();  // useful for core dumps and backtraces
			#else
				QApplication::exit(1);  // further unwind
			#endif
				return false;
			}
			// don't let it fall through, else qt will trap it
			catch(...) {
				std::cerr << "GuiApplicationImpl::notify(): Unknown unhandled exception." << std::endl;
			#if (defined DEBUG_BUILD && DEBUG_BUILD)  // Debug build
				std::abort();  // useful for core dumps and backtraces
			#else
				QApplication::exit(1);  // further unwind
			#endif
				return false;
			}
		}

};





GuiApplication::GuiApplication(int& par_argc, char**& par_argv)
{
	s_application_instance = this;

	auto app = new GuiApplicationImpl(par_argc, par_argv);

	// Needed for settings
	QCoreApplication::setOrganizationName(QStringLiteral("Qt-ccTalk"));
	QCoreApplication::setApplicationName(QStringLiteral("Qt-ccTalk GUI"));

	qapp_ = app;
	qapp_->setQuitOnLastWindowClosed(false);  // better manage it explicitly

	QObject::connect(qapp_, SIGNAL(aboutToQuit()), this, SLOT(quitCleanup()));
}



GuiApplication::~GuiApplication()
{ }



QApplication* GuiApplication::qappInstance()
{
	return instance()->qapp_;
}



GuiApplication* GuiApplication::instance()
{
	return s_application_instance;
}



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


	// Load application settings (before the plugins)
	AppSettings::init();


	// The main window, construct and show it.
	main_window_ = new MainWindow();  // not a smart pointer, we're deleting it manually in quitCleanup().
	main_window_->show();

	// Useful for troubleshooting
	debug_out_dump("Screen physical DPI: " << main_window_->physicalDpiX()
			<< "x" << main_window_->physicalDpiY() << ".");
	debug_out_dump("Device pixel ratio: " << main_window_->devicePixelRatio() << ".");

	// The Main Loop
	debug_out_info("Entering main loop.");

	int status = qapp_->exec();

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

	// Destruct all global windows. Make sure you're not doing anything interactive there.
	auto windows = QApplication::topLevelWindows();
	for (int i = windows.size(); i > 0; --i) {  // delete in reverse order, just in case
		delete windows[i - 1];
	}

	// Write unwritten settings
	AppSettings::sync();
}







/// @}
