/**************************************************************************
Copyright: (C) 2014 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#ifndef CCTALK_LINK_CONTROLLER_H
#define CCTALK_LINK_CONTROLLER_H

#include <QThread>
#include <functional>

#include "cctalk_enums.h"


namespace qtcc {



class SerialWorker;


/**
\file

How it all works:

SerialWorker runs in its own thread (launched by CctalkLinkController) and
performs blocking operations on a serial port. CctalkLinkController communicates
with it using a queued signal/slot mechanism only. To transmit requests,
CctalkLinkController emits signals that are tied to SerialWorker slots.
The receiving of results, errors, timeout conditions and log messages
happens by connecting SerialWorker signals to CctalkLinkController slots.
*/


/// ccTalk protocol implementation, controller for the serial worker thread.
class CctalkLinkController : public QObject {
	Q_OBJECT
	public:

		/// void callback()
		using ResponseAckFunc = std::function<void()>;

		/// void callback(const QByteArray& command_data)
		using ResponseGenericReplyFunc = std::function<void(const QByteArray& command_data)>;

		/// void callback(quint8 command, const QByteArray& command_data)
		using ResponseWithCommandFunc = std::function<void(quint8 command, const QByteArray& command_data)>;


		/// Constructor
		CctalkLinkController();

		/// Destructor
		virtual ~CctalkLinkController();


		/// Set ccTalk options. Call before opening the device.
		void setCcTalkOptions(const QString& port_device, quint8 device_addr, bool checksum_16bit, bool des_encrypted);

		/// Set logging options (though logMessage() signal). Call before opening the device.
		void setLoggingOptions(bool show_full_response, bool show_serial_request, bool show_serial_response,
				bool show_cctalk_request, bool show_cctalk_response);

		/// Open the serial port.
		void openPort(std::function<void(const QString& error_msg)> finish_callback);

		/// Close the serial port.
		void closePort();


	// Public signals
	signals:

		/// Emitted whenever a cctalk generic reply is received. If the data size is 0,
		/// the message should be treated as ACK.
		void ccResponseReply(quint64 request_id, const QByteArray& command_data);

		/// Emitted whenever cctalk message parsing fails (on general level; the actual
		/// logical errors are checked in individual CctalkDevice functions)
		void ccResponseMessageStructureError(quint64 request_id, const QString& error_msg);


		/// Mirrored from SerialWorker
		void portError(const QString& error_msg);

		/// Mirrored from SerialWorker
		void portOpen();

// 		/// Mirrored from SerialWorker
// 		void requestTimeout(quint64 request_id);
//
// 		/// Mirrored from SerialWorker
// 		void responseTimeout(quint64 request_id);


		/// This signals the executeOnReturn() function to call its callback.
		void requestFinishedOrError(quint64 request_id, const QString& error_msg, const QByteArray& command_data);

		/// Mirrored from SerialWorker and expanded with local events.
		void logMessage(const QString& msg);


	// SerialWorker caller signals. There are emitted internally to call SerialWorker slots.
	// For each slot in SerialWorker there should be a signal here.

	// Private signals
	signals:

		/// Proxy signal to call a slot in the worker thread.
		void openPortInWorker(const QString& port_name);

		/// Proxy signal to call a slot in the worker thread.
		void closePortInWorker();

		/// Proxy signal to call a slot in the worker thread.
		void sendRequestToWorker(quint64 request_id, const QByteArray& request_data,
				bool request_needs_response, int write_timeout_msec, int response_timeout_msec);


	public:

		/// Send request to serial port.
		/// The returned value is request ID which can be used to identify which
		/// response comes from which request.
		quint64 ccRequest(CcHeader command, const QByteArray& data, int response_timeout_msec = 1500);

		/// A helper function for writing response handlers.
		void executeOnReturn(quint64 sent_request_id,
				std::function<void(quint64 request_id, const QString& error_msg, const QByteArray& command_data)> callback);


	protected slots:

		/// Handle generic serial response and emit ccResponse
		void onResponseReceive(quint64 request_id, const QByteArray& response_data);


	private:

		QScopedPointer<SerialWorker> serial_worker_;  ///< Serial port worker, lives in worker_thread_.
		QThread worker_thread_;  ///< The thread that SerialWorker lives in

		QString port_device_;  ///< Serial port device, e.g. /dev/ttyUSB0
		quint8 device_addr_ = 0x00;  ///< ccTalk destination address, used to differentiate different devices on the same serial bus. 0 for all.
		quint8 controller_addr_ = 0x01;  ///< Controller address. 1 means "Master". There is no reason to change this.
		bool checksum_16bit_ = false;  /// If true, use 16-bit CRC checksum. Otherwise use simple 8-bit checksum. The device must be set to the same value.
		bool des_encrypted_ = false;  ///< If true, use DES encryption. The device must be set to the same value. NOTE: Unsupported.

		quint64 req_num_ = 0;  ///< Request number. This is used to identify which response came from which request.

		bool show_cctalk_request_ = true;
		bool show_cctalk_response_ = true;

};



}


#endif

