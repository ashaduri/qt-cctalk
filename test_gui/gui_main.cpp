/**************************************************************************
Copyright: (C) 2014 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#include <iostream>  // std::cerr
#include <cstring>  // std::strcmp

#ifdef _WIN32  // console stuff
	/// Require WinXP (needed for console functions).
// 	#define _WIN32_WINNT 0x0501  // this is set by cmake
	#include <windows.h>
	#include <wincon.h>
	#include <cstdio>
#else
	#include <unistd.h>  // sleep
#endif

#include <QObject>
#include <QMessageBox>

#include "gui_application.h"
// #include "version.h"
#include "main_tools.h"



/// Print application version information (--version)
inline void printVersionInfo()
{
	QString str = QStringLiteral("\n") + QObject::tr("qt-cctalk");
	str += QStringLiteral("\n");
	str += QObject::tr("Copyright (C) 2014 - 2021 Alexander Shaduri");
	str += QStringLiteral("\n\n");
	std::cerr << appQStringToConsole(str);
}



/// Print application command-line usage information (--help)
inline void printHelpInfo(const char* argv0)
{
	// Note: This stuff is partially mirrored in doxy_main_page.h
	QString str = QObject::tr("Usage: %1 [parameters...]").arg(QString::fromUtf8(argv0)) + QStringLiteral("\n\n");
	str += QStringLiteral("    --help, -h\t\t") + QObject::tr("Display a short help information and exit.") + QStringLiteral("\n");
	str += QStringLiteral("    --version, -V\t") + QObject::tr("Display version information and exit.") + QStringLiteral("\n");

	std::cerr << appQStringToConsole(str);
}



/// Implementation of main()
int mainImpl(int argc, char** argv)
{
	bool help_mode = false;
	bool version_mode = false;

	for (int i = 0; i < argc; ++i) {
		if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
			help_mode = true;
		} else if (std::strcmp(argv[i], "--version") == 0 || std::strcmp(argv[i], "-V") == 0) {
			version_mode = true;
		}
	}


	int status = 0;

	// Check Qt version
	QString run_qt_version, build_qt_version;
	if (!appCheckQtVersion(run_qt_version, build_qt_version)) {
		// Print this in both CLI and GUI mode, since the GUI creation may fail.
		std::cerr << "Error: Executable \"" << argv[0] << "\" requires Qt " << appQStringToConsole(build_qt_version)
				<< ", found Qt " << appQStringToConsole(run_qt_version) << ".\n";
		new QApplication(argc, argv);  // needed for GUI
		QString msg = QObject::tr("Executable \"%1\" requires Qt %2, found Qt %3.")
				.arg(qAppName()).arg(build_qt_version).arg(run_qt_version);
		QMessageBox::critical(0, QApplication::tr("Incompatible Qt Library Error"), msg, QMessageBox::Abort, 0);
		status = 255;  // why not
	}

	if (status == 0) {
		if (help_mode) {
			printVersionInfo();
			printHelpInfo(argv[0]);

		} else if (version_mode) {
			printVersionInfo();

		} else {  // application/solver mode
			GuiApplication app(argc, argv);
			status = app.run();
		}
	}

	return status;
}



/// Main entry point of the program. Note that there is no WinMain, it's
/// automatically provided by Qt.
int main(int argc, char** argv)
{
	return mainExceptionWrapper(argc, argv, mainImpl);
}




