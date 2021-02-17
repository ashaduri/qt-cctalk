/**************************************************************************
Copyright: (C) 2014 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#ifndef SERIAL_WORKER_H
#define SERIAL_WORKER_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QSerialPort>



namespace qtcc {



/// Serial port communication handler.
/// Note that except construction, all of the code is executed in the worker thread.
class SerialWorker : public QObject {
	Q_OBJECT
	public:

		/// Constructor
		SerialWorker();

		/// Set logging options for logMessage() signal.
		void setLoggingOptions(bool show_full_response, bool show_serial_request, bool show_serial_response);


	public slots:

		// NOTE These slots may only be called through queued connections from the controller thread.

		/// Open the serial port (e.g. /dev/ttyUSB0).
		/// If the port is already open, it is closed first, then reopened.
		void openPort(const QString& port_name);

		/// Close the serial port.
		void closePort();

		/// Send request to serial port and listen to response if needed.
		void sendRequest(quint64 request_id, const QByteArray& request_data,
				bool request_needs_response, int write_timeout_msec, int response_timeout_msec);


	signals:

		/// Emitted on serial port error
		void portError(const QString& error_msg);

		/// Emitted when a serial port is opened
		void portOpen();


		/// Emitted when the request data is fully written to the port
		void requestWritten(quint64 request_id);

		/// Emitted whenever response is received
		void responseReceived(quint64 request_id, const QByteArray& response_data);


		/// Emitted on request write timeout
		void requestTimeout(quint64 request_id);

		/// Emitted on response final timeout (after request retries)
		void responseTimeout(quint64 request_id);


		/// Emitted each time the request is retried
// 		void requestRetry(quint64 request_id);


		/// This can be used to log requests and responses.
		/// All errors and timeouts are sent here as well.
		void logMessage(const QString& msg);


	private:

		QScopedPointer<QSerialPort> serial_port_;  ///< Serial port

		/// If true, the response from serial port comes with request prepended to it (e.g. cctalk), remove it.
		/// This is due to bi-directional nature of the data line.
		bool response_contains_request_ = true;

		bool show_full_response_ = false;
		bool show_serial_request_ = false;
		bool show_serial_response_ = false;

// 		int request_max_retries_ = 3;  ///< If, while waiting for response, timeout was reached, re-send the request.

};


}



#endif

