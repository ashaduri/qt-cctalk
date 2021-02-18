/**************************************************************************
Copyright: (C) 2009 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#include <QSettings>
#include <QtGlobal>
#include <QCoreApplication>
#include <QFileInfo>

#include "app_settings.h"
#include "cctalk/helpers/debug.h"



namespace {

	QSettings* s_app_settings = nullptr;

}



bool AppSettings::init()
{
	// Load the system settings from the current directory
	QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, QStringLiteral("."));

	if (s_app_settings) {
		debug_out_warn(DBG_FUNC_MSG << "Settings already loaded.");
		return false;  // already loaded
	}

	// This searches in "." first (for system-wide settings). Then in user-specific directory.
	// The write happens to user-specific file.
	s_app_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope,
			QCoreApplication::organizationName(), QCoreApplication::applicationName());

	debug_out_info("Using \"" << s_app_settings->fileName() << "\" as a user-specific settings file");

	return true;
}



void AppSettings::sync()
{
	DBG_ASSERT_MSG_FATAL(s_app_settings,
			DBG_FUNC_MSG << "You must call settings_init() first.");

	s_app_settings->sync();
}



QString AppSettings::getUserSettingsFile()
{
	DBG_ASSERT_MSG_FATAL(s_app_settings,
			DBG_FUNC_MSG << "You must call settings_init() first.");

	return s_app_settings->fileName();
}



QString AppSettings::getUserSettingsDirectory()
{
	return QFileInfo(getUserSettingsFile()).absolutePath();
}



bool AppSettings::valueExists(const QString& key)
{
	DBG_ASSERT_MSG_FATAL(s_app_settings,
			DBG_FUNC_MSG << "You must call settings_init() first.");
	return s_app_settings->contains(key);
}



bool AppSettings::valueExists(const char* key)
{
	return valueExists(QString::fromUtf8(key));
}



void AppSettings::setValue(const QString& key, const QVariant& value)
{
	DBG_ASSERT_MSG_FATAL(s_app_settings,
			DBG_FUNC_MSG << "You must call settings_init() first.");
	s_app_settings->setValue(key, value);
}



void AppSettings::setValue(const char* key, const QVariant& value)
{
	setValue(QString::fromUtf8(key), value);
}



bool AppSettings::setValueIfNonExistent(const QString& key, const QVariant& value)
{
	if (!valueExists(key)) {
		setValue(key, value);
		return true;
	}
	return false;
}



bool AppSettings::setValueIfNonExistent(const char* key, const QVariant& value)
{
	return setValueIfNonExistent(QString::fromUtf8(key), value);
}



void AppSettings::remove(const QString& key)
{
	DBG_ASSERT_MSG_FATAL(s_app_settings,
			DBG_FUNC_MSG << "You must call settings_init() first.");
	s_app_settings->remove(key);
}



void AppSettings::remove(const char* key)
{
	remove(QString::fromUtf8(key));
}



QVariant AppSettings::getValue(const QString& key)
{
	DBG_ASSERT_MSG_FATAL(s_app_settings,
			DBG_FUNC_MSG << "You must call settings_init() first.");

	if (s_app_settings->contains(key)) {
		return s_app_settings->value(key);
	}

	// key not found
	DBG_ASSERT_MSG(s_app_settings->contains(key),
			DBG_FUNC_MSG << "key \"" << key << "\" is not present in default config.");

	return QVariant();
}



QVariant AppSettings::getValue(const char* key)
{
	return getValue(QString::fromUtf8(key));
}



QVariant AppSettings::getValue(const QString& key, const QVariant& default_value)
{
	DBG_ASSERT_MSG_FATAL(s_app_settings,
			DBG_FUNC_MSG << "You must call settings_init() first.");

	return s_app_settings->value(key, default_value);
}



QVariant AppSettings::getValue(const char* key, const QVariant& default_value)
{
	return getValue(QString::fromUtf8(key), default_value);
}



QStringList AppSettings::getKeys(const QString& group)
{
	DBG_ASSERT_MSG_FATAL(s_app_settings,
			DBG_FUNC_MSG << "You must call settings_init() first.");

	if (!group.isEmpty()) {
		s_app_settings->beginGroup(group);
	}
	QStringList ret = s_app_settings->allKeys();
	if (!group.isEmpty()) {
		s_app_settings->endGroup();
	}
	return ret;
}



QStringList AppSettings::getKeys(const char* group)
{
	return getKeys(QString::fromUtf8(group));
}




