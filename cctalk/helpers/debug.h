/**************************************************************************
Copyright: (C) 2008 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#ifndef DEBUG_H
#define DEBUG_H

#include <bitset>
#include <string>
#include <sstream>
#include <exception>  // std::exception
#include <cstddef>  // std::size_t
#include <cstring>  // std::strncpy, for string.h
#ifdef _MSC_VER
	#include <string.h>  // strncpy_s
#endif

#include <ostream>
#ifndef APP_DISABLE_QT
	#include <QString>
	#include <QStringRef>
#endif


#ifndef COMMON_SYSTEM_LIBRARY_EXPORT
	#define COMMON_SYSTEM_LIBRARY_EXPORT
#endif



#ifndef APP_DISABLE_QT

/// A workaround for sending QString-s to std::ostream.
inline std::ostream& operator<< (std::ostream& os, const QString& s)
{
	os << s.toUtf8().constData();
	return os;
}


/// A workaround for sending QByteArray-s to std::ostream.
inline std::ostream& operator<< (std::ostream& os, const QByteArray& s)
{
	os << s.constData();
	return os;
}

#endif



/// This is thrown (and caught in main(), or not caught at all) in
/// case of fatal messages.
class DebugFatalException : public std::exception {
	public:

		explicit DebugFatalException(const char* msg)  // copied
		{
			std::size_t buf_len = std::strlen(msg) + 1;
			msg_ = new char[buf_len];
		#ifdef _MSC_VER  // stupid compiler shows a warning for std::strncpy
			strncpy_s(msg_, buf_len, msg, buf_len - 1);  // copies (buf_len-1) chars and adds 0.
		#else
			std::strncpy(msg_, msg, buf_len);  // copies buf_len chars, including 0.
		#endif
		}

		~DebugFatalException() noexcept override
		{
			delete[] msg_;
			msg_ = nullptr;  // protect from double-deletion compiler bugs
		}

		const char* what() const noexcept override
		{
			return msg_;
		}

	private:
		char* msg_;  // holds a copy
};




/// Holder for debug_level::flag enum and related.
namespace debug_level {

	/// Debug message level (severity).
	enum flag {
		none = 0,
		dump = 1 << 0,  ///< traces, etc...
		info = 1 << 1,  ///< simple stuff like "user pressed a button" or "loaded a plugin"
		warn = 1 << 2,  ///< never aborts (stuff like oops, server sent us non-ideal reply goes here)
		error = 1 << 3,  ///< sometimes aborts (stuff like no network, no db... goes here)
		fatal = 1 << 4,  ///< always aborts (stuff like memory corruption is imminent goes here)
		all = dump | info | warn | error | fatal,
		bits = 5
	};

	using type = std::bitset<bits>;

	const char* get_name(flag level);

	const char* get_color_start(flag level);
	const char* get_color_stop(flag level);


	template<class Container> inline
	void get_matched_levels_array(const type& levels, Container& put_here)
	{
		unsigned long levels_ulong = levels.to_ulong();
		if (levels_ulong & debug_level::dump) put_here.push_back(debug_level::dump);
		if (levels_ulong & debug_level::info) put_here.push_back(debug_level::info);
		if (levels_ulong & debug_level::warn) put_here.push_back(debug_level::warn);
		if (levels_ulong & debug_level::error) put_here.push_back(debug_level::error);
		if (levels_ulong & debug_level::fatal) put_here.push_back(debug_level::fatal);
	}
}



/// Holder for debug_dest::flag enum and related.
namespace debug_dest {

	/// Message stream destination. These can be ORed in some situations.
	enum flag {
		none = 0,
		console = 1 << 0,  ///< std::ostream, usually std::cerr.
		file = 1 << 1,  ///< a file
		syslog = 1 << 2,  ///< syslog
		custom = 1 << 3,  ///< custom std::ostream, usually std::stringstream.
		all = console | file | syslog | custom,  ///< all of the above
		def = 1 << 4,  ///< default for current level
		bits = 5,
	};

	using type = std::bitset<bits>;
}


// for easy typing:
#define DEBUG_NONE debug_dest::none
#define DEBUG_CONSOLE debug_dest::console
#define DEBUG_FILE debug_dest::file
#define DEBUG_SYSLOG debug_dest::syslog
#define DEBUG_CUSTOM debug_dest::custom
#define DEBUG_ALL debug_dest::all
#define DEBUG_DEF debug_dest::def




// -------------------------------- Management interface


/// Global "enabled" flag. Default: true.
/// Since the order of destruction in C++ is undefined, the objects below may be destroyed
/// before some others (e.g. Qt global thread pool) that print messages, so we have to
/// disable the whole libdebug printing before main() exits.
COMMON_SYSTEM_LIBRARY_EXPORT
void debug_global_enable(bool enabled);


/// Set default destinations for specified levels.
/// This function is not thread-safe, so it must be called from the main thread
/// before the other threads start using libdebug.
COMMON_SYSTEM_LIBRARY_EXPORT
void debug_set_default_dests(debug_level::type levels, debug_dest::type dests);


/// Set levels that will abort the program if printing to them.
/// This function is not thread-safe, so it must be called from the main thread
/// before the other threads start using libdebug.
COMMON_SYSTEM_LIBRARY_EXPORT
void debug_set_abort_on_levels(debug_level::type levels);


/// Set stream for debug_dest::console.
/// This won't take the ownership, you must make sure that it doesn't get deleted.
/// Set to 0 to disable.
/// This function is not thread-safe, so it must be called from the main thread
/// before the other threads start using libdebug.
/// Notes on thread-safety of streams themselves: Typically, libstdc++ guarantees
/// them to be thread-safe as long as the underlying library is thread-safe. For
/// example, POSIX guarantees fwrite() to be thread-safe. But doing parallel
/// fopen() / fwrite() / fclose() calls will, of course, lead to errors (a file may be
/// closed by another thread while this one tries to write to it).
/// We do NOT do any locking when writing to \c os.
COMMON_SYSTEM_LIBRARY_EXPORT
void debug_set_console_stream(std::ostream* os);


/// Set stream for debug_dest::custom.
/// This won't take the ownership, you must make sure that it doesn't get deleted.
/// See the notes for debug_set_console_stream()
COMMON_SYSTEM_LIBRARY_EXPORT
void debug_set_custom_stream(std::ostream* os);


/// Set the default application name to send along the message.
/// This function is not thread-safe, so it must be called from the main thread
/// before the other threads start using libdebug.
COMMON_SYSTEM_LIBRARY_EXPORT
void debug_set_application_name(const std::string& name);




// -------------------------------- Stream-like interface


// Note that all debug output functions insert newline at the end if
// printing to newline-separated destinations (console, file).


/// Send a message with \c level to a debug stream.
/// If you need to disable a single destination from default,
/// use e.g. DEBUG_DEF | DEBUG_DB.
COMMON_SYSTEM_LIBRARY_EXPORT
void debug_send_to_stream(debug_level::flag level, const std::string& msg,
		debug_dest::type dests = DEBUG_DEF);


// Convenience macros

#define debug_out_dump(output) \
	{ std::stringstream debug_ss; 	debug_ss << output; \
		debug_send_to_stream(debug_level::dump, debug_ss.str()); }

#define debug_out_info(output) \
	{ std::stringstream debug_ss; 	debug_ss << output; \
		debug_send_to_stream(debug_level::info, debug_ss.str()); }

#define debug_out_warn(output) \
	{ std::stringstream debug_ss; 	debug_ss << output; \
		debug_send_to_stream(debug_level::warn, debug_ss.str()); }

#define debug_out_error(output) \
	{ std::stringstream debug_ss; 	debug_ss << output; \
		debug_send_to_stream(debug_level::error, debug_ss.str()); }

#define debug_out_fatal(output) \
	{ std::stringstream debug_ss; 	debug_ss << output; \
		debug_send_to_stream(debug_level::fatal, debug_ss.str()); }



// -------------------------------- Printf-like interface


/// A printf()-like interface to debug_send_to_stream().
COMMON_SYSTEM_LIBRARY_EXPORT
void debug_print(debug_level::flag level, debug_dest::type dests,
		const char* format, ...);



#define debug_print_dump(dests, ...) \
	debug_print(debug_level::dump, dests, __VA_ARGS__)

#define debug_print_info(dests, ...) \
	debug_print(debug_level::info, dests, __VA_ARGS__)

#define debug_print_warn(dests, ...) \
	debug_print(debug_level::warn, dests, __VA_ARGS__)

#define debug_print_error(dests, ...) \
	debug_print(debug_level::error, dests, __VA_ARGS__)

#define debug_print_fatal(dests, ...) \
	debug_print(debug_level::fatal, dests, __VA_ARGS__)






// ---------------------------- Position printing


/// Internal implementation of libdebug
namespace debug_internal {


	/// Internal libdebug class representing current position in source code.
	struct COMMON_SYSTEM_LIBRARY_EXPORT DebugSourcePos {

		inline DebugSourcePos(const std::string& par_file, unsigned int par_line, const std::string& par_func)
				: func(par_func), line(par_line), file(par_file)
		{ }

		std::string str() const;

		std::string func;
		unsigned int line;
		std::string file;
	};


	// it can serve as a manipulator
	inline std::ostream& operator<< (std::ostream& os, const DebugSourcePos& pos)
	{
		return os << pos.str();
	}


	inline std::string format_function_msg(const std::string& func, bool add_suffix)
	{
		// if it's "bool<unnamed>::A::func(int)" or "bool test::A::func(int)",
		// remove the return type and parameters.
		std::string::size_type endpos = func.find('(');
		if (endpos == std::string::npos)
			endpos = func.size();

		// search for first space (after the parameter), or "<unnamed>".
		std::string::size_type pos = func.find_first_of(" >");
		if (pos != std::string::npos) {
			if (func[pos] == '>')
				pos += 2;  // skip ::
			++pos;  // skip whatever character we're over
			// debug_out_info("default", "pos: " << pos << ", endpos: " << endpos << "\n");
			return func.substr(pos >= endpos ? 0 : pos, endpos - pos) + (add_suffix ? "(): " : "()");
		}
		return func.substr(0, endpos) + (add_suffix ? "(): " : "()");
	}

}



// __BASE_FILE__, __FILE__, __LINE__, __func__, __FUNCTION__, __PRETTY_FUNCTION__

// These two may seem pointless, but they actually help to implement
// zero-overhead (if you define them to something else when libdebug is disabled).

/// Current file as const char*.
#define DBG_FILE __FILE__

/// Current line as unsigned int.
#define DBG_LINE __LINE__


/** \def DBG_FUNC_NAME
Function name (without classes / namespaces) only, e.g. "main".
*/
#if defined __GNUC__
	#define DBG_FUNC_NAME __func__
#elif defined _MSC_VER
	#define DBG_FUNC_NAME __FUNCTION__
#else
	#define DBG_FUNC_NAME "unknown"
#endif


/** \def DBG_FUNC_PRNAME
Pretty name is the whole prototype, including return type and classes / namespaces.
*/
#ifdef __GNUC__
	#define DBG_FUNC_PRNAME __PRETTY_FUNCTION__
#else
	#define DBG_FUNC_PRNAME DBG_FUNC_NAME
#endif


/// "class::function()"
#define DBG_FUNC (debug_internal::format_function_msg(DBG_FUNC_PRNAME, false).c_str())

/// "class::function(): "
#define DBG_FUNC_MSG (debug_internal::format_function_msg(DBG_FUNC_PRNAME, true).c_str())


/// An object to send into a stream. Prints position.
#define DBG_POS debug_internal::DebugSourcePos(DBG_FILE, DBG_LINE, DBG_FUNC)


/// Prints "Trace point a reached" to dump-level. Don't need to send into stream, it prints by itself.
#define DBG_TRACE_POINT_MSG(a) debug_out_dump("TRACE point \"" << #a << "\" reached at " << DBG_POS << ".")

/// Prints "Trace point reached" to dump-level. Don't need to send into stream, it prints by itself.
#define DBG_TRACE_POINT_AUTO debug_out_dump("TRACE point reached at " << DBG_POS << ".")


/// Prints "ENTER: function_name" to dump-level. Don't need to send into stream, it prints by itself.
#define DBG_FUNCTION_ENTER_MSG debug_out_dump("ENTER: \"" << DBG_FUNC << "\".")

/// Prints "EXIT:  function_name" to dump-level. Don't need to send into stream, it prints by itself.
#define DBG_FUNCTION_EXIT_MSG debug_out_dump("EXIT:  \"" << DBG_FUNC << "\".")




/// Prints generic message to error-level if assertion fails. Don't need to send into stream, it prints by itself.
#define DBG_ASSERT(cond) \
	do { \
		if (!(cond)) \
			debug_out_error("ASSERTION FAILED: " << #cond << " at " << DBG_POS << "."); \
	} while(false)


/// Prints generic message to error-level if assertion fails. Don't need to send into stream, it prints by itself.
/// Returns from the function on assertion failure.
#define DBG_ASSERT_RETURN(cond, return_value) \
	do { \
		if (!(cond)) { \
			debug_out_error("ASSERTION FAILED: " << #cond << " at " << DBG_POS << "."); \
			return (return_value); \
		} \
	} while(false)


/// Prints generic message to error-level if assertion fails. Don't need to send into stream, it prints by itself.
/// Returns from the function on assertion failure.
#define DBG_ASSERT_RETURN_NONE(cond) \
	do { \
		if (!(cond)) { \
			debug_out_error("ASSERTION FAILED: " << #cond << " at " << DBG_POS << "."); \
			return; \
		} \
	} while(false)


/// Prints generic message to error-level if assertion fails. Don't need to send into stream, it prints by itself.
/// Continues to the next iteration of the enclosing loop on assertion failure.
#define DBG_ASSERT_CONTINUE(cond) \
	if (!(cond)) { \
		debug_out_error("ASSERTION FAILED: " << #cond << " at " << DBG_POS << "."); \
		continue; \
	} else \
		do { } while (false)



/// Prints generic message to error-level if assertion fails. Don't need to send into stream, it prints by itself.
/// Breaks out of the enclosing loop on assertion failure.
#define DBG_ASSERT_BREAK(cond) \
	if (!(cond)) { \
		debug_out_error("ASSERTION FAILED: " << #cond << " at " << DBG_POS << "."); \
		break; \
	} else \
		do { } while (false)


/// Same as DBG_ASSERT, but prints to fatal-level.
/// Use in situation where not handling it would result in a memory corruption.
#define DBG_ASSERT_FATAL(cond) \
	do { \
		if (!(cond)) \
			debug_out_fatal("FATAL ASSERTION FAILED: " << #cond << " at " << DBG_POS << "."); \
	} while(false)


/// Same as DBG_ASSERT, but prints to fatal-level.
/// Use in situation where not handling it would result in a memory corruption.
/// Returns from the function on assertion failure.
#define DBG_ASSERT_RETURN_FATAL(cond, return_value) \
	do { \
		if (!(cond)) { \
			debug_out_fatal("FATAL ASSERTION FAILED: " << #cond << " at " << DBG_POS << "."); \
			return (return_value); \
		} \
	} while(false)


/// Same as DBG_ASSERT, but prints to fatal-level.
/// Use in situation where not handling it would result in a memory corruption.
/// Returns from the function on assertion failure.
#define DBG_ASSERT_RETURN_NONE_FATAL(cond, return_value) \
	do { \
		if (!(cond)) { \
			debug_out_fatal("FATAL ASSERTION FAILED: " << #cond << " at " << DBG_POS << "."); \
			return; \
		} \
	} while(false)



/// Prints msg to error-level if assertion fails. Don't need to send into stream, it prints by itself.
/// The msg format is the same as with debug_out_*() functions.
#define DBG_ASSERT_MSG(cond, msg) \
	do { \
		if (!(cond)) \
			debug_out_error(msg); \
	} while(false)


/// Prints msg to error-level if assertion fails. Don't need to send into stream, it prints by itself.
/// The msg format is the same as with debug_out_*() functions.
/// Returns from the function on assertion failure.
#define DBG_ASSERT_MSG_RETURN(cond, msg, return_value) \
	do { \
		if (!(cond)) { \
			debug_out_error(msg); \
			return (return_value); \
		} \
	} while(false)


/// Prints msg to error-level if assertion fails. Don't need to send into stream, it prints by itself.
/// The msg format is the same as with debug_out_*() functions.
/// Returns from the function on assertion failure.
#define DBG_ASSERT_MSG_RETURN_NONE(cond, msg, return_value) \
	do { \
		if (!(cond)) { \
			debug_out_error(msg); \
			return; \
		} \
	} while(false)


/// Prints msg to error-level if assertion fails. Don't need to send into stream, it prints by itself.
/// Continues to the next iteration of the enclosing loop on assertion failure.
#define DBG_ASSERT_MSG_CONTINUE(cond, msg) \
	if (!(cond)) { \
		debug_out_error(msg); \
		continue; \
	} else \
		do { } while (false)



/// Prints msg to error-level if assertion fails. Don't need to send into stream, it prints by itself.
/// Breaks out of the enclosing loop on assertion failure.
#define DBG_ASSERT_MSG_BREAK(cond, msg) \
	if (!(cond)) { \
		debug_out_error(msg); \
		break; \
	} else \
		do { } while (false)


/// Same as DBG_ASSERT_MSG, but prints to fatal-level.
/// Use in situation where not handling it would result in a memory corruption.
#define DBG_ASSERT_MSG_FATAL(cond, msg) \
	do { \
		if (!(cond)) \
			debug_out_fatal(msg); \
	} while(false)


/// Same as DBG_ASSERT_MSG_RETURN, but prints to fatal-level.
/// Use in situation where not handling it would result in a memory corruption.
/// Returns from the function on assertion failure.
#define DBG_ASSERT_MSG_RETURN_FATAL(cond, msg, return_value) \
	do { \
		if (!(cond)) { \
			debug_out_fatal(msg); \
			return (return_value); \
		} \
	} while(false)


/// Same as DBG_ASSERT_MSG_RETURN_NONE, but prints to fatal-level.
/// Use in situation where not handling it would result in a memory corruption.
/// Returns from the function on assertion failure.
#define DBG_ASSERT_MSG_RETURN_NONE_FATAL(cond, msg, return_value) \
	do { \
		if (!(cond)) { \
			debug_out_fatal(msg); \
			return; \
		} \
	} while(false)





#endif  // hg
