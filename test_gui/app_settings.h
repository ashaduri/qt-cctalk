/**************************************************************************
Copyright: (C) 2009 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <string>



/// This class acts as a namespace for settings-changing functions,
/// which are all static.
class AppSettings {
	public:

		/// This object is non-constructible.
		AppSettings() = delete;


		/// Initialize application settings.
		/// You must call this function before using any other functions of this class.
		/// \return True on success, false on failure of if init() was already called.
		static bool init();

		/// Force write of the settings to disk.
		static void sync();

		/// Get user-specific settings file path.
		/// For linux the location conforms to XDG spec, so it's something like
		/// <tt>~/.config/[vendor]/[program_name].ini</tt>
		/// For windows 7 it's something like
		/// <tt>C:\\Users\\[username]\\AppData\\Roaming\\[vendor]\\[program_name].ini</tt>
		static QString getUserSettingsFile();

		/// Get directory where the user-specific settings file is stored.
		static QString getUserSettingsDirectory();

		/// Check if a value with key \c key exists in the settings.
		/// \param key A unique key
		/// \return True if exists, false otherwise.
		static bool valueExists(const QString& key);

		/// Check if a value with key \c key exists in the settings.
		/// \param key A unique key
		/// \return True if exists, false otherwise.
		static bool valueExists(const char* key);

		/// Add or change a value identified by \c key in the settings.
		/// \param key A unique key
		/// \param value A value. Keep in mind that not all types are convertible
		/// to QVariant and even less of them are serializable.
		static void setValue(const QString& key, const QVariant& value);

		/// Add or change a value identified by \c key in the settings.
		/// \param key A unique key
		/// \param value A value. Keep in mind that not all types are convertible
		/// to QVariant and even less of them are serializable.
		static void setValue(const char* key, const QVariant& value);

		/// Set a value if no such key is present.
		/// \param key A unique key
		/// \param value A value. Keep in mind that not all types are convertible
		/// to QVariant and even less of them are serializable.
		/// \return true if the value was set
		static bool setValueIfNonExistent(const QString& key, const QVariant& value);

		/// Set a value if no such key is present.
		/// \param key A unique key
		/// \param value A value. Keep in mind that not all types are convertible
		/// to QVariant and even less of them are serializable.
		/// \return true if the value was set
		static bool setValueIfNonExistent(const char* key, const QVariant& value);

		/// Remove a value identified by \c key from the settings.
		/// \param key A unique key. Ignored if it doesn't exist.
		static void remove(const QString& key);

		/// Remove a value identified by \c key from the settings.
		/// \param key A unique key. Ignored if it doesn't exist.
		static void remove(const char* key);

		/// Get a value identified by \c key.
		/// \param key A unique key
		/// \return Invalid QVariant if key doesn't exist in the settings, a stored value otherwise.
		static QVariant getValue(const QString& key);

		/// Get a value identified by \c key.
		/// \param key A unique key
		/// \return Invalid QVariant if key doesn't exist in the settings, a stored value otherwise.
		static QVariant getValue(const char* key);

		/// Get a value identified by \c key. If there is no such key in the settings,
		/// \c default_value is returned.
		/// \param key A unique key
		/// \param default_value A value to return if \c key doesn't exist.
		/// \return Stored value or \c default_value
		static QVariant getValue(const QString& key, const QVariant& default_value);

		/// Get a value identified by \c key. If there is no such key in the settings,
		/// \c default_value is returned.
		/// \param key A unique key
		/// \param default_value A value to return if \c key doesn't exist.
		/// \return Stored value or \c default_value
		static QVariant getValue(const char* key, const QVariant& default_value);

		/// This function is equivalent to getValue(const QString& key), except the
		/// returned value is converted to \c ValueType.
		/// \tparam ValueType type of the stored value
		/// \param key A unique key
		/// \return Default-constructed value if key doesn't exist in the settings, a stored value otherwise.
		template <typename ValueType>
		static ValueType getValue(const QString& key)
		{
			return getValue(key).value<ValueType>();
		}

		/// This function is equivalent to getValue(const QString& key), except the
		/// returned value is converted to \c ValueType.
		/// \tparam ValueType type of the stored value
		/// \param key A unique key
		/// \return Default-constructed value if key doesn't exist in the settings, a stored value otherwise.
		template <typename ValueType>
		static ValueType getValue(const char* key)
		{
			return getValue(key).value<ValueType>();
		}

		/// This function is equivalent to getValue(const QString& key, const QVariant& default_value), except the
		/// returned value is converted to \c ValueType.
		/// \tparam ValueType type of the stored value
		/// \param key A unique key
		/// \param default_value A value to return if \c key doesn't exist.
		/// \return Stored value or \c default_value
		template <typename ValueType>
		static ValueType getValue(const QString& key, const ValueType& default_value)
		{
			if (valueExists(key)) {
				return getValue<ValueType>(key);
			}
			return default_value;
		}

		/// This function is equivalent to getValue(const QString& key, const QVariant& default_value), except the
		/// returned value is converted to \c ValueType.
		/// \tparam ValueType type of the stored value
		/// \param key A unique key
		/// \param default_value A value to return if \c key doesn't exist.
		/// \return Stored value or \c default_value
		template <typename ValueType>
		static ValueType getValue(const char* key, const ValueType& default_value)
		{
			return getValue<ValueType>(QString::fromUtf8(key), default_value);
		}

		/// Return all keys for a group (prefix). If empty, return all keys.
		static QStringList getKeys(const QString& group = QString());

		/// Return all keys for a group (prefix). If empty, return all keys.
		static QStringList getKeys(const char* group);

};



#endif  // hg

