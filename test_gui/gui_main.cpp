/**************************************************************************
Copyright: (C) 2014 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#include <iostream>  // std::cout
#include <cstring>  // std::strcmp
#include <QObject>
#include <QMessageBox>

#include "gui_application.h"



/// Print application version information (--version)
inline void printVersionInfo()
{
	QString str = QStringLiteral("\n") + QObject::tr("qt-cctalk");
	str += QStringLiteral("\n");
	str += QObject::tr("Copyright (C) 2014 - 2021 Alexander Shaduri");
	str += QStringLiteral("\n\n");
	std::cout << str.toUtf8().constData();
}



/// Print application command-line usage information (--help)
inline void printHelpInfo(const char* argv0)
{
	// Note: This stuff is partially mirrored in doxy_main_page.h
	QString str = QObject::tr("Usage: %1 [parameters...]").arg(QString::fromUtf8(argv0)) + QStringLiteral("\n\n");
	str += QStringLiteral("    --help, -h\t\t") + QObject::tr("Display a short help information and exit.") + QStringLiteral("\n");
	str += QStringLiteral("    --version, -V\t") + QObject::tr("Display version information and exit.") + QStringLiteral("\n");

	std::cout << str.toUtf8().constData();
}



/// Main entry point of the program. Note that there is no WinMain, it's
/// automatically provided by Qt.
int main(int argc, char** argv)
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

	if (help_mode) {
		printVersionInfo();
		printHelpInfo(argv[0]);

	} else if (version_mode) {
		printVersionInfo();

	} else {  // application mode
		GuiApplication app(argc, argv);
		status = app.run();
	}

	return status;
}




