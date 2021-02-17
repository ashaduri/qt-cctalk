/**************************************************************************
Copyright: (C) 2014 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#include <memory>

#include "cctalk_link_controller.h"
#include "helpers/debug.h"
#include "serial_worker.h"


namespace qtcc {




CctalkLinkController::CctalkLinkController()
{
	serial_worker_.reset(new SerialWorker());

	serial_worker_->moveToThread(&worker_thread_);
	connect(&worker_thread_, &QThread::finished, serial_worker_.data(), &QObject::deleteLater);

	// Connect our proxy signals to their slots.
	connect(this, &CctalkLinkController::openPortInWorker, serial_worker_.data(), &SerialWorker::openPort, Qt::QueuedConnection);
	connect(this, &CctalkLinkController::closePortInWorker, serial_worker_.data(), &SerialWorker::closePort, Qt::QueuedConnection);
	connect(this, &CctalkLinkController::sendRequestToWorker, serial_worker_.data(), &SerialWorker::sendRequest, Qt::QueuedConnection);

	// Handle their signals
	connect(serial_worker_.data(), &SerialWorker::responseReceived, this, &CctalkLinkController::onResponseReceive, Qt::QueuedConnection);

	// Mirror some signals
	connect(serial_worker_.data(), &SerialWorker::portError, this, &CctalkLinkController::portError, Qt::QueuedConnection);
	connect(serial_worker_.data(), &SerialWorker::portOpen, this, &CctalkLinkController::portOpen, Qt::QueuedConnection);
// 	connect(serial_worker_.data(), &SerialWorker::requestTimeout, this, &CctalkLinkController::requestTimeout, Qt::QueuedConnection);
// 	connect(serial_worker_.data(), &SerialWorker::responseTimeout, this, &CctalkLinkController::responseTimeout, Qt::QueuedConnection);
	connect(serial_worker_.data(), &SerialWorker::logMessage, this, &CctalkLinkController::logMessage, Qt::QueuedConnection);

	// Log message structure errors
	connect(this, &CctalkLinkController::ccResponseMessageStructureError, [this]([[maybe_unused]] quint64 request_id,
			const QString& error_msg) {
		emit logMessage(error_msg);
	});


	// requestFinishedOrError signal, which signals the executeOnReturn() function to call its callback.

	connect(this, &CctalkLinkController::ccResponseReply, [this](quint64 request_id, const QByteArray& command_data) {
		emit requestFinishedOrError(request_id, QString(), command_data);
	});

	connect(serial_worker_.data(), &SerialWorker::portError, [this](const QString& error_msg) {
		emit requestFinishedOrError(0, error_msg, QByteArray());
	});

	connect(serial_worker_.data(), &SerialWorker::requestTimeout, [this](quint64 request_id) {
		emit requestFinishedOrError(request_id, QObject::tr("Request #%1 write timeout").arg(request_id), QByteArray());
	});

	connect(serial_worker_.data(), &SerialWorker::responseTimeout, [this](quint64 request_id) {
		emit requestFinishedOrError(request_id, QObject::tr("Response #%1 read timeout").arg(request_id), QByteArray());
	});

	connect(this, &CctalkLinkController::ccResponseMessageStructureError, [this](quint64 request_id, const QString& error_msg) {
		emit requestFinishedOrError(request_id, error_msg, QByteArray());
	});


	// Start the thread (calls run(), enters event loop).
	worker_thread_.start();
}



CctalkLinkController::~CctalkLinkController()
{
	worker_thread_.quit();
	worker_thread_.wait();
}



void CctalkLinkController::setCcTalkOptions(const QString& port_device, quint8 device_addr, bool checksum_16bit, bool des_encrypted)
{
	port_device_ = port_device;
	device_addr_ = device_addr;
	checksum_16bit_ = checksum_16bit;
	des_encrypted_ = des_encrypted;
}



void CctalkLinkController::setLoggingOptions(bool show_full_response, bool show_serial_request, bool show_serial_response,
		bool show_cctalk_request, bool show_cctalk_response)
{
	serial_worker_->setLoggingOptions(show_full_response, show_serial_request, show_serial_response);
	show_cctalk_request_ = show_cctalk_request;
	show_cctalk_response_ = show_cctalk_response;
}



void CctalkLinkController::openPort(const std::function<void(const QString& error_msg)>& finish_callback)
{
	emit openPortInWorker(port_device_);  // Queued
	connect(serial_worker_.data(), &SerialWorker::portError, [=](const QString& error_msg) {
		finish_callback(error_msg);
	});
	connect(serial_worker_.data(), &SerialWorker::portOpen, [=]() {
		finish_callback(QString());
	});
}



void CctalkLinkController::closePort()
{
	emit closePortInWorker();
}



quint64 CctalkLinkController::ccRequest(CcHeader command, const QByteArray& data, int response_timeout_msec)
{
	DBG_ASSERT(data.size() <= 255);

// 	if (device_status_ == CcDeviceState::Dead && command != CcHeader::SimplePoll) {
// 		emit logMessage(tr("Command (%1) not sent to a dead device.").arg(int(command)));
// 		return 0;
// 	}
	if (des_encrypted_) {
		emit logMessage(tr("! ccTalk encryption requested, unsupported. Aborting request."));
		return 0;
	}
	if (checksum_16bit_) {
		// TODO support this
		emit logMessage(tr("! ccTalk 16-bit CRC checksums requested, unsupported. Aborting request."));
		return 0;
	}

	if (show_cctalk_request_) {
		emit logMessage(tr("> ccTalk request: %1, address: %2, data: %3").arg(ccHeaderGetDisplayableName(command))
				.arg(int(device_addr_))
				.arg(data.isEmpty() ? tr("(empty)") : QString::fromLatin1(data.toHex())));
	}

	// Note: The request and response messages have the same format (with source/dest addresses swapped).
	QByteArray request_data;
	request_data.append(char(device_addr_));  // Field: Destination address. 0 means broadcast to all devices on port.
	request_data.append(char(data.size()));  // Field: Data size
	request_data.append(char(controller_addr_));  // Field: Source address. 1 means master.
	request_data.append(char(command));  // Field: Command
	request_data.append(data);  // Field: The data (if data size != 0)

	quint8 checksum = 0;
	for (char c : request_data) {
		checksum = static_cast<quint8>(checksum + c);
	}
	checksum = static_cast<quint8>(256 - checksum);
	request_data.append(char(checksum));  // Field: Checksum

	quint64 request_id = ++req_num_;
	if (request_id == 0) {
		request_id = ++req_num_;
	}

	const bool response_contains_request = true;  // due to local loopback of serial port.
	const int write_timeout_msec = 500 + request_data.size() * 2;  // should be more than enough at 9600 baud.

	// The actual request will be sent on the next main loop iteration, when it reaches the serial worker.
	// This means that we can safely connect to response / error signals right after this function.
	emit sendRequestToWorker(request_id, request_data, response_contains_request, write_timeout_msec, response_timeout_msec);

	return request_id;
}



void CctalkLinkController::executeOnReturn(quint64 sent_request_id,
		const std::function<void(quint64 request_id, const QString& error_msg, const QByteArray& command_data)>& callback) const
{
	if (sent_request_id == 0) {  // nothing was sent
		return;
	}
	auto conn = std::make_shared<QMetaObject::Connection>();

	*conn = connect(this, &CctalkLinkController::requestFinishedOrError,
			[=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable
	{
		if (sent_request_id != request_id) {  // should not happen, since we won't have two active requests at the same time, but just in case...
			return;
		}
		QObject::disconnect(*conn);  // it's a one-time slot

		callback(request_id, error_msg, command_data);
	});
}



void CctalkLinkController::onResponseReceive(quint64 request_id, const QByteArray& response_data)
{
	if (response_data.size() < 5) {
		emit ccResponseMessageStructureError(request_id, QObject::tr("! ccTalk response #%1 size too small (%2 bytes).")
				.arg(request_id).arg(response_data.size()));
		// TODO The command should be retried.
		return;
	}

	quint8 destination_addr = response_data[0];
	quint8 data_size = response_data[1];
	quint8 source_addr = response_data[2];
	quint8 command = response_data[3];
	QByteArray command_data = response_data.mid(4, data_size);
	// quint8 received_checksum = response_data[response_data.size() - 1];

	// Format error
	if (response_data.size() != 5 + command_data.size()) {
		emit ccResponseMessageStructureError(request_id, QObject::tr("! Invalid ccTalk response #%1 size (%2 bytes).")
				.arg(request_id).arg(response_data.size()));
		// TODO The command should be retried.
		return;
	}

	// Checksum error
	{
		quint8 checksum = 0;
		for (char c : response_data) {
			checksum = static_cast<quint8>(checksum + c);
		}
		// The sum of all bytes must be 0.
		if (checksum != 0) {
			emit ccResponseMessageStructureError(request_id, QObject::tr("! Invalid ccTalk response #%1 checksum.").arg(request_id));
			// TODO The command should be retried.
			return;
		}
	}

	// We should be the only destination. In multi-host networks this should be
	// ignored, but not here.
	if (destination_addr != 0x01) {
		emit ccResponseMessageStructureError(request_id, QObject::tr("! Invalid ccTalk response #%1 destination address %2.")
				.arg(request_id).arg(int(destination_addr)));
		return;
	}

	// We should be the only destination. In multi-host networks this should be
	// ignored, but not here.
	if (device_addr_ != 0 && source_addr != device_addr_) {
		emit ccResponseMessageStructureError(request_id, QObject::tr("! Invalid ccTalk response #%1 source address %2, expected %3.")
				.arg(request_id).arg(int(source_addr)).arg(int(device_addr_)));
		return;
	}

	QString formatted_data = command_data.isEmpty() ? tr("(empty)") : QString::fromLatin1(command_data.toHex());

	// Every reply must have the command field set to 0.
	if (command != static_cast<decltype(command)>(CcHeader::Reply)) {
		emit ccResponseMessageStructureError(request_id,
				QObject::tr("! Invalid ccTalk response #%1 from address %2: Command is %3, expected 0.")
				.arg(request_id).arg(int(source_addr)).arg(int(command)));
		return;
	}

// 	if (command == static_cast<decltype(command)>(CcHeader::Reply)) {
// 		emit logMessage(QObject::tr("< ccTalk response #%1 data: %2")
// 				.arg(request_id).arg(formatted_data));
		if (show_cctalk_response_) {
			// Don't print response_id, it interferes with identical message hiding.
			emit logMessage(QObject::tr("< ccTalk response from address %1, data: %2")
					.arg(int(source_addr)).arg(formatted_data));
		}
		emit ccResponseReply(request_id, command_data);
// 	} else {
// 		QString command_name = ccHeaderGetDisplayableName(CcHeader(command));
// 		if (command_name.isEmpty()) {
// 			command_name = QString::number(int(command));
// 		}
// // 		emit logMessage(QObject::tr("< ccTalk response #%1 command: %2; data: %3")
// // 				.arg(request_id).arg(command_name).arg(formatted_data));
// 		// Don't print response_id, it interferes with identical message hiding.
// 		emit logMessage(QObject::tr("< ccTalk response: %1, address: %2, data: %3")
// 				.arg(command_name).arg(int(source_addr)).arg(formatted_data));
// 		emit ccResponseWithCommand(request_id, command, command_data);
// 	}
}





}

