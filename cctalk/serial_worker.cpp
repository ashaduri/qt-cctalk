/**************************************************************************
Copyright: (C) 2014 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#include "serial_worker.h"


namespace qtcc {



SerialWorker::SerialWorker()
{
	connect(this, &SerialWorker::portError, [this](const QString& error_msg) {
		emit logMessage(tr("! Serial port %1 error: %2").arg(serial_port_ ? serial_port_->portName() : tr("[unknown]")).arg(error_msg));
	});
}



void SerialWorker::setLoggingOptions(bool show_full_response, bool show_serial_request, bool show_serial_response)
{
	show_full_response_ = show_full_response;
	show_serial_request_ = show_serial_request;
	show_serial_response_ = show_serial_response;
}



void SerialWorker::openPort(const QString& port_name)
{
	if (!serial_port_) {
		serial_port_.reset(new QSerialPort());
	}

	if (serial_port_->isOpen()) {
		closePort();
	}

	serial_port_->setPortName(port_name);

	emit logMessage(tr("* Opening port \"%1\".").arg(port_name));

	if (!serial_port_->open(QIODevice::ReadWrite)) {
		emit portError(tr("Can't open port %1: %2").arg(serial_port_->portName()).arg(serial_port_->errorString()));
		return;
	}

	// cctalk uses 9600 by default, but can use 115200 over usb
	if (!serial_port_->setBaudRate(QSerialPort::Baud9600)) {
		emit portError(tr("Can't set baud rate on port %1, error code %2").arg(serial_port_->portName()).arg(int(serial_port_->error())));
		return;
	}

	// TODO Start bits? cctalk wants 1 start bit.

	if (!serial_port_->setDataBits(QSerialPort::Data8)) {
		emit portError(tr("Can't set 8 data bits on port %1, error code %2").arg(serial_port_->portName()).arg(int(serial_port_->error())));
		return;
	}

	if (!serial_port_->setParity(QSerialPort::NoParity)) {
		emit portError(tr("Can't set no parity on port %1, error code %2").arg(serial_port_->portName()).arg(int(serial_port_->error())));
		return;
	}

	if (!serial_port_->setStopBits(QSerialPort::OneStop)) {
		emit portError(tr("Can't set 1 stop bit on port %1, error code %2").arg(serial_port_->portName()).arg(int(serial_port_->error())));
		return;
	}

	if (!serial_port_->setFlowControl(QSerialPort::NoFlowControl)) {
		emit portError(tr("Can't set no flow control on port %1, error code %2").arg(serial_port_->portName()).arg(int(serial_port_->error())));
		return;
	}

	emit logMessage(tr("* Port \"%1\" opened.").arg(port_name));

	emit portOpen();
}



void SerialWorker::closePort()
{
	if (serial_port_) {
		emit logMessage(tr("* Port \"%1\" closed.").arg(serial_port_->portName()));
		serial_port_->close();
	}
}



void SerialWorker::sendRequest(quint64 request_id, const QByteArray& request_data,
		bool request_needs_response, int write_timeout_msec, int response_timeout_msec)
{
	// Keep in mind:
	// At 9600 baud, each byte transmitted or received takes 1.042ms.

	if (show_serial_request_) {
		emit logMessage(QObject::tr("> Request: %2").arg(QString::fromLatin1(request_data.toHex())));
	}

	// NOTE Retrying is implemented on ccTalk level, no need to do it here.

// 	int left_retries = request_max_retries_;

	// Write request
// 	while (left_retries--) {
		serial_port_->write(request_data);

		if (serial_port_->waitForBytesWritten(write_timeout_msec)) {
			emit requestWritten(request_id);

			if (request_needs_response) {
				// Read response
				if (serial_port_->waitForReadyRead(response_timeout_msec)) {  // first read
					QByteArray response_data = serial_port_->readAll();
					while (serial_port_->waitForReadyRead(50)) {  // subsequent data chunks. ccTalk recommends using 50ms.
						response_data += serial_port_->readAll();
					}
					if (response_contains_request_) {
						if (show_full_response_) {
							emit logMessage(QObject::tr("< Full response: %1").arg(QString::fromLatin1(response_data.toHex())));
						}
						response_data = response_data.right(response_data.size() - request_data.size());
					}
					if (show_serial_response_) {
						emit logMessage(QObject::tr("< Response: %1").arg(QString::fromLatin1(response_data.toHex())));
					}
					emit responseReceived(request_id, response_data);
					return;  // all done

// 				} else if (left_retries > 0) {
// 					emit logMessage(QObject::tr("> Retrying request #%1 (try #%2)").arg(request_id).arg(request_max_retries_ - left_retries));
// 					emit requestRetry(request_id);
				} else {
					emit logMessage(QObject::tr("!< Response #%1 read timeout (%2ms)").arg(request_id).arg(response_timeout_msec));
					emit responseTimeout(request_id);
				}
			} else {
				return;  // all done, one try only
			}
		} else {
			emit logMessage(QObject::tr("!> Request #%1 write timeout (%2ms)").arg(request_id).arg(write_timeout_msec));
			emit requestTimeout(request_id);
		}
// 	}
}



}

