/**************************************************************************
Copyright: (C) 2014 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#include <memory>
#include <QStringList>

#include "cctalk_device.h"
#include "helpers/debug.h"
#include "helpers/async_serializer.h"


namespace qtcc {


CctalkDevice::CctalkDevice()
{
	// Set up log message passthrough from link controller to us.
	connect(&link_controller_, &CctalkLinkController::logMessage, this, &CctalkDevice::logMessage);

	// Log data decode errors
	connect(this, &CctalkDevice::ccResponseDataDecodeError, [this](quint64 request_id, const QString& error_msg) {
		emit logMessage(error_msg);
	});

	// The polling interval may change with the state.
	event_timer_.setInterval(not_alive_polling_interval_msec_);

	connect(&event_timer_, &QTimer::timeout, this, &CctalkDevice::timerIteration);
}



CctalkLinkController& CctalkDevice::getLinkController()
{
	return link_controller_;
}



void CctalkDevice::setBillValidationFunction(BillValidatorFunc validator)
{
	bill_validator_func_ = validator;
}



bool CctalkDevice::initialize(std::function<void(const QString& error_msg)> finish_callback)
{
	if (getDeviceState() != CcDeviceState::ShutDown) {
		logMessage(tr("! Cannot initialize device that is in %1 state.").arg(ccDeviceStateGetDisplayableName(getDeviceState())));
		return false;
	}

	return requestSwitchDeviceState(CcDeviceState::Initialized, [=](const QString& error_msg) {
		finish_callback(error_msg);
	});
}



bool CctalkDevice::shutdown(std::function<void(const QString& error_msg)> finish_callback)
{
	return requestSwitchDeviceState(CcDeviceState::ShutDown, [=](const QString& error_msg) {
		finish_callback(error_msg);
	});
}



void CctalkDevice::startTimer()
{
	emit logMessage(tr("Starting poll timer."));

	event_timer_.start();

	// The timer will kick in only after a timeout, so do the first iteration manually.
	QTimer::singleShot(0, this, SLOT(timerIteration()));
}



void CctalkDevice::stopTimer()
{
	emit logMessage(tr("Stopping poll timer."));

	event_timer_.stop();
}



void CctalkDevice::timerIteration()
{
	if (timer_iteration_task_running_) {
		return;
	}
// 	emit logMessage(tr("Polling..."));

	// This is set to false in finish callbacks.
	timer_iteration_task_running_ = true;


	// The device is not initialized, do nothing and wait for request for state change to Initialized.
	if (getDeviceState() == CcDeviceState::ShutDown) {
		// Nothing
		timer_iteration_task_running_ = false;
		return;
	}

	// The device didn't respond to alive check and is assumed uninitialized,
	// see if it came back and if so, initialize it.
	if (getDeviceState() == CcDeviceState::UninitializedDown) {
		requestCheckAlive([=](const QString& error_msg, bool alive) {
			if (alive) {
				requestSwitchDeviceState(CcDeviceState::Initialized, [=](const QString& local_error_msg) {
					timer_iteration_task_running_ = false;
				});
			} else {
				timer_iteration_task_running_ = false;
			}
		});
		return;
	}

	// The device has been initialized, resume normal rejecting or diagnostics polling.
	// Default to bill / coin rejection.
	if (getDeviceState() == CcDeviceState::Initialized) {
		// Perform a self-check and see if everything's ok.
		requestSelfCheck([=](const QString& error_msg, CcFaultCode fault_code) {
			if (fault_code == CcFaultCode::Ok) {
				// The device is OK, resume normal rejecting mode.
				requestSwitchDeviceState(CcDeviceState::NormalRejecting, [=](const QString& local_error_msg) {
					timer_iteration_task_running_ = false;
				});
			} else {
				// The device is not ok, resume diagnostics polling mode.
				requestSwitchDeviceState(CcDeviceState::DiagnosticsPolling, [=](const QString& local_error_msg) {
					timer_iteration_task_running_ = false;
				});
			}
		});
		return;
	}

	// The device initialization failed, something wrong with it. Abort.
	if (getDeviceState() == CcDeviceState::InitializationFailed) {
		timer_iteration_task_running_ = false;
		// Nothing we can do, cannot work with this device.
		stopTimer();
		return;
	}

	// We're accepting the credit, process the credit / event log.
	if (getDeviceState() == CcDeviceState::NormalAccepting) {
		requestBufferedCreditEvents([=](const QString& error_msg, quint8 event_counter, const QVector<CcEventData>& event_data) {
			processCreditEventLog(true, error_msg, event_counter, event_data, [=]() {
				timer_iteration_task_running_ = false;
			});
		});
		return;
	}

	// We're rejecting the credit. Process the credit / event log in case of a fault occurring.
	if (getDeviceState() == CcDeviceState::NormalRejecting) {
		requestBufferedCreditEvents([=](const QString& error_msg, quint8 event_counter, const QVector<CcEventData>& event_data) {
			processCreditEventLog(false, error_msg, event_counter, event_data, [=]() {
				timer_iteration_task_running_ = false;
			});
		});
		return;
	}

	// If we're in diagnostics polling mode, poll the fault code until it's resolved,
	// then switch to rejecting mode.
	if (getDeviceState() == CcDeviceState::DiagnosticsPolling) {
		requestSelfCheck([=](const QString& error_msg, CcFaultCode fault_code) {
			if (fault_code == CcFaultCode::Ok) {
				// The error has been resolved, switch to rejecting mode.
				requestSwitchDeviceState(CcDeviceState::NormalRejecting, [=](const QString& state_error_msg) {
					timer_iteration_task_running_ = false;
				});
			} else {  // the fault is still there
				timer_iteration_task_running_ = false;
			}
		});
		return;
	}

	// The link was lost with the device. We should not do anything that may lead
	// to the loss of the event table (and, therefore, credit). Just re-initialize it, the
	// NormalRejecting state will be enabled and the event table will be read, if everything's ok.
	if (getDeviceState() == CcDeviceState::UnexpectedDown) {
		requestSwitchDeviceState(CcDeviceState::Initialized, [=](const QString& error_msg) {
			timer_iteration_task_running_ = false;
		});
		return;
	}

	// The device event log turned out to be empty (after being non-empty). This means
	// that the device was probably reset externally, with possible loss of credits.
	// Assume it needs initialization.
	if (getDeviceState() == CcDeviceState::ExternalReset) {
		requestSwitchDeviceState(CcDeviceState::Initialized, [=](const QString& error_msg) {
			timer_iteration_task_running_ = false;
		});
		return;
	}
}



bool CctalkDevice::requestSwitchDeviceState(CcDeviceState state, std::function<void(const QString& error_msg)> finish_callback)
{
	emit logMessage(tr("Requested device state change from %1 to: %2")
			.arg(ccDeviceStateGetDisplayableName(getDeviceState())).arg(ccDeviceStateGetDisplayableName(state)));

	if (device_state_ == state) {
		emit logMessage(tr("Cannot switch to device state %1, already there.").arg(ccDeviceStateGetDisplayableName(state)));
		return true;  // already there
	}


	switch(state) {
		case CcDeviceState::ShutDown:
		{
			bool success = switchStateShutDown(finish_callback);
			event_timer_.setInterval(normal_polling_interval_msec_);
			stopTimer();
			return success;
		}

		case CcDeviceState::UninitializedDown:
			setDeviceState(state);
			event_timer_.setInterval(not_alive_polling_interval_msec_);
			finish_callback(QString());
			return true;

		case CcDeviceState::Initialized:
		{
			bool success = switchStateInitialized(finish_callback);
			event_timer_.setInterval(normal_polling_interval_msec_);
			startTimer();
			return success;
		}

		case CcDeviceState::InitializationFailed:
			setDeviceState(state);
			event_timer_.setInterval(not_alive_polling_interval_msec_);
			finish_callback(QString());
			return true;

		case CcDeviceState::NormalAccepting:
			return switchStateNormalAccepting(finish_callback);

		case CcDeviceState::NormalRejecting:
			return switchStateNormalRejecting(finish_callback);

		case CcDeviceState::DiagnosticsPolling:
		{
			bool success = switchStateDiagnosticsPolling(finish_callback);
			event_timer_.setInterval(normal_polling_interval_msec_);
			return success;
		}

		case CcDeviceState::UnexpectedDown:
			setDeviceState(state);
			event_timer_.setInterval(not_alive_polling_interval_msec_);
			finish_callback(QString());
			return true;

		case CcDeviceState::ExternalReset:
			setDeviceState(state);
			event_timer_.setInterval(not_alive_polling_interval_msec_);
			finish_callback(QString());
			return true;
	}

	DBG_ASSERT(0);
	return false;
}



bool CctalkDevice::switchStateInitialized(std::function<void(const QString& error_msg)> finish_callback)
{
	DBG_ASSERT_RETURN(device_state_ == CcDeviceState::ShutDown || device_state_ == CcDeviceState::ExternalReset
			|| device_state_ == CcDeviceState::UnexpectedDown || device_state_ == CcDeviceState::UninitializedDown, false);

	auto shared_error = std::make_shared<QString>();
	auto shared_alive = std::make_shared<bool>();

	auto aser = new AsyncSerializer(  // auto-deleted
		// Finish callback
		[=](AsyncSerializer* serializer) {
			if (shared_error->isEmpty()) {
				setDeviceState(CcDeviceState::Initialized);
				finish_callback(*shared_error);
			} else if (*shared_alive) {
				requestSwitchDeviceState(CcDeviceState::InitializationFailed, finish_callback);
			} else {
				requestSwitchDeviceState(CcDeviceState::UninitializedDown, finish_callback);
			}
		}
	);

	// Check if it's present / alive
	aser->add([=](AsyncSerializer* serializer) {
		requestCheckAlive([=](const QString& error_msg, bool alive) {
			if (!error_msg.isEmpty()) {
				*shared_error = error_msg;
			}
			*shared_alive = alive;
			serializer->continueSequence(alive);
		});
	});

	// Get device manufacturing info
	aser->add([=](AsyncSerializer* serializer) {
		requestManufacturingInfo([=](const QString& error_msg, CcCategory category, const QString& info) {
			if (!error_msg.isEmpty()) {
				*shared_error = error_msg;
			} else {
				device_category_ = category;
				manufacturing_info_ = info;
			}
			serializer->continueSequence(error_msg.isEmpty() && (category == CcCategory::BillValidator || category == CcCategory::CoinAcceptor));
		});
	});

	// Get recommended polling frequency
	aser->add([=](AsyncSerializer* serializer) {
		requestPollingInterval([=](const QString& error_msg, quint64 msec) {
			if (!error_msg.isEmpty()) {
				*shared_error = error_msg;
			} else {
				// For very large values and unsupported values pick reasonable defaults.
				const quint64 max_interval_msec = 1000;
				if (msec == 0 || msec > max_interval_msec) {  // usually means "see device docs".
					emit logMessage(tr("* Device-recommended polling frequency is invalid, using our default: %1").arg(default_normal_polling_interval_msec_));
					normal_polling_interval_msec_ = default_normal_polling_interval_msec_;
				} else {
					emit logMessage(tr("* Device-recommended polling frequency: %1").arg(msec));
					normal_polling_interval_msec_ = int(msec);
				}
			}
			serializer->continueSequence(error_msg.isEmpty());
		});
	});

	// Get bill / coin identifiers
// 	if (req_identifiers_on_init_) {
		aser->add([=](AsyncSerializer* serializer) {
			requestIdentifiers([=](const QString& error_msg, const QMap<quint8, CcIdentifier>& identifiers) {
				if (!error_msg.isEmpty()) {
					*shared_error = error_msg;
				} else {
					identifiers_ = identifiers;
				}
				serializer->continueSequence(error_msg.isEmpty());
			});
		});
// 	}

	// Modify bill validator operating mode - enable escrow and stacker
	aser->add([=](AsyncSerializer* serializer) {
		if (device_category_ == CcCategory::BillValidator) {
			requestSetBillOperatingMode(true, true, [=](const QString& error_msg) {
				if (!error_msg.isEmpty()) {
					*shared_error = error_msg;
				}
				serializer->continueSequence(error_msg.isEmpty());
			});
		} else {
			serializer->continueSequence(true);
		}
	});

	// Set individual inhibit status on all bills / coins. The specification says
	// that this is not needed for coin acceptors, but the practice shows it is.
	aser->add([=](AsyncSerializer* serializer) {
		requestSetInhibitStatus(0xff, 0xff, [=](const QString& error_msg) {
			if (!error_msg.isEmpty()) {
				*shared_error = error_msg;
			}
			serializer->continueSequence(error_msg.isEmpty());
		});
	});

	return aser->start();
}



bool CctalkDevice::switchStateNormalAccepting(std::function<void(const QString& error_msg)> finish_callback)
{
	DBG_ASSERT_RETURN(device_state_ == CcDeviceState::Initialized
			|| device_state_ == CcDeviceState::NormalRejecting || device_state_ == CcDeviceState::DiagnosticsPolling, false);

	// Disable master inhibit.
	requestSetMasterInhibitStatus(false, [=](const QString& error_msg) {
		if (error_msg.isEmpty()) {
			setDeviceState(CcDeviceState::NormalAccepting);
			finish_callback(error_msg);
		} else {
			requestSwitchDeviceState(CcDeviceState::UnexpectedDown, finish_callback);
		}
	});

	return true;
}



bool CctalkDevice::switchStateNormalRejecting(std::function<void(const QString& error_msg)> finish_callback)
{
	DBG_ASSERT_RETURN(device_state_ == CcDeviceState::Initialized
			|| device_state_ == CcDeviceState::NormalAccepting || device_state_ == CcDeviceState::DiagnosticsPolling, false);

	// Enable master inhibit.
	requestSetMasterInhibitStatus(true, [=](const QString& error_msg) {
		if (error_msg.isEmpty()) {
			setDeviceState(CcDeviceState::NormalRejecting);
			finish_callback(error_msg);
		} else {
			requestSwitchDeviceState(CcDeviceState::UnexpectedDown, finish_callback);
		}
	});

	return true;
}



bool CctalkDevice::switchStateDiagnosticsPolling(std::function<void(const QString& error_msg)> finish_callback)
{
	DBG_ASSERT_RETURN(device_state_ == CcDeviceState::Initialized
			|| device_state_ == CcDeviceState::NormalAccepting || device_state_ == CcDeviceState::NormalRejecting, false);

	// Enable master inhibit (if possible).
	// In theory, this is redundant since the device itself will enable it if a fault
	// is detected, but just in case of a software logic error...
	requestSetMasterInhibitStatus(true, [=](const QString& error_msg) {
		if (error_msg.isEmpty()) {
			setDeviceState(CcDeviceState::DiagnosticsPolling);
			finish_callback(error_msg);
		} else {
			requestSwitchDeviceState(CcDeviceState::UnexpectedDown, finish_callback);
		}
	});

	return true;
}



bool CctalkDevice::switchStateShutDown(std::function<void(const QString& error_msg)> finish_callback)
{
	// If the device is in accepting mode, switch it off
	if (device_state_ == CcDeviceState::NormalAccepting) {
		requestSetMasterInhibitStatus(true, [=](const QString& error_msg) {
			// No error checking (?)
			setDeviceState(CcDeviceState::ShutDown);
			finish_callback(error_msg);
		});
	} else {
		setDeviceState(CcDeviceState::ShutDown);
		finish_callback(QString());
	}
	return true;
}



void CctalkDevice::requestCheckAlive(std::function<void(const QString& error_msg, bool alive)> finish_callback)
{
	// This can be before the callback connection because it's queued.
	quint64 sent_request_id = link_controller_.ccRequest(CcHeader::SimplePoll, QByteArray());

	link_controller_.executeOnReturn(sent_request_id, [=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable {
		if (!error_msg.isEmpty()) {
			emit logMessage(tr("! Error checking for device alive status (simple poll): %1").arg(error_msg));
			finish_callback(error_msg, false);
			return;
		}
		if (!command_data.isEmpty()) {
			QString error = tr("! Non-empty data received while waiting for ACK.");
			emit ccResponseDataDecodeError(request_id, error);  // auto-logged
			finish_callback(error, false);
			return;
		}
		emit logMessage(tr("* Device is alive (answered to simple poll)"));
		finish_callback(QString(), true);
	});
}



void CctalkDevice::requestManufacturingInfo(std::function<void(const QString& error_msg, CcCategory category, const QString& info)> finish_callback)
{
	auto shared_error = std::make_shared<QString>();
	auto category = std::make_shared<CcCategory>();
	auto infos = std::make_shared<QStringList>();

	auto aser = new AsyncSerializer(  // auto-deleted
		// Finish callback
		[=](AsyncSerializer* serializer) {
			QString info = infos->join(QStringLiteral("\n"));

			// Log the info
			if (!shared_error->isEmpty()) {
				emit logMessage(tr("! Error getting full general information: %1").arg(*shared_error));
			}
			if (!info.isEmpty()) {
				emit logMessage(tr("* Manufacturing information:\n%1").arg(info));
			}
			finish_callback(*shared_error, *category, info);
		}
	);

	// Category
	aser->add([=](AsyncSerializer* serializer) {
		quint64 sent_request_id = link_controller_.ccRequest(CcHeader::GetEquipmentCategory, QByteArray());
		link_controller_.executeOnReturn(sent_request_id, [=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable {
			if (!error_msg.isEmpty()) {
				*shared_error = error_msg;
			} else {
				// Decode the data
				QString decoded = QString::fromUtf8(command_data);
				*infos << QObject::tr("*** Equipment category: %1").arg(decoded);
				*category = ccCategoryFromReportedName(decoded);
			}
			serializer->continueSequence(error_msg.isEmpty());
		});
	});

	// Product code
	aser->add([=](AsyncSerializer* serializer) {
		quint64 sent_request_id = link_controller_.ccRequest(CcHeader::GetProductCode, QByteArray());
		link_controller_.executeOnReturn(sent_request_id, [=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable {
			if (!error_msg.isEmpty()) {
				*shared_error = error_msg;
			} else {
				// Decode the data
				QString decoded = QString::fromLatin1(command_data);
				*infos << QObject::tr("*** Product code: %1").arg(decoded);
			}
			serializer->continueSequence(error_msg.isEmpty());
		});
	});

	// Build code
	aser->add([=](AsyncSerializer* serializer) {
		quint64 sent_request_id = link_controller_.ccRequest(CcHeader::GetBuildCode, QByteArray());
		link_controller_.executeOnReturn(sent_request_id, [=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable {
			if (!error_msg.isEmpty()) {
				*shared_error = error_msg;
			} else {
				// Decode the data
				QString decoded = QString::fromLatin1(command_data);
				*infos << QObject::tr("*** Build code: %1").arg(decoded);
			}
			serializer->continueSequence(error_msg.isEmpty());
		});
	});

	// Manufacturer
	aser->add([=](AsyncSerializer* serializer) {
		quint64 sent_request_id = link_controller_.ccRequest(CcHeader::GetManufacturer, QByteArray());
		link_controller_.executeOnReturn(sent_request_id, [=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable {
			if (!error_msg.isEmpty()) {
				*shared_error = error_msg;
			} else {
				// Decode the data
				QString decoded = QString::fromLatin1(command_data);
				*infos << QObject::tr("*** Manufacturer: %1").arg(decoded);
			}
			serializer->continueSequence(error_msg.isEmpty());
		});
	});

	// S/N
	aser->add([=](AsyncSerializer* serializer) {
		quint64 sent_request_id = link_controller_.ccRequest(CcHeader::GetSerialNumber, QByteArray());
		link_controller_.executeOnReturn(sent_request_id, [=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable {
			if (!error_msg.isEmpty()) {
				*shared_error = error_msg;
			} else {
				// Decode the data
				QString decoded = QString::fromLatin1(command_data.toHex());
				*infos << QObject::tr("*** Serial number: %1").arg(decoded);
			}
			serializer->continueSequence(error_msg.isEmpty());
		});
	});

	// Software revision
	aser->add([=](AsyncSerializer* serializer) {
		quint64 sent_request_id = link_controller_.ccRequest(CcHeader::GetSoftwareRevision, QByteArray());
		link_controller_.executeOnReturn(sent_request_id, [=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable {
			if (!error_msg.isEmpty()) {
				*shared_error = error_msg;
			} else {
				// Decode the data
				QString decoded = QString::fromLatin1(command_data);
				*infos << QObject::tr("*** Software Revision: %1").arg(decoded);
			}
			serializer->continueSequence(error_msg.isEmpty());
		});
	});

	// ccTalk command set revision
	aser->add([=](AsyncSerializer* serializer) {
		quint64 sent_request_id = link_controller_.ccRequest(CcHeader::GetCommsRevision, QByteArray());
		link_controller_.executeOnReturn(sent_request_id, [=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable {
			if (!error_msg.isEmpty()) {
				*shared_error = error_msg;
			} else {
				if (command_data.size() == 3) {
					*infos << QObject::tr("*** ccTalk product release: %1, ccTalk version %2.%3").arg(int(command_data.at(0)))
							.arg(int(command_data.at(1))).arg(int(command_data.at(2)));
				} else {
					*infos << QObject::tr("*** ccTalk comms revision (encoded): %1").arg(QString::fromLatin1(command_data.toHex()));
				}
			}
			serializer->continueSequence(error_msg.isEmpty());
		});
	});

	aser->start();
}



void CctalkDevice::requestPollingInterval(std::function<void(const QString& error_msg, quint64 msec)> finish_callback)
{
	quint64 sent_request_id = link_controller_.ccRequest(CcHeader::GetPollingPriority, QByteArray());
	link_controller_.executeOnReturn(sent_request_id, [=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable {
		if (!error_msg.isEmpty()) {
			emit logMessage(tr("! Error getting polling interval: %1").arg(error_msg));
			finish_callback(error_msg, 0);
			return;
		}
		// Decode the data
		if (command_data.size() != 2) {
			QString error = tr("! Invalid polling interval data received.");
			emit ccResponseDataDecodeError(request_id, error);  // auto-logged
			finish_callback(error, 0);
			return;
		}

		quint64 unit = command_data[0];
		quint64 value = command_data[1];

		quint64 ms_multiplier = 1;
		switch(unit) {
			case 0: ms_multiplier = 0; break;
			case 1: ms_multiplier = 1; break;
			case 2: ms_multiplier = 10; break;
			case 3: ms_multiplier = 1000UL; break;
			case 4: ms_multiplier = 1000UL * 60; break;
			case 5: ms_multiplier = 1000UL * 60 * 60; break;
			case 6: ms_multiplier = 1000UL * 60 * 60 * 24; break;
			case 7: ms_multiplier = 1000UL * 60 * 60 * 24 * 7; break;
			case 8: ms_multiplier = 1000UL * 60 * 60 * 24 * 7 * 30; break;
			case 9: ms_multiplier = 1000UL * 31557600; break;
		}

		// 0,0 means "see the device docs".
		// 0,255 means "use hw device poll line".
		quint64 interval_ms = ms_multiplier * value;

		finish_callback(QString(), interval_ms);
	});
}



void CctalkDevice::requestSetInhibitStatus(quint8 accept_mask1, quint8 accept_mask2, std::function<void(const QString& error_msg)> finish_callback)
{
	QByteArray command_arg;
	// lower 8 and higher 8, 16 coins/bills total.
	command_arg.append(accept_mask1).append(accept_mask2);

	quint64 sent_request_id = link_controller_.ccRequest(CcHeader::SetInhibitStatus, command_arg);
	link_controller_.executeOnReturn(sent_request_id, [=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable {
		if (!error_msg.isEmpty()) {
			emit logMessage(tr("! Error setting inhibit status: %1").arg(error_msg));
			finish_callback(error_msg);
			return;
		}
		if (!command_data.isEmpty()) {
			QString error = tr("! Non-empty data received while waiting for ACK.");
			emit ccResponseDataDecodeError(request_id, error);  // auto-logged
			finish_callback(error);
			return;
		}
		emit logMessage(tr("* Inhibit status set: %1, %2").arg(int(accept_mask1)).arg(int(accept_mask2)));
		finish_callback(QString());
	});
}



void CctalkDevice::requestSetMasterInhibitStatus(bool inhibit, std::function<void(const QString& error_msg)> finish_callback)
{
	QByteArray command_arg;
	command_arg.append(char(inhibit ? 0x0 : 0x1));  // 0 means master inhibit active.

	quint64 sent_request_id = link_controller_.ccRequest(CcHeader::SetMasterInhibitStatus, command_arg);
	link_controller_.executeOnReturn(sent_request_id, [=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable {
		if (!error_msg.isEmpty()) {
			emit logMessage(tr("! Error setting master inhibit status: %1").arg(error_msg));
			finish_callback(error_msg);
			return;
		}
		if (!command_data.isEmpty()) {
			QString error = tr("! Non-empty data received while waiting for ACK.");
			emit ccResponseDataDecodeError(request_id, error);  // auto-logged
			finish_callback(error);
			return;
		}
		emit logMessage(tr("* Master inhibit status set to: %1").arg(inhibit ? tr("reject") : tr("accept")));
/*
		emit logMessage(tr("* Verifying that master inhibit status was set correctly..."));
		requestMasterInhibitStatus([=](const QString& verify_error_msg, bool fetched_inhibit) {
			if (!verify_error_msg.isEmpty()) {
				finish_callback(verify_error_msg);

			} else if (fetched_inhibit != inhibit) {
				QString error = tr("! Current master inhibit status set to: %1, which differs from the one we requested.")
						.arg(fetched_inhibit ? tr("reject") : tr("accept"));
				emit logMessage(error);
				finish_callback(error);

			} else {
				emit logMessage(tr("* Master inhibit status verified to be: %1").arg(fetched_inhibit ? tr("reject") : tr("accept")));
				finish_callback(QString());
			}
		});
*/
		finish_callback(QString());
	});
}



void CctalkDevice::requestMasterInhibitStatus(std::function<void(const QString& error_msg, bool inhibit)> finish_callback)
{
	quint64 sent_request_id = link_controller_.ccRequest(CcHeader::GetMasterInhibitStatus, QByteArray());
	link_controller_.executeOnReturn(sent_request_id, [=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable {
		if (!error_msg.isEmpty()) {
			emit logMessage(tr("! Error getting master inhibit status: %1").arg(error_msg));
			finish_callback(error_msg, false);
			return;
		}
		if (command_data.size() != 1) {
			QString error = tr("! Invalid data received for GetMasterInhibitStatus.");
			emit ccResponseDataDecodeError(request_id, error);  // auto-logged
			finish_callback(error, false);
			return;
		}
		// Decode the data
		bool inhibit = !bool(command_data.at(0));  // 0 means inhibit
		emit logMessage(tr("* Master inhibit status: %1").arg(!inhibit ? tr("accept") : tr("reject")));
		finish_callback(QString(), inhibit);
	});
}



void CctalkDevice::requestSetBillOperatingMode(bool use_stacker, bool use_escrow,
		std::function<void(const QString& error_msg)> finish_callback)
{
	QByteArray command_arg;
	unsigned int mask = 0;
	if (use_stacker) {
		mask += 1;
	}
	if (use_escrow) {
		mask += 2;
	}
	command_arg.append(char(mask));

	quint64 sent_request_id = link_controller_.ccRequest(CcHeader::SetBillOperatingMode, command_arg);
	link_controller_.executeOnReturn(sent_request_id, [=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable {
		if (!error_msg.isEmpty()) {
			emit logMessage(tr("! Error setting bill validator operating mode: %1").arg(error_msg));
			finish_callback(error_msg);
			return;
		}
		if (!command_data.isEmpty()) {
			QString error = tr("! Non-empty data received while waiting for ACK.");
			emit ccResponseDataDecodeError(request_id, error);  // auto-logged
			finish_callback(error);
			return;
		}
		emit logMessage(tr("* Bill validator operating mode set to: %1").arg(int(mask)));
		finish_callback(QString());
	});
}



void CctalkDevice::requestIdentifiers(std::function<void(const QString& error_msg, const QMap<quint8, CcIdentifier>& identifiers)> finish_callback)
{
	if (device_category_ != CcCategory::CoinAcceptor && device_category_ != CcCategory::BillValidator) {
		emit logMessage(tr("! Cannot request coin / bill identifiers from device category \"%2\".").arg(int(device_category_)));
		return;
	}

	auto shared_max_positions = std::make_shared<quint8>(16);  // FIXME Not sure if this can be queried for coin acceptors
	auto shared_error = std::make_shared<QString>();
	auto shared_identifiers = std::make_shared<QMap<quint8, CcIdentifier>>();
	auto shared_country_scaling_data = std::make_shared<QMap<QByteArray, CcCountryScalingData>>();

	QString coin_bill = (device_category_ == CcCategory::CoinAcceptor ? tr("Coin") : tr("Bill"));


	auto aser = new AsyncSerializer(  // auto-deleted
		// Finish handler
		[=](AsyncSerializer* serializer) {
			if (!shared_error->isEmpty()) {
				emit logMessage(tr("! Error getting %1 identifiers: %2").arg(coin_bill).arg(*shared_error));
			} else {
				if (!shared_identifiers->isEmpty()) {
					QStringList strs;
					strs << tr("* %1 identifiers:").arg(coin_bill);
					for (auto iter = shared_identifiers->constBegin(); iter != shared_identifiers->constEnd(); ++iter) {
						strs << tr("*** %1 position %2: %3").arg(coin_bill).arg(int(iter.key())).arg(QString::fromLatin1(iter.value().id_string));
					}
					emit logMessage(strs.join(QStringLiteral("\n")));
				} else {
					emit logMessage(tr("* No non-empty %1 identifiers received.").arg(coin_bill));
				}
			}
			finish_callback(*shared_error, *shared_identifiers);
		}
	);


	// If this is a Bill Validator, get number of bill types currently supported
	aser->add([=](AsyncSerializer* serializer) {
		if (device_category_ != CcCategory::BillValidator) {
			serializer->continueSequence(true);
			return;
		}

		quint64 sent_request_id = link_controller_.ccRequest(CcHeader::GetVariableSet, QByteArray());
		link_controller_.executeOnReturn(sent_request_id,
				[=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable {
			if (!error_msg.isEmpty()) {
				// Do not set global error, this is a local error of an optional command.
				// *shared_error = error_msg;
			} else {
				// Decode the data
				if (command_data.size() < 2) {
					emit logMessage(tr("! Invalid variable set data returned for bill validator."));
				} else {
					quint8 num_bill_types = command_data.at(0);
					// quint8 num_banks = command_data.at(1);  // unused
					if (num_bill_types > 1) {
						*shared_max_positions = num_bill_types;
						emit logMessage(tr("* Number of bill types currently supported: %1.").arg(int(*shared_max_positions)));
					} else {
						emit logMessage(tr("! Could not get the number of bill types currently supported, falling back to %1.")
								.arg(int(*shared_max_positions)));
					}
				}
			}
			// Don't treat failure as error, this is an optional command.
			serializer->continueSequence(true);
		});
	});


	/// Get coin / bill IDs (and possibly country scaling data)
	for (quint8 pos = 1; pos <= *shared_max_positions; ++pos) {

		/// Fetch coin / bill ID at position pos.
		aser->add([=](AsyncSerializer* serializer) {
			CcHeader get_command = (device_category_ == CcCategory::CoinAcceptor ? CcHeader::GetCoinId : CcHeader::GetBillId);

			quint64 sent_request_id = link_controller_.ccRequest(get_command, QByteArray().append(pos));
			link_controller_.executeOnReturn(sent_request_id,
					[=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable {
				if (!error_msg.isEmpty()) {
					*shared_error = error_msg;

				} else {
					// Decode the data.
					// 6 dots mean empty by convention, but we've seen all-null too.
					if (!command_data.trimmed().isEmpty() && command_data != "......" && command_data.at(0) != 0) {
						CcIdentifier identifier(command_data);
						if (shared_country_scaling_data->contains(identifier.country)) {
							identifier.setCountryScalingData(shared_country_scaling_data->value(identifier.country));
						}
						shared_identifiers->insert(pos, identifier);
					}
				}

				serializer->continueSequence(error_msg.isEmpty());
			});
		});


		// If this is a Bill Validator, get country scaling data.
		// For coin acceptors, use a fixed, predefined country scaling data.
		aser->add([=](AsyncSerializer* serializer) {

			if (!shared_identifiers->contains(pos)) {  // empty position
				serializer->continueSequence(true);
				return;
			}

			QByteArray country = shared_identifiers->value(pos).country;
			if (country.isEmpty() || shared_country_scaling_data->contains(country)) {
				serializer->continueSequence(true);
				return;  // already present
			}

			// Predefined rules for Georgia. TODO Make this configurable and / or remove it from here.
			if (device_category_ == CcCategory::CoinAcceptor && country == "GE") {
				CcCountryScalingData data;
				data.scaling_factor = 1;
				data.decimal_places = 2;
				shared_country_scaling_data->insert(country, data);
				(*shared_identifiers)[pos].setCountryScalingData(data);
				emit logMessage(tr("* Using predefined country scaling data for %1: scaling factor: %2, decimal places: %3.")
						.arg(QString::fromLatin1(country)).arg(data.scaling_factor).arg(int(data.decimal_places)));
				serializer->continueSequence(true);
				return;
			}

			if (device_category_ != CcCategory::BillValidator) {
				serializer->continueSequence(true);
				return;
			}

			quint64 sent_request_id = link_controller_.ccRequest(CcHeader::GetCountryScalingFactor, country);
			link_controller_.executeOnReturn(sent_request_id,
					[=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable {
				if (!error_msg.isEmpty()) {
					*shared_error = error_msg;

				} else {
					// Decode the data
					if (command_data.size() != 3) {
						emit logMessage(tr("! Invalid scaling data for country %1.").arg(QString::fromLatin1(country)));
					} else {
						CcCountryScalingData data;
						int lsb = command_data.at(0), msb = command_data.at(1);
						data.scaling_factor = quint16(lsb + msb*256);
						data.decimal_places = command_data.at(2);
						if (data.isValid()) {
							shared_country_scaling_data->insert(country, data);
							(*shared_identifiers)[pos].setCountryScalingData(data);
							emit logMessage(tr("* Country scaling data for %1: scaling factor: %2, decimal places: %3.")
									.arg(QString::fromLatin1(country)).arg(data.scaling_factor).arg(int(data.decimal_places)));
						} else {
							emit logMessage(tr("* Country scaling data for %1: empty!").arg(QString::fromLatin1(country)));
						}
					}
				}
				serializer->continueSequence(error_msg.isEmpty());
			});

		});
	}

	aser->start();
}



void CctalkDevice::requestBufferedCreditEvents(std::function<void(const QString& error_msg,
		quint8 event_counter, const QVector<CcEventData>& event_data)> finish_callback)
{
	// Coin acceptors use ReadBufferedCredit command.
	// Bill validators use ReadBufferedBillEvents command.
	// Both commands return data in approximately the same format.

	// The response format is: [event_counter] [result 1A] [result 1B] [result 2A] [result 2B] ... [result 5B].
	// There are usually 5 events. 1A/1B is the newest one.
	// diff (event_counter, last_event_counter) indicates the number of results that are new. If > 5, it means data was lost.
	// [event_counter] == 0 means power-up or reset condition.
	// Note that [event_counter] wraps from 255 to 1, not 0.
	// [result A]: If in 1-255 range, it's credit (coin/bill position). If 0, see error code in [result B].
	// [result B]: If A is 0, B is error code, see CcCoinAcceptorEventCode / CcBillValidatorErrorCode.
	// If A is credit, B is sorter path (0 unsupported, 1-8 path number).

	QString coin_bill = (device_category_ == CcCategory::CoinAcceptor ? tr("Coin") : tr("Bill"));
	CcHeader command = (device_category_ == CcCategory::CoinAcceptor ? CcHeader::ReadBufferedCredit : CcHeader::ReadBufferedBillEvents);

	quint64 sent_request_id = link_controller_.ccRequest(command, QByteArray());
	link_controller_.executeOnReturn(sent_request_id,
			[=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable {

		// TODO Handle command timeout

		QVector<CcEventData> event_data;
		if (!error_msg.isEmpty()) {
			emit logMessage(tr("! Error getting %1 buffered credit / events: %2").arg(coin_bill).arg(error_msg));
			finish_callback(error_msg, 0, event_data);
			return;
		}
		if (command_data.isEmpty()) {
			QString error = tr("! Invalid (empty) %1 buffered credit / event data received.").arg(coin_bill);
			emit ccResponseDataDecodeError(request_id, error);  // auto-logged
			finish_callback(error, 0, event_data);
			return;
		}
		if (command_data.size() % 2 != 1) {
			QString error = tr("! Invalid %1 buffered credit / event data size received, unexpected size: %2.")
					.arg(coin_bill).arg(command_data.size());
			emit ccResponseDataDecodeError(request_id, error);  // auto-logged
			finish_callback(error, 0, event_data);
			return;
		}

		quint8 event_counter = quint8(command_data[0]);

		// Log the table, but only if changed.
		if (!event_log_read_ || last_event_num_ != event_counter) {
			QStringList strs;
			strs << tr("* %1 buffered credit / event table (newest to oldest):").arg(coin_bill);
			strs << tr("*** Host-side last processed event number: %1").arg(int(last_event_num_));
			strs << tr("*** Device-side event counter: %1").arg(int(event_counter));
			for (int i = 1; (i+1) < command_data.size(); i += 2) {
				strs << tr("*** Credit: %1, error / sorter: %2").arg(int(command_data[i])).arg(int(command_data[i+1]));
			}
			emit logMessage(strs.join(QStringLiteral("\n")));
			event_log_read_ = true;
		}

		for (int i = 1; (i+1) < command_data.size(); i += 2) {
			CcEventData ev_data(command_data.at(i), command_data.at(i+1), device_category_);
			event_data << ev_data;  // new ones first
		}

		finish_callback(QString(), event_counter, event_data);
	});
}



void CctalkDevice::processCreditEventLog(bool accepting, const QString& event_log_cmd_error_msg, quint8 event_counter,
		const QVector<CcEventData>& event_data, std::function<void()> finish_callback)
{
	// The specification says:
	// If ReadBufferedCredit times out, do nothing.
	// If [event_counter] == 0 and last_event_num == 0, the device is operating normally (just initialized).
	// If [event_counter] == 0 and last_event_num != 0, the device lost power (possible loss of credits).
	// If [event_counter] == last_event_counter, no new credits.
	// If diff([event_counter], last_event_counter) > 5, one or more credits lost.
	// if diff([event_counter], last_event_counter) <= 5, new credit/event info.

	// The general mode of operation:
	// If in coin event polling and an error event is received, set status to error and start error event polling instead.
	// If in error event polling and all ok event is received, switch to coin event polling.

	// if master inhibit ON is detected there, switch to NormalRejecting (?).
	// if an error is detected there, switch to diagnostics state.
	// If bill is held in escrow, call validator function and accept / reject it.
	// emit creditAccepted().

	// Per specification, a command timeout should be ignored.
	if (event_log_cmd_error_msg.isEmpty() && event_counter == 0 && event_data.isEmpty()) {
		finish_callback();
		return;  // nothing
	}

	// If an error occurred during polling, do nothing (?)
	if (!event_log_cmd_error_msg.isEmpty()) {
		finish_callback();
		return;
	}

	// When the device is first booted, the event log contains all zeroes.

	if (last_event_num_ == 0 && event_counter == 0) {
		// The device is operating normally (just initialized).
		finish_callback();
		return;
	}

	// If the event counter suddenly drops to 0, this means that the device was
	// probably reset. Probable loss of credits.
	if (last_event_num_ != 0 && event_counter == 0) {
		emit logMessage(tr("! The device appears to have been reset, possible loss of credit."));
		requestSwitchDeviceState(CcDeviceState::ExternalReset, [=](const QString& local_error_msg) {
			last_event_num_ = 0;
			finish_callback();
		});
		return;
	}

	// If the event counters are equal, there are no new events.
	if (last_event_num_ == event_counter) {
		// nothing
		finish_callback();
		return;
	}

	// If true, it means that we're processing the events that are in the device.
	// We should not give credit in this case, since that was probably processed
	// during previous application run.
	const bool processing_app_startup_events = (last_event_num_ == 0);
	if (processing_app_startup_events && event_counter != 0) {
		emit logMessage(tr("! Detected device that was up (and generating events) before the host startup; ignoring \"credit accepted\" events."));
	}

	int num_new_events = int(event_counter) - int(last_event_num_);
	if (num_new_events < 0) {
		num_new_events += 255;
	}
	last_event_num_ = event_counter;

	if (num_new_events > event_data.size()) {
		emit logMessage(tr("! Event counter difference %1 is greater than buffer size %2, possible loss of credit.")
				.arg(num_new_events).arg(event_data.size()));
	}

	// Newest to oldest
	QVector<CcEventData> new_event_data = event_data.mid(0, num_new_events);
	emit logMessage(tr("* Found %1 new event(s); processing from oldest to newest.").arg(new_event_data.size()));

	bool self_check_requested = false;
	bool bill_routing_pending = false;
	bool bill_routing_force_reject = false;
	CcEventData routing_req_event;

	for (int i = new_event_data.size() - 1; i >= 0; --i) {
		CcEventData ev = new_event_data.at(i);
		const bool processing_last_event = (i == 0);

		if (ev.hasError()) {
			// Coin Acceptor
			if (getStoredDeviceCategory() == CcCategory::CoinAcceptor) {
				// Events may be of 3 types: Accepted (e.g. "Coin too fast over sensor"),
				// Rejected (e.g. "Inhibited coin") and Possibly accepted (e.g. "Motor error").

				CcCoinRejectionType rejection_type = ccCoinAcceptorEventCodeGetRejectionType(ev.coin_error_code);

				emit logMessage(tr("$ Coin status/error event %1 found, rejection type: %2.")
						.arg(ccCoinAcceptorEventCodeGetDisplayableName(ev.coin_error_code))
						.arg(ccCoinRejectionTypeGetDisplayableName(rejection_type)));

				// If the coin event code is of Unknown type (that is, not Accepted or Rejected),
				// there may be a device error. Perform diagnostics.
				// FIXME Not sure if this is correct.
				if (rejection_type == CcCoinRejectionType::Unknown) {
					self_check_requested = true;
				}

			// Bill Validator
			} else {
				// Events may be just status events (Bill Returned From Escrow, Stacker OK, ...),
				// or they may be ones that cause the PerformSelfCheck command to return a fault code.

				emit logMessage(tr("$ Bill status/error event %1 found, event type: %2.")
						.arg(ccBillValidatorErrorCodeGetDisplayableName(ev.bill_error_code))
						.arg(ccBillValidatorEventTypeGetDisplayableName(ev.bill_event_type)));

				// Schedule a single PerformSelfCheck to see if it's actually an error (and if it's persistent).
				if (ev.bill_event_type != CcBillValidatorEventType::Status && ev.bill_event_type != CcBillValidatorEventType::Reject) {
					self_check_requested = true;
				}
			}

		} else {

			// Coins are accepted unconditionally
			if (getStoredDeviceCategory() == CcCategory::CoinAcceptor) {
				// Coin accepted, credit the user.
				CcIdentifier id = identifiers_.value(ev.coin_id);
				if (processing_app_startup_events) {
					emit logMessage(tr("$ The following is a startup event message, ignore it:"));
				}
				emit logMessage(tr("$ Coin (position %1, ID %2) has been accepted to sorter path %3.")
						.arg(int(ev.coin_id)).arg(QString::fromLatin1(id.id_string)).arg(int(ev.coin_sorter_path)));
				if (!accepting && !processing_app_startup_events) {
					emit logMessage(tr("! Coin accepted even though we're in rejecting mode; internal error!"));
				}
				if (!processing_app_startup_events) {
					emit creditAccepted(ev.coin_id, id);
				}

			// Bills
			} else {
				CcIdentifier id = identifiers_.value(ev.bill_id);

				// Bills are held in escrow and should be manually accepted into a stacker.
				if (ev.bill_success_code == CcBillValidatorSuccessCode::ValidatedAndHeldInEscrow) {
					// We send the RouteBill command ONLY if this is the last event in event log, otherwise
					// there may be an inhibit or rejection or accepted any other event after it and we do not
					// want to operate on old data and assumptions.
					if (!processing_last_event) {
						if (processing_app_startup_events) {
							emit logMessage(tr("$ The following is a startup event message, ignore it:"));
						}
						emit logMessage(tr("$ Bill (position %1, ID %2) is or was in escrow, too late to process an old event; ignoring.")
								.arg(int(ev.bill_id)).arg(QString::fromLatin1(id.id_string)));
						continue;
					}
					if (!accepting) {
						if (processing_app_startup_events) {
							emit logMessage(tr("$ The following is a startup event message, ignore it:"));
						}
						emit logMessage(tr("$ Bill (position %1, ID %2) is or was in escrow, even though we're in rejecting mode; ignoring.")
								.arg(int(ev.bill_id)).arg(QString::fromLatin1(id.id_string)));
						bill_routing_force_reject = true;
					}

					// emit logMessage(tr("$ Bill (position %1, ID %2) is in escrow, deciding whether to accept.").arg(int(ev.bill_id)).arg(id.id_string));
					bill_routing_pending = true;
					routing_req_event = ev;
					// Continue below

				} else if (ev.bill_success_code == CcBillValidatorSuccessCode::ValidatedAndAccepted) {
					// This success code appears in event log after a bill routing request, or no routing command timeout.
					// Credit the user.
					if (processing_app_startup_events) {
						emit logMessage(tr("$ The following is a startup event message, ignore it:"));
					}
					emit logMessage(tr("$ Bill (position %1, ID %2) has been accepted.").arg(int(ev.bill_id)).arg(QString::fromLatin1(id.id_string)));
					if (!accepting && !processing_app_startup_events) {
						emit logMessage(tr("! Bill accepted even though we're in rejecting mode; internal error!"));
					}
					if (!processing_app_startup_events) {
						emit creditAccepted(ev.bill_id, id);
					}

				} else {
					DBG_ASSERT_MSG(0, QStringLiteral("Invalid ev.bill_success_code %1").arg(int(ev.bill_success_code)).toUtf8().constData());
				}
			}
		}
	}


	// We may have a pending routing command and an optionally fatal fault code.
	// First, determine if the fault code is actually fatal.
	// If it is, return the bill. If not, accept it, provided the validator function likes it.

	if (!bill_routing_pending && !self_check_requested) {
		// Nothing more to do, continue processing the events.
		finish_callback();
		return;
	}

	auto aser = new AsyncSerializer(  // auto-deleted
		// Finish handler
		[=](AsyncSerializer* serializer) {
			finish_callback();
		}
	);

	auto shared_self_check_fault_code = std::make_shared<CcFaultCode>(CcFaultCode::Ok);

	// Check if the error is a problem enough to warrant a diagnostics polling mode
	if (self_check_requested) {
		aser->add([=](AsyncSerializer* serializer) {
			emit logMessage(tr("* At least one new event has an error code, requesting SelfCheck to see if there is a global fault code."));

			*shared_self_check_fault_code = CcFaultCode::CustomCommandError;

			requestSelfCheck([=](const QString& error_msg, CcFaultCode fault_code) {
				*shared_self_check_fault_code = fault_code;
				aser->continueSequence(true);
			});
		});
	}

	// Decide what to do with the bill currently in escrow
	if (bill_routing_pending) {
		aser->add([=](AsyncSerializer* serializer) {
			bool accept = false;
			CcIdentifier id = identifiers_.value(routing_req_event.bill_id);

			if (*shared_self_check_fault_code != CcFaultCode::Ok) {
				emit logMessage(tr("* SelfCheck returned a non-OK fault code; pending bill in escrow will be rejected."));

			} else if (bill_routing_force_reject) {
				emit logMessage(tr("! Forcing bill validation rejection due to being in NormalRejecting state; internal error."));

			} else {
				DBG_ASSERT(bill_validator_func_);
				if (bill_validator_func_) {
					accept = bill_validator_func_(routing_req_event.bill_id, id.id_string);
				}
				emit logMessage(tr("* Bill validating function status: %1.").arg(accept ? tr("accept") : tr("reject")));
			}

			CcBillRouteCommandType route_command = accept ? CcBillRouteCommandType::RouteToStacker : CcBillRouteCommandType::ReturnBill;
			emit logMessage(tr("$ Bill (position %1, ID %2) is in escrow, sending a request for: %3.")
					.arg(int(routing_req_event.bill_id)).arg(QString::fromLatin1(id.id_string))
					.arg(ccBillRouteCommandTypeGetDisplayableName(route_command)));

			requestRouteBill(route_command, [=](const QString& error_msg, CcBillRouteStatus status) {
				emit logMessage(tr("$ Bill (position %1, ID %2) routing status: %3.")
						.arg(int(routing_req_event.bill_id)).arg(QString::fromLatin1(id.id_string))
						.arg(ccBillRouteStatusGetDisplayableName(status)));

				aser->continueSequence(true);
			});
		});
	}

	// If the fault code was not Ok, switch to diagnostics mode.
	if (self_check_requested) {
		aser->add([=](AsyncSerializer* serializer) {
			if (*shared_self_check_fault_code == CcFaultCode::Ok) {
				aser->continueSequence(true);
			} else {
				emit logMessage(tr("* SelfCheck returned a non-OK fault code, switching to diagnostics polling mode."));

				requestSwitchDeviceState(CcDeviceState::DiagnosticsPolling, [=](const QString& local_error_msg) {
					aser->continueSequence(true);
				});
			}
		});
	}

	aser->start();
}



void CctalkDevice::requestRouteBill(CcBillRouteCommandType route,
		std::function<void(const QString& error_msg, CcBillRouteStatus status)> finish_callback)
{
	QByteArray command_arg;
	command_arg.append(char(route));

	quint64 sent_request_id = link_controller_.ccRequest(CcHeader::RouteBill, command_arg);
	link_controller_.executeOnReturn(sent_request_id, [=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable {
		if (!error_msg.isEmpty()) {
			emit logMessage(tr("! Error sending RouteBill command: %1").arg(error_msg));
			finish_callback(error_msg, CcBillRouteStatus::FailedToRoute);
			return;
		}
		if (command_data.size() > 1) {
			QString error = tr("! Invalid data received for RouteBill.");
			emit ccResponseDataDecodeError(request_id, error);  // auto-logged
			finish_callback(error, CcBillRouteStatus::FailedToRoute);
			return;
		}
		// Decode the data.
		// ACK means Routed
		CcBillRouteStatus status = CcBillRouteStatus::Routed;
		if (command_data.size() == 1) {
			status = static_cast<CcBillRouteStatus>(command_data.at(0));
		}
		emit logMessage(tr("* RouteBill command status: %1").arg(ccBillRouteStatusGetDisplayableName(status)));
		finish_callback(QString(), status);
	});
}



void CctalkDevice::requestSelfCheck(std::function<void(const QString& error_msg, CcFaultCode fault_code)> finish_callback)
{
	quint64 sent_request_id = link_controller_.ccRequest(CcHeader::PerformSelfCheck, QByteArray());
	link_controller_.executeOnReturn(sent_request_id, [=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable {
		if (!error_msg.isEmpty()) {
			emit logMessage(tr("! Error getting self-check status: %1").arg(error_msg));
			finish_callback(error_msg, CcFaultCode::CustomCommandError);
			return;
		}
		if (command_data.size() != 1) {
			QString error = tr("! Invalid data received for PerformSelfCheck.");
			emit ccResponseDataDecodeError(request_id, error);  // auto-logged
			finish_callback(error, CcFaultCode::CustomCommandError);
			return;
		}
		// Decode the data
		CcFaultCode fault_code = static_cast<CcFaultCode>(command_data.at(0));
		emit logMessage(tr("* Self-check fault code: %1").arg(ccFaultCodeGetDisplayableName(fault_code)));
		finish_callback(QString(), fault_code);
	});
}



void CctalkDevice::requestResetDevice(std::function<void(const QString& error_msg)> finish_callback)
{
	quint64 sent_request_id = link_controller_.ccRequest(CcHeader::ResetDevice, QByteArray());
	link_controller_.executeOnReturn(sent_request_id, [=](quint64 request_id, const QString& error_msg, const QByteArray& command_data) mutable {
		if (!error_msg.isEmpty()) {
			emit logMessage(tr("! Error sending soft reset request: %1").arg(error_msg));
			finish_callback(error_msg);
			return;
		}
		if (!command_data.isEmpty()) {
			QString error = tr("! Non-empty data received while waiting for ACK.");
			emit ccResponseDataDecodeError(request_id, error);  // auto-logged
			finish_callback(error);
			return;
		}
		emit logMessage(tr("* Soft reset acknowledged, waiting for the device to get back up."));
	});
}



void CctalkDevice::requestResetDeviceWithState(std::function<void(const QString& error_msg)> finish_callback)
{
	requestResetDevice([=](const QString& error_msg) {
		if (error_msg.isEmpty()) {
			requestSwitchDeviceState(CcDeviceState::UninitializedDown, finish_callback);
		} else {
			finish_callback(error_msg);
		}
	});
}



CcDeviceState CctalkDevice::getDeviceState() const
{
	return device_state_;
}



void CctalkDevice::setDeviceState(CcDeviceState state)
{
	if (device_state_ != state) {
		CcDeviceState old_state = device_state_;
		device_state_ = state;
		emit logMessage(tr("Device state changed to: %1").arg(ccDeviceStateGetDisplayableName(state)));
		emit deviceStateChanged(old_state, device_state_);
	}
}



CcCategory CctalkDevice::getStoredDeviceCategory() const
{
	return device_category_;
}



QString CctalkDevice::getStoredManufacturingInfo() const
{
	return manufacturing_info_;
}



int CctalkDevice::getStoredPollingInterval() const
{
	return normal_polling_interval_msec_;
}



QMap<quint8, CcIdentifier> CctalkDevice::getStoredIndentifiers() const
{
	return identifiers_;
}




}
