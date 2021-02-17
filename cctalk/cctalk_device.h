/**************************************************************************
Copyright: (C) 2014 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#ifndef CCTALK_DEVICE_H
#define CCTALK_DEVICE_H

#include <QMap>
#include <QTimer>
#include <QTime>
#include <functional>
#include <memory>

#include "helpers/async_serializer.h"
#include "cctalk_enums.h"
#include "cctalk_link_controller.h"



namespace qtcc {



/// Device state.
/// Additional commands that may change the state:
/// - ResetDevice
enum class CcDeviceState {

	/// Initial state, device not probed yet, or shut down.
	/// Switching to this mode stops the timer / polling.
	ShutDown,

	/// The state is set automatically when the device fails to respond
	/// when entering Initialized stage, or after a soft reset.
	/// The device should be checked for Alive continuously and if alive, initialized.
	UninitializedDown,

	/// Switching from ShutDown, ExternalReset, UninitializedDown this is:
	/// - probe the device using SimplePoll
	/// - read device manufacturing info (category, serial, manufacturer, ...)
	/// - read device recommended polling frequency
	/// - initialize coin / bill IDs (including country scaling)
	/// - enable stacker and escrow for bill validators
	/// - set inhibit status off on all bills (but not coins!).
	/// If the device doesn't respond to SimplePoll, UninitializedDown state is entered.
	/// If any of the other initialization errors occur, InitializationFailed state is entered.
	/// Switching to this mode starts the timer / polling.
	Initialized,

	/// This can be set only during switching to Initialized.
	/// The device didn't respond to probing or other initialization requests. Device cannot be used.
	InitializationFailed,

	/// This sets master inhibit status off.
	/// The event table is polled continuously.
	NormalAccepting,

	/// This sets master inhibit status on.
	/// The event table is polled continuously.
	/// Note that if the device sets master inhibit to on due to polling timeout,
	/// this state is set (only for bill validators, since coin acceptors automatically
	/// disable master inhibit if the polling resumes after a timeout).
	NormalRejecting,

	/// The device is polling the diagnostics log. This state may be caused by
	/// an error condition in the event table. Once the error is resolved, the
	/// device switches to NormalRejecting state.
	DiagnosticsPolling,

	/// This state is set automatically if the device is found to be down during
	/// normal operation.
	/// Do NOT reset the device, since the event log (with the credits) will be lost.
	/// The device should be assumed to be just like in ShutDown state.
	UnexpectedDown,

	/// This state is set automatically if an external reset of the device is detected.
	/// The device should be assumed to be just like in ShutDown state.
	ExternalReset,
};


/// Get displayable name
inline QString ccDeviceStateGetDisplayableName(CcDeviceState status)
{
	static QMap<CcDeviceState, QString> name_map = {
		{CcDeviceState::ShutDown, QObject::tr("ShutDown")},
		{CcDeviceState::UninitializedDown, QObject::tr("UninitializedDown")},
		{CcDeviceState::Initialized, QObject::tr("Initialized")},
		{CcDeviceState::InitializationFailed, QObject::tr("InitializationFailed")},
		{CcDeviceState::NormalAccepting, QObject::tr("NormalAccepting")},
		{CcDeviceState::NormalRejecting, QObject::tr("NormalRejecting")},
		{CcDeviceState::DiagnosticsPolling, QObject::tr("DiagnosticsPolling")},
		{CcDeviceState::UnexpectedDown, QObject::tr("UnexpectedDown")},
		{CcDeviceState::ExternalReset, QObject::tr("ExternalReset")},
	};
	return name_map.value(status, QString());
}





/// ccTalk device. This class contains high-level functions to manipulate
/// ccTalk devices. The actual messaging protocol is implemented in the
/// controller-thread-managing CctalkLinkController class.
class CctalkDevice : public QObject {
	Q_OBJECT
	public:

		using BillValidatorFunc = std::function<bool(quint8 bill_id, const CcIdentifier& identifier)>;

		/// Constructor
		CctalkDevice();


		/// Get link controller
		CctalkLinkController& getLinkController();


		/// This function is called in NormalAccepting state when a bill is inserted and
		/// should be checked for validity by us.
		/// If the function returns true, the bill is accepted.
		void setBillValidationFunction(BillValidatorFunc validator);


		/// Request initializing the device from ShutDown state.
		/// Starts event timer.
		/// \return true if the request was successfully sent.
		bool initialize(const std::function<void(const QString& error_msg)>& finish_callback);

		/// Request the device to be switched to ShutDown state.
		/// Stops event timer.
		/// \return true if the request was successfully sent.
		bool shutdown(const std::function<void(const QString& error_msg)>& finish_callback);


	protected:

		/// Start event-handling timer
		void startTimer();

		/// Stop event-handling timer
		void stopTimer();


	signals:

		/// Emitted whenever device state is changed.
		void deviceStateChanged(CcDeviceState old_state, CcDeviceState new_state);

		/// Emitted whenever a credit is accepted.
		void creditAccepted(quint8 id, CcIdentifier identifier);

		/// Emitted whenever cctalk message data cannot be decoded (logic error)
		void ccResponseDataDecodeError(quint64 request_id, const QString& error_msg);

		/// Mirrored from CctalkLinkController and expanded with local events.
		void logMessage(const QString& msg);


	protected slots:

		/// Event handling function (timer callback)
		void timerIteration();


	public:

		/// Request switching the device state.
		/// \return true if the request was successfully sent.
		bool requestSwitchDeviceState(CcDeviceState state, const std::function<void(const QString& error_msg)>& finish_callback);


	protected:

		/// Switch to Initialized state from ShutDown state.
		/// If the switch request fails, the devices is switched to InitializationFailed state.
		/// This sets the stored member variables.
		/// \return true if preconditions were acceptable and the switch has been initiated.
		bool switchStateInitialized(const std::function<void(const QString& error_msg)>& finish_callback);

		/// Switch to NormalAccepting state.
		bool switchStateNormalAccepting(const std::function<void(const QString& error_msg)>& finish_callback);

		/// Switch to NormalRejecting state.
		bool switchStateNormalRejecting(const std::function<void(const QString& error_msg)>& finish_callback);

		/// Switch to DiagnosticsPolling state.
		bool switchStateDiagnosticsPolling(const std::function<void(const QString& error_msg)>& finish_callback);

		/// Switch to ShutDown state.
		bool switchStateShutDown(const std::function<void(const QString& error_msg)>& finish_callback);


		/// Send SimplePoll and return for ACK.
		void requestCheckAlive(const std::function<void(const QString& error_msg, bool alive)>& finish_callback);

		/// Request manufacturing information info from the device.
		/// This includes category, serial number, manufacturer, ...
		void requestManufacturingInfo(const std::function<void(const QString& error_msg, CcCategory category, const QString& info)>& finish_callback);

		/// Get device-recommended polling interval in ms.
		void requestPollingInterval(const std::function<void(const QString& error_msg, quint64 msec)>& finish_callback);

		/// Request inhibit status modification. This is needed to enable coin/bill acceptance.
		void requestSetInhibitStatus(quint8 accept_mask1, quint8 accept_mask2, const std::function<void(const QString& error_msg)>& finish_callback);

		/// Request master inhibit status modification. This is needed to enable coin/bill acceptance.
		void requestSetMasterInhibitStatus(bool inhibit, const std::function<void(const QString& error_msg)>& finish_callback);

		/// Request master inhibit status retrieval,
		void requestMasterInhibitStatus(const std::function<void(const QString& error_msg, bool inhibit)>& finish_callback);

		/// Request bill validator operating mode modification.
		void requestSetBillOperatingMode(bool use_stacker, bool use_escrow, const std::function<void(const QString& error_msg)>& finish_callback);

		/// Request coin / bill identifiers (quantity for bills, bill/coin names) and country scaling data (bills).
		void requestIdentifiers(const std::function<void(const QString& error_msg, const QMap<quint8, CcIdentifier>& identifiers)>& finish_callback);

		/// Request buffered credit (coins / bills) events or error events using ReadBufferedBillEvents
		/// or ReadBufferedCredit commands. This function should be executed repeatedly when polling.
		/// \c event_data contains the event log from newest (at offset 0) to oldest (at offset 4).
		/// A command timeout here is not an error condition - an empty event counter / log and
		// an empty error message are returned. The caller should ignore this and continue normally.
		void requestBufferedCreditEvents(const std::function<void(const QString& error_msg,
				quint8 event_counter, const QVector<CcEventData>& event_data)>& finish_callback);

		/// Process the credit/event log. This is used by timerIteration().
		void processCreditEventLog(bool accepting, const QString& event_log_cmd_error_msg, quint8 event_counter,
				const QVector<CcEventData>& event_data, const std::function<void()>& finish_callback);

		/// Route a bill that is held in escrow.
		void requestRouteBill(CcBillRouteCommandType route,
				const std::function<void(const QString& error_msg, CcBillRouteStatus status)>& finish_callback);

		/// Request self-check (diagnostics mode). This function should be executed repeatedly
		/// when polling in diagnostics mode.
		void requestSelfCheck(const std::function<void(const QString& error_msg, CcFaultCode fault_code)>& finish_callback);

		/// Request soft reset. Finish callback is called when the device accepts the reset command.
		void requestResetDevice(const std::function<void(const QString& error_msg)>& finish_callback);

		/// Call requestResetDevice() and set the state to UninitializedDown.
		void requestResetDeviceWithState(const std::function<void(const QString& error_msg)>& finish_callback);


	public:

		/// Get device status as set by the latest status-updating function
		[[nodiscard]] CcDeviceState getDeviceState() const;


	protected:

		/// Set the device status. Emits deviceStateChanged() if changed.
		void setDeviceState(CcDeviceState state);


	public:

		/// Get requestManufacturingInfo() category result.
		[[nodiscard]] CcCategory getStoredDeviceCategory() const;

		/// Get requestManufacturingInfo() free-form string result.
		[[nodiscard]] QString getStoredManufacturingInfo() const;

		/// Get detectPollingInterval() result
		[[nodiscard]] int getStoredPollingInterval() const;

		/// Get requestIdentifiers() result
		[[nodiscard]] QMap<quint8, CcIdentifier> getStoredIndentifiers() const;


	private:

		CctalkLinkController link_controller_;  ///< Controller for serial worker thread with cctalk link management support.

		int normal_polling_interval_msec_ = 0;  ///< Polling interval for normal and diagnostics modes.
		const int default_normal_polling_interval_msec_ = 100;  ///< Default polling interval for normal and diagnostics modes.
		const int not_alive_polling_interval_msec_ = 1000;  ///< Polling interval for modes when the device doesn't respond to alive check.

		QTimer event_timer_;  ///< Polling timer
		bool timer_iteration_task_running_ = false;  ///< Avoids parallel executions of state change, since it's asynchronous

		CcDeviceState device_state_ = CcDeviceState::ShutDown;  ///< Current status

		BillValidatorFunc bill_validator_func_;  ///< Bill validator function, which tells us to accept or reject a certain bill.

// 		QTimer reset_timer_;  ///< Timer that waits for the device to get back up after SoftReset
// 		QTime last_reset_time_;  ///< Last time the device was SoftReset

		CcCategory device_category_ = CcCategory::Unknown;  ///< Equipment category
		QString manufacturing_info_;  ///< Free-form text product information (manufacturer, serial number, etc...)

		QMap<quint8, CcIdentifier> identifiers_;  ///< Coin positions / bill types and IDs (names)
// 		bool req_identifiers_on_init_ = true;  ///< Request identifiers in requestInitialization().

		bool event_log_read_ = false;  ///< True if the event log was read at least once.
		quint8 last_event_num_ = 0;  ///< Last event number returned by ReadBufferedCredit command.

};



}


#endif
