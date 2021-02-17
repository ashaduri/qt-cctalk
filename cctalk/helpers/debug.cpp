/**************************************************************************
Copyright: (C) 2008 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#include <cstdarg>  // va_*
#include <map>
#include <ostream>
#include <vector>
#include <fstream>
#include <ios>  // std::ios::*
#include <cstddef>  // std::size_t
#include <cstdlib>  // std::exit, SUCCESS_FAILURE
#include <QMutex>
#include <QMutexLocker>

#ifndef _WIN32
	#include <syslog.h>
	#include <unistd.h>
#endif

#include "debug.h"


/// \def HAVE_WIN_SE_FUNCS
/// Defined to 0 or 1. If 1, compiler supports Win32's "secure" *_s() functions (since msvc 2005/8.0).
/// If some of the functions have different requirements, they are listed separately.
#ifndef HAVE_WIN_SE_FUNCS
	#if defined _MSC_VER && _MSC_VER >= 1400
		#define HAVE_WIN_SE_FUNCS 1
	#else
		#define HAVE_WIN_SE_FUNCS 0
	#endif
#endif


/// \def HAVE_REENTRANT_LOCALTIME
/// Defined to 0 or 1. If 1, localtime() is reentrant.
#ifndef HAVE_REENTRANT_LOCALTIME
	// win32 and solaris localtime() is reentrant
	#if defined _WIN32 || defined sun || defined __sun
		#define HAVE_REENTRANT_LOCALTIME 1
	#else
		#define HAVE_REENTRANT_LOCALTIME 0
	#endif
#endif



/// Internal libdebug class representing debug stream flags.
struct DebugFlags {

	DebugFlags()
	{
		level_names[debug_level::fatal] = "fatal";
		level_names[debug_level::error] = "error";
		level_names[debug_level::warn] = "warn";
		level_names[debug_level::info] = "info";
		level_names[debug_level::dump] = "dump";

		level_colors[debug_level::fatal] = "\033[1;4;31m";  // red underlined
		level_colors[debug_level::error] = "\033[1;31m";  // red
		level_colors[debug_level::warn] = "\033[1;35m";  // magenta
		level_colors[debug_level::info] = "\033[1;36m";  // cyan
		level_colors[debug_level::dump] = "\033[1;32m";  // green
	}

	std::map<debug_level::flag, const char*> level_names;
	std::map<debug_level::flag, const char*> level_colors;
};




namespace {

	/// Global "enabled" flag.
	/// Since the order of destruction in C++ is undefined, the objects below may be destroyed
	/// before some others (e.g. Qt global thread pool) that print messages, so we have to
	/// disable the whole libdebug printing before main() exits.
	bool s_debug_global_enabled = true;

	/// Const, so it's thread-safe.
	const DebugFlags s_debug_flags;

	/// This is not thread-safe, modify from a single thread only, before other threads
	/// can access it.
	std::map<debug_level::flag, debug_dest::type> s_debug_default_dests;

	/// This is not thread-safe, modify from a single thread only, before other threads
	/// can access it.
	debug_level::type s_debug_abort_on_levels;

	/// This is not thread-safe, modify from a single thread only, before other threads
	/// can access it.
	std::string s_debug_application_name;

	/// This is not thread-safe, modify from a single thread only, before other threads
	/// can access it. The actual writing safety depends on implementation, see
	/// debug_set_console_stream().
	std::ostream* s_debug_console_stream = nullptr;

	/// This is not thread-safe, modify from a single thread only, before other threads
	/// can access it. The actual writing safety depends on implementation, see
	/// debug_set_console_stream().
	std::ostream* s_debug_custom_stream = nullptr;

	/// This is not thread-safe, modify from a single thread only, before other threads
	/// can access it. The actual writing is thread-safe.
	std::map<debug_level::flag, std::pair<QMutex*, std::string> > s_debug_output_files;

}




namespace debug_level {

	const char* get_name(flag level)
	{
		std::map<flag, const char*>::const_iterator iter = s_debug_flags.level_names.find(level);
		if (iter == s_debug_flags.level_names.end()) {
			return "";
		}
		return iter->second;
	}

	const char* get_color_start(flag level)
	{
		std::map<flag, const char*>::const_iterator iter = s_debug_flags.level_colors.find(level);
		if (iter == s_debug_flags.level_colors.end()) {
			return "";
		}
		return iter->second;
	}

	const char* get_color_stop(flag level)
	{
		return "\033[0m";
	}

}




void debug_global_enable(bool enabled)
{
	s_debug_global_enabled = enabled;
}



void debug_set_default_dests(debug_level::type levels, debug_dest::type dests)
{
	std::vector<debug_level::flag> matched;
	debug_level::get_matched_levels_array(levels, matched);

	for(std::vector<debug_level::flag>::const_iterator iter = matched.begin(); iter != matched.end(); ++iter) {
		s_debug_default_dests[*iter] = dests;
	}
}



void debug_set_abort_on_levels(debug_level::type levels)
{
	s_debug_abort_on_levels = levels;
}



void debug_set_console_stream(std::ostream* os)
{
	s_debug_console_stream = os;
}



void debug_set_custom_stream(std::ostream* os)
{
	s_debug_custom_stream = os;
}



void debug_set_application_name(const std::string& name)
{
	s_debug_application_name = name;
}



namespace {

	/// Holder for (internal) debug_format::flag enum and related.
	namespace debug_format {
		enum flag {
			none = 0,
			time = 1 << 0,
			color = 1 << 1,
			level = 1 << 2,
			appname = 1 << 3,
			bits = 4
		};

		using type = std::bitset<bits>;
	}


	// a helper function, formats a message.
	std::string debug_format_message(debug_level::flag level, debug_format::type format_flags, const std::string& msg)
	{
		if (msg.empty())
			return msg;

		std::string ret;
		ret.reserve(msg.size() + 42);  // time + level (colored) + application

		// This is not implemented in this version
		if (format_flags.to_ulong() & debug_format::time) {  // print time
			// don't use locale, the format doesn't depend on it.
			// ret += hz::format_date("%Y-%m-%d %H:%M:%S: ", false);
			const std::time_t timet = std::time(nullptr);

		if (format_flags.to_ulong() & debug_format::level) {  // print level name
			bool use_color = (format_flags.to_ulong() & debug_format::color);
			if (use_color)
				ret += debug_level::get_color_start(level);

			std::string level_name = std::string("<") + debug_level::get_name(level) + ">";
			ret += level_name + std::string(8 - level_name.size(), ' ');  // align to 8 chars

			if (use_color)
				ret += debug_level::get_color_stop(level);
		}

		#if defined HAVE_WIN_SE_FUNCS && HAVE_WIN_SE_FUNCS
			struct std::tm ltm;
			localtime_s(&ltm, &timet);  // shut up msvc (it thinks std::localtime() is unsafe)
			const struct std::tm* ltmp = &ltm;
		#elif defined HAVE_REENTRANT_LOCALTIME && HAVE_REENTRANT_LOCALTIME
			const struct std::tm* ltmp = std::localtime(&timet);
		#else
			struct std::tm ltm;
			localtime_r(&timet, &ltm);  // use reentrant localtime_r (posix/bsd and related)
			const struct std::tm* ltmp = &ltm;
		#endif
			if (ltmp) {
				// FIXME
				// ret += hz::string_sprintf("%02d:%02d:%02d", ltmp->tm_hour, ltmp->tm_min, ltmp->tm_sec) + " ";
			}
		}


		if (format_flags.to_ulong() & debug_format::appname) {  // print application name
			ret += std::string("[") + s_debug_application_name + "] ";
		}

		ret += msg;

		return ret;
	}



}  // anon ns




void debug_send_to_stream(debug_level::flag level, const std::string& msg, debug_dest::type dests)
{
	if (!s_debug_global_enabled) {
		return;
	}

	if (dests.to_ulong() & debug_dest::def) {

		std::map<debug_level::flag, debug_dest::type>::const_iterator iter = s_debug_default_dests.find(level);

		if (iter == s_debug_default_dests.end()) {
			if (level != debug_level::error) {
				debug_send_to_stream(debug_level::error,
						"debug_send_to_stream(): Debug level not found in default destinations map.", debug_dest::console);
			} // if it's error level, I've no idea what else we can do.
			return;
		}

		dests = iter->second & ~dests;  // disables specified destinations
	}

	unsigned long val = dests.to_ulong();
	if (!val)  // shortcut
		return;


	
	if (val & debug_dest::console) {
		if (s_debug_console_stream) {
			debug_format::type flags = debug_format::level | debug_format::appname | debug_format::time;
#ifndef _WIN32
			// While it is possible to have stdout go to console and stderr to a file, we don't handle that.
			if (isatty(STDERR_FILENO) != 0) {
				flags |= debug_format::color;
			}
#endif
			*s_debug_console_stream << debug_format_message(level, flags, msg) << "\n";
		}
	}



	if (val & debug_dest::custom) {
		if (s_debug_custom_stream) {
			debug_format::type flags = debug_format::level | debug_format::appname | debug_format::time;
			*s_debug_custom_stream << debug_format_message(level, flags, msg) << "\n";
		}
	}



#ifndef _WIN32
	if (val & debug_dest::syslog) {
		int priority = LOG_ALERT;  // internal error, invalid level.
		switch(level) {
			case debug_level::dump: priority = LOG_DEBUG; break;
			case debug_level::info: priority = LOG_INFO; break;
			case debug_level::warn: priority = LOG_WARNING; break;
			case debug_level::error: priority = LOG_ERR; break;
			case debug_level::fatal: priority = LOG_CRIT; break;
			default: break;  // shut up compiler
		}

		// Log to syslog server.
		openlog(s_debug_application_name.c_str(), LOG_PID, LOG_USER);
		syslog(priority, "%s", debug_format_message(level, debug_format::none, msg).c_str());
		closelog();
	}
#endif



	if (val & debug_dest::file) {

		std::map< debug_level::flag, std::pair<QMutex*, std::string> >::const_iterator iter = s_debug_output_files.find(level);

		if (iter != s_debug_output_files.end()) {
			std::string file = iter->second.second;
			QMutexLocker locker(iter->second.first);
			if (!file.empty()) {
				// Open it each time. This will hopefully help with avoiding fs buffers and ntfs corruption.
				std::ofstream of(file.c_str(), std::ios::app | std::ios::out | std::ios::binary);  // append
				if (of.fail()) {
					// failed, send the error to all the other destinations
					debug_send_to_stream(debug_level::error,
							"debug_send_to_stream(): Could not open log file \"" + file + "\" for writing.",
							dests & ~debug_dest::type(debug_dest::file));
				}

				debug_format::type flags = debug_format::level | debug_format::appname | debug_format::time;

				of << debug_format_message(level, flags, msg) << "\n";

				of.close();

				if (of.fail()) {
					// failed, send the error to all the other destinations
					debug_send_to_stream(debug_level::error,
							"debug_send_to_stream(): Could not write/close log file \"" + file + "\".",
							dests & ~debug_dest::type(debug_dest::file));
				}
			}
		}
	}



	// if fatal, exit
	if (s_debug_abort_on_levels.to_ulong() & level) {
		throw DebugFatalException(msg.c_str());
		// Note that Qt doesn't support throwing exceptions from slots, so we have
		// to catch them in QApplication::notify().
		// We could call exit() (no stack unwind, but calls static object destructors
		// and flushes C streams), but it's hard to get automatic core dump or backtrace
		// from it.
		// We could call abort() (no unwind at all), but it's too brutal.
	}

}




//
// void debug_print(debug_level::flag level, debug_dest::type dests, const char* format, ...)
// {
// 	std::va_list ap;
// 	va_start(ap, format);
//
// 	std::string s = hz::string_vsprintf(format, ap);
//
// 	va_end(ap);
// 	debug_send_to_stream(level, s, dests);
// }
//



namespace debug_internal {


	std::string DebugSourcePos::str() const
	{
		std::ostringstream os;
		os << "(function: " << func << ", file: " << file << ":" << line << ")";
		return os.str();
	}


}



