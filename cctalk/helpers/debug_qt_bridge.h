/**************************************************************************
Copyright: (C) 2009 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#ifndef DEBUG_QT_BRIDGE_H
#define DEBUG_QT_BRIDGE_H

#include <QtGlobal>
#include <QMutex>
#include "debug.h"


namespace internal {

	template <typename Dummy>
	struct DebugQtMessageFlagsStaticHolder {
		static bool suppress_messages;
		static QMutex mutex;
	};

	// definitions
	template<typename Dummy> bool DebugQtMessageFlagsStaticHolder<Dummy>::suppress_messages = false;
	template<typename Dummy> QMutex DebugQtMessageFlagsStaticHolder<Dummy>::mutex;

	using DebugQtMessageFlagsHolder = DebugQtMessageFlagsStaticHolder<void>;  // one (and only) specialization.

}


/// Enable / disable showing of Qt messages. Returns the old value.
inline bool debug_qt_suppress_messages(bool suppress)
{
	QMutexLocker locker(&internal::DebugQtMessageFlagsHolder::mutex);
	bool old_value = internal::DebugQtMessageFlagsHolder::suppress_messages;
	internal::DebugQtMessageFlagsHolder::suppress_messages = suppress;
	return old_value;
}



/*
/// Qt Exception
class DebugQtError : public std::logic_error {
	public:
		DebugQtError(const QString& message, const std::string& category_, const std::string& function_, const std::string& file_, int line_)
				: std::logic_error(message.toUtf8().constData()), category(category_), function(function_), file(file_), line(line_)
		{ }

		std::string category;
		std::string function;
		std::string file;
		int line  = -1;
};
*/




inline void debug_qt5_message_handler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
	{
		QMutexLocker locker(&internal::DebugQtMessageFlagsHolder::mutex);
		if (internal::DebugQtMessageFlagsHolder::suppress_messages) {
			// fatal errors still abort (Qt does that).
			return;
		}
	}

	std::string function = (context.function ? context.function : "");
	std::string file = (context.file ? context.file : "");
// 	int line = context.line;
	std::string category = (context.category ? context.category : "");

	std::string cat = "Qt";
	if (!category.empty()) {
		cat = "Qt " + category;
	}

	std::string decorated_message = std::string("[") + cat + "] " + msg.toUtf8().constData();
	if (!function.empty()) {
		decorated_message += std::string("\nFunction: ") + function;
	}

	switch (type) {
		case QtDebugMsg:
			debug_out_info(decorated_message);
			break;
		case QtInfoMsg:
			debug_out_info(decorated_message);
			break;
		case QtWarningMsg:
			debug_out_warn(decorated_message);
			break;
		case QtCriticalMsg:
			debug_out_error(decorated_message);
			break;
		case QtFatalMsg:  // QVector uses Q_ASSERT(), which uses qFatal().
			debug_out_error(decorated_message);  // just prints the message (don't use "fatal", it aborts).
			// Qt does not support throwing exceptions from message handler
			// (it has calling functions marked as noexcept), so we can only print here.
			// Fatal messages in Qt will cause a break in Windows debugger and
			// std::abort() (see qt_message_fatal()).
			// We _could_ define Q_ASSERT() and Q_ASSERT_X() to something throwing
			// (project-wide), but cmake doesn't support function-like macro definition and
			// it can cause trouble in Qt since it's not designed for exception safety.
// 			throw DebugQtError(msg, category, function, file, line);
			break;
	}
}



/// Use libdebug for Qt messages
inline void debug_install_qt_message_handler()
{
	qInstallMessageHandler(debug_qt5_message_handler);
}




#endif
