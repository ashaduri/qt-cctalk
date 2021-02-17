/**************************************************************************
Copyright: (C) 2014 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#ifndef CCTALK_TOOLS_H
#define CCTALK_TOOLS_H

#include <QString>
#include <QObject>
#include <QSerialPortInfo>

#include "cctalk/bill_validator_device.h"
#include "cctalk/coin_acceptor_device.h"
#include "app_settings.h"
#include "logging_tools.h"



/// Set up cctalk devices - coin acceptor and bill validator. This
/// reads the config file to get the device settings.
/// \return Error string or empty string if no error.
inline QString setUpCctalkDevices(qtcc::BillValidatorDevice* bill_validator, qtcc::CoinAcceptorDevice* coin_acceptor,
		const std::function<void(QString message)>& message_logger)
{
	QStringList port_devices;
	{
		QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
		for (const QSerialPortInfo& info : ports) {
			message_logger(QObject::tr("Found port \"%1\".").arg(info.systemLocation()));
			port_devices << info.systemLocation();
		}
	}
	if (port_devices.empty()) {
		port_devices << QStringLiteral("/dev/ttyUSB0");
	}

	// Note: If using multiple devices, all ccTalk options must be the same if the
	// device name is the same; except for cctalk_address, which must be
	// non-zero and must be different.

	auto bill_device = AppSettings::getValue<QString>(QStringLiteral("bill_validator/serial_device_name"), port_devices.value(0));
	auto bill_cctalk_address = AppSettings::getValue<quint8>(QStringLiteral("bill_validator/cctalk_address"),
			ccCategoryGetDefaultAddress(qtcc::CcCategory::BillValidator));
	bool bill_des_encrypted = AppSettings::getValue<bool>(QStringLiteral("bill_validator/cctalk_des_encrypted"), false);
	bool bill_checksum_16bit = AppSettings::getValue<bool>(QStringLiteral("bill_validator/cctalk_checksum_16bit"), false);

	auto coin_device = AppSettings::getValue<QString>(QStringLiteral("coin_acceptor/serial_device_name"), port_devices.value(1));
	auto coin_cctalk_address = AppSettings::getValue<quint8>(QStringLiteral("coin_acceptor/cctalk_address"),
			ccCategoryGetDefaultAddress(qtcc::CcCategory::CoinAcceptor));
	bool coin_des_encrypted = AppSettings::getValue<bool>(QStringLiteral("coin_acceptor/cctalk_des_encrypted"), false);
	bool coin_checksum_16bit = AppSettings::getValue<bool>(QStringLiteral("coin_acceptor/cctalk_checksum_16bit"), false);

	if (!bill_device.isEmpty() && bill_device == coin_device) {
		if (bill_cctalk_address == 0x00 || coin_cctalk_address == 0x00) {
			return QObject::tr("! At least one ccTalk device has address set to 0 in a multi-device serial network, cannot continue.");
		}
		if (bill_cctalk_address == coin_cctalk_address) {
			return QObject::tr("! Two ccTalk devices have the same address in a multi-device serial network, cannot continue.");
		}
		if (bill_checksum_16bit != coin_checksum_16bit || bill_des_encrypted != coin_des_encrypted) {
			return QObject::tr("! ccTalk or serial options are different for devices in a multi-device serial network, cannot continue.");
		}
		if (bill_checksum_16bit || coin_checksum_16bit) {
			return QObject::tr("! 16-bit checksum enabled for at least one ccTalk device in a multi-device serial network, cannot continue.");
		}
	}

	bool show_full_response = AppSettings::getValue<bool>("cctalk/show_full_response", false);
	bool show_serial_request = AppSettings::getValue<bool>("cctalk/show_serial_request", false);
	bool show_serial_response = AppSettings::getValue<bool>("cctalk/show_serial_response", false);
	bool show_cctalk_request = AppSettings::getValue<bool>("cctalk/show_cctalk_request", true);
	bool show_cctalk_response = AppSettings::getValue<bool>("cctalk/show_cctalk_response", true);

	// Bill validator
	if (bill_validator) {
		if (bill_device.isEmpty()) {
			return QObject::tr("! Bill validator configured device name is empty, cannot continue.");
		}
		message_logger(QObject::tr("* Bill validator configured device: %1").arg(bill_device));
		bill_validator->getLinkController().setCcTalkOptions(bill_device, bill_cctalk_address, bill_checksum_16bit, bill_des_encrypted);
		bill_validator->getLinkController().setLoggingOptions(show_full_response, show_serial_request, show_serial_response,
				show_cctalk_request, show_cctalk_response);

		bill_validator->setBillValidationFunction([]([[maybe_unused]] quint8 bill_id, [[maybe_unused]] const qtcc::CcIdentifier& identifier) {
			return true;  // accept all supported bills
		});

		QObject::connect(bill_validator, &qtcc::CctalkDevice::logMessage, message_logger);
	}

	// Coin acceptor
	if (coin_acceptor) {
		if (coin_device.isEmpty()) {
			return QObject::tr("! Coin acceptor configured device name is empty, cannot continue.");
		}
		message_logger(QObject::tr("* Coin acceptor configured device: %1").arg(coin_device));
		coin_acceptor->getLinkController().setCcTalkOptions(coin_device, coin_cctalk_address, coin_checksum_16bit, coin_des_encrypted);
		coin_acceptor->getLinkController().setLoggingOptions(show_full_response, show_serial_request, show_serial_response,
				show_cctalk_request, show_cctalk_response);

		QObject::connect(coin_acceptor, &qtcc::CctalkDevice::logMessage, message_logger);
	}

	return QString();
}



/// Process string message from cctalk.
/// \return processed message
inline QString ccProcessLoggingMessage(QString msg, bool markup_output)
{
	static bool show_full_response = AppSettings::getValue<bool>("cctalk/show_full_response", false);
	static bool show_serial_request = AppSettings::getValue<bool>("cctalk/show_serial_request", false);
	static bool show_serial_response = AppSettings::getValue<bool>("cctalk/show_serial_response", false);
	static bool show_cctalk_request = AppSettings::getValue<bool>("cctalk/show_cctalk_request", true);
	static bool show_cctalk_response = AppSettings::getValue<bool>("cctalk/show_cctalk_response", true);

	bool show_msg = true;

	QString color = QStringLiteral("#000000");  // black
	if (show_full_response && msg.startsWith(QStringLiteral("< Full response:"))) {
		color = QStringLiteral("#c0c0c0");  // grey
	} else if (show_serial_response && msg.startsWith(QStringLiteral("< Response:"))) {
		color = QStringLiteral("#00A500");  // dark green
	} else if (show_cctalk_response && msg.startsWith(QStringLiteral("< ccTalk"))) {
		color = QStringLiteral("#00A597");  // marine
	} else if (show_serial_request && msg.startsWith(QStringLiteral("> Request:"))) {
		color = QStringLiteral("#7C65A5");  // violet
	} else if (show_cctalk_request && msg.startsWith(QStringLiteral("> ccTalk"))) {
		color = QStringLiteral("#5886A5");  // blue-grey
	} else if (msg.startsWith(QStringLiteral("* "))) {
		color = QStringLiteral("#B900CA");  // pink-violet
	} else if (msg.startsWith(QStringLiteral("! ")) || msg.startsWith(QStringLiteral("!<")) || msg.startsWith(QStringLiteral("!>"))) {
		color = QStringLiteral("#FF0000");  // red
	}

	static MessageAccumulator acc1(1), acc2(2), acc3(3), acc4(4);
	static int last_repeat_count = 0;

	const int max_shown_matches = 3;
	const int match_step = 40;
	int matches1 = acc1.push(msg);
	int matches2 = acc2.push(msg);
	int matches3 = acc3.push(msg);
// 	int matches4 = acc4.push(msg);

	if (matches1 > max_shown_matches) {
		if (matches1 % match_step == 0 && matches1 != last_repeat_count) {
			msg = QObject::tr("- The last message was repeated %1 times total").arg(matches1);
			last_repeat_count = matches1;
		} else {
			show_msg = false;
		}
	} else if (matches2 > max_shown_matches) {
		if (matches2 % match_step == 0 && matches2 != last_repeat_count) {
			msg = QObject::tr("- The last %1 messages were repeated %2 times total").arg(2).arg(matches2);
			last_repeat_count = matches2;
		} else {
			show_msg = false;
		}
	} else if (matches3 > max_shown_matches) {
		if (matches3 % match_step == 0 && matches3 != last_repeat_count) {
			msg = QObject::tr("- The last %1 messages were repeated %2 times total").arg(3).arg(matches3);
			last_repeat_count = matches3;
		} else {
			show_msg = false;
		}
// 	} else if (matches4 > max_shown_matches) {
// 		if (matches4 % match_step == 0 && matches4 != last_repeat_count) {
// 			msg = QObject::tr("- The last %1 messages were repeated %2 times total").arg(4).arg(matches4);
// 			last_repeat_count = matches4;
// 		} else {
// 			show_msg = false;
// 		}
	} else {
		last_repeat_count = 0;
	}

	// TODO If a new message arrives, print the remainder repeated messages.

	if (!show_msg) {
		return QString();
	}

	if (!markup_output) {
		return msg;
	}

	QString formatted = msg.toHtmlEscaped().replace(QStringLiteral("\n"), QStringLiteral("<br>"))
			.replace(QStringLiteral(" "), QStringLiteral("&#160;"));  // newline and nbsp

	QString style_str = QStringLiteral(" style=\"color: ") + color + QStringLiteral("\"");
	return QStringLiteral("<div") + style_str + QStringLiteral(">") + formatted + QStringLiteral("</div>");
}





#endif  // hg

