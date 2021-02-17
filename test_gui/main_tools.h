/**************************************************************************
Copyright: (C) 2013 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#ifndef MAIN_TOOLS_H
#define MAIN_TOOLS_H

#include <cstdlib>  // EXIT_*
#include <iostream>  // std::cerr
#include <exception>  // std::exception, std::set_terminate()
#include <string>

#include <QString>
#include <QtGlobal>


/// \def HAVE_VERBOSE_TERMINATE_HANDLER
/// Defined to 0 or 1. If 1, compiler supports __gnu_cxx::__verbose_terminate_handler.
#ifndef HAVE_VERBOSE_TERMINATE_HANDLER
	#if defined __GNUC__
		#define HAVE_VERBOSE_TERMINATE_HANDLER 1
	#else
		#define HAVE_VERBOSE_TERMINATE_HANDLER 0
	#endif
#endif




/// An argument type for mainExceptionWrapper
using MainImplFunc = int (*)(int, char **);



/// This function calls \c main_impl() but wraps it in verbose exception handlers.
inline int mainExceptionWrapper(int argc, char** argv, MainImplFunc main_impl)
{
#if defined HAVE_VERBOSE_TERMINATE_HANDLER && HAVE_VERBOSE_TERMINATE_HANDLER
	// Verbose uncaught exception handler
	std::set_terminate(__gnu_cxx::__verbose_terminate_handler);
#else
	try {
#endif

		return main_impl(argc, argv);

// print uncaught exceptions for non-gcc-compatible
#if !(defined HAVE_VERBOSE_TERMINATE_HANDLER && HAVE_VERBOSE_TERMINATE_HANDLER)
	}  // end try
	catch(std::exception& e) {
		// don't use anything other than cerr here, it's the most safe option.
		std::cerr << "main(): Unhandled exception: " << e.what() << std::endl;

		return EXIT_FAILURE;
	}
	catch(...) {  // this guarantees proper unwinding in case of unhandled exceptions (win32 I think)
		std::cerr << "main(): Unhandled unknown exception." << std::endl;

		return EXIT_FAILURE;
	}
#endif

}



/// Check Qt version. If the runtime version is less than build version, return false.
inline bool appCheckQtVersion(QString& run_version, QString& build_version)
{
	// Generally, we require at least the version it was built with, since we may
	// be using some functions (though inline functions of Qt) which may not be
	// available in older versions. For example, QTabWidget::heightForWidth() becomes
	// required when building against Qt 4.8, even though we don't call it. The build-time
	// checks already specify the minimum supported Qt versions for each platform.
	run_version = QString::fromLatin1(qVersion());
	build_version = QString::fromLatin1(QT_VERSION_STR);

	int run_version_int = (run_version.section(QChar::fromLatin1('.'), 0, 0).toInt() << 16)
			+ (run_version.section(QChar::fromLatin1('.'), 1, 1).toInt() << 8)
			+ run_version.section(QChar::fromLatin1('.'), 2, 2).toInt();
	int build_version_int = (build_version.section(QChar::fromLatin1('.'), 0, 0).toInt() << 16)
			+ (build_version.section(QChar::fromLatin1('.'), 1, 1).toInt() << 8)
			+ build_version.section(QChar::fromLatin1('.'), 2, 2).toInt();

	return run_version_int >= build_version_int;
}



/// Convert QString to a cerr-compatible const char* data, taking
/// in account the console locale.
inline std::string appQStringToConsole(const QString& str)
{
	// For some very weird reason returning const char* doesn't
	// work here (even though the original variable is alive during
	// the call), so use std::string instead.
#ifdef _WIN32
	return str.toLocal8Bit().constData();
#else
	return str.toUtf8().constData();
#endif
}





#endif

