/**************************************************************************
Copyright: (C) 2014 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#ifndef CCTALK_ENUMS_H
#define CCTALK_ENUMS_H

#include <QMap>
#include <QString>
#include <QObject>
#include <utility>

#include "helpers/debug.h"


namespace qtcc {


/// ccTalk header bytes. Note: Core commands are mandatory, Core plus are optional
/// (except when required for a certain type of device).
/// See specification Appendix 13 for mandatory commands.
enum class CcHeader : quint8 {
	/// Core: Generic reply. ACKs also use this with data size 0.
	Reply = 0,
	/// Core plus, coin acceptor required, bill validator required: Perform soft reset. Returns ACK.
	/// The wait time after ACK is device-specific (see manuals), but may be quite long.
	ResetDevice = 1,
	/// Core plus: A form of reply. Not used.
	Busy = 6,

	// Headers 20-99 are device-type-specific. For example, a bill validator may use
	// one set of commands, while coin acceptor - another.

	// Headers 100-103 are expansion headers. For example, a command 100:221
	// means that the main cc header is 100 and the data contains sub-command value - 221.

	/// Core plus: Switch to new baud rate. Default is 9600. This can be used to set/get current and maximum baud rates.
	SwitchBaudRate = 113,

	/// Coin acceptor auditing, bill validator auditing: Get number of fraud coins/bills put through the device.
	/// Returns [byte1][byte2][byte3]. counter = [byte1] + 256*[byte2] + 65536*[byte3]. Range 0 to 16,777,215.
	GetFraudCounter = 193,
	/// Coin acceptor auditing, bill validator auditing: Get number of coins/bills rejected by the device.
	/// Returns [byte1][byte2][byte3]. counter = [byte1] + 256*[byte2] + 65536*[byte3]. Range 0 to 16,777,215.
	GetRejectCounter = 194,
	/// Coin acceptor auditing, bill validator auditing: Get number of coins/bills accepted by the device.
	/// Returns [byte1][byte2][byte3]. counter = [byte1] + 256*[byte2] + 65536*[byte3]. Range 0 to 16,777,215.
	GetAcceptCounter = 225,
	/// Coin acceptor auditing, bill validator auditing: Get number of coins/bills put through the device.
	/// Returns [byte1][byte2][byte3]. counter = [byte1] + 256*[byte2] + 65536*[byte3]. Range 0 to 16,777,215.
	GetInsertionCounter = 226,

	/// Bill validator required: Read buffered credits or error code. This is the command for polling.
	/// Received: Last 5 events or so. See CcBillValidatorErrorCode.
	/// NOTE Takes at least 38ms for 5 events to be transferred at 9600 baud.
	ReadBufferedBillEvents = 159,
	/// Route bill to stacker. Argument: 0 (return bill), 1 (route to stacker), 255 (increase timeout).
	/// Returned: ACK or error code: 254 (escrow empty), 255 (failed to route).
	RouteBill = 154,
	/// Coin acceptor required: Read buffered credits or error code. This is the command for polling.
	/// Received: Last 5 events or so. See CcCoinAcceptorEventCode.
	/// NOTE Takes at least 38ms for 5 events to be transferred at 9600 baud.
	ReadBufferedCredit = 229,

	/// Coin acceptor required, bill validator required: In case of device fault condition, poll the device using this
	/// command until the fault is resolved. See CcFaultCode.
	PerformSelfCheck = 232,

	/// Coin acceptor required, bill validator required: Get individual accept/reject status of coins/bills.
	/// Returns: 2- (or more-) byte masks (0 accept, 1 reject).
	GetInhibitStatus = 230,
	/// Coin acceptor required, bill validator required: Accept/reject individual coins/bills.
	/// Argument: 2-byte mask (0 accept, 1 reject). Returns ACK.
	SetInhibitStatus = 231,
	/// Coin acceptor required, bill validator required: Get master inhibit status (global switch).
	/// Returns 1 byte (only bit 0 is used), 1 means accept.
	GetMasterInhibitStatus = 227,
	/// Coin acceptor required, bill validator required: Accept/reject all coins/bills (global switch).
	/// Argument: 1 byte (only bit 0 is used), 1 means accept. Returns ACK.
	SetMasterInhibitStatus = 228,
	/// Modify bill validator operating mode. Argument: bit mask, B0: use stacker; B1: use escrow.
	SetBillOperatingMode = 153,

	/// Bill validator required: Get scaling factor for country code. If all returned bytes are 0, the country code is not supported.
	GetCountryScalingFactor = 156,
	/// Bill validator optional, coin acceptor optional: Get device variables. Returned: Free-form data
	/// (except for the first two variables in bill validators).
	GetVariableSet = 247,
	/// Bill validator required: Get bill ID. Argument: bill type, e.g. [1-16]. Returned: ASCII
	GetBillId = 157,
	/// Coin acceptor required: Get coin ID. Argument: coin position [1-16]. Returned: ASCII
	GetCoinId = 184,

	/// Core plus: Get base year in ASCII (e.g. "1998"). Used as base offset in
	/// GetCreationDate and GetLastModificationDate commands.
	GetBaseYear = 170,
	/// Coin acceptor required, bill validator required: Get ccTalk command set revision.
	/// Returns 3 bytes (release, major, minor).
	GetCommsRevision = 4,
	/// Core. Get build code in ASCII
	GetBuildCode = 192,
	/// Core plus, coin acceptor required, bill validator required: Software revision in ASCII
	GetSoftwareRevision = 241,
	/// Core plus, coin acceptor required, bill validator required: Get device S/N (usually 3 bytes)
	GetSerialNumber = 242,
	/// Core: Get product code in ASCII.
	GetProductCode = 244,
	/// Core: Get equipment category in ASCII. See ccCategoryFromReportedName().
	GetEquipmentCategory = 245,
	/// Core: Get manufacturer name (ascii)
	GetManufacturer = 246,

	/// Coin acceptor: Coin acceptor status (1 byte). See CcCoinAcceptorStatus. UNUSED?
	GetStatus = 248,

	/// Coin acceptor, bill validator: Get the recommended maximum polling frequency.
	/// See CctalkDevice::requestPollingInterval().
	GetPollingPriority = 249,
	/// Multi-drop: Each device returns its address, with delay proportional to the address
	/// value (so that there are no mixups).
	AddressPoll = 253,
	/// Core: A simple "alive" check, returns ACK.
	SimplePoll = 254,

	/// Manufactorer-specific, avoid.
	FactorySetUpAndTest = 255,
};



/// Get displayable name
inline QString ccHeaderGetDisplayableName(CcHeader header)
{
	static QMap<CcHeader, QString> name_map = {
		{CcHeader::Reply, QObject::tr("Reply")},
		{CcHeader::ResetDevice, QObject::tr("ResetDevice")},
		{CcHeader::Busy, QObject::tr("Busy")},

		{CcHeader::SwitchBaudRate, QObject::tr("SwitchBaudRate")},

		{CcHeader::GetFraudCounter, QObject::tr("GetFraudCounter")},
		{CcHeader::GetRejectCounter, QObject::tr("GetRejectCounter")},
		{CcHeader::GetAcceptCounter, QObject::tr("GetAcceptCounter")},
		{CcHeader::GetInsertionCounter, QObject::tr("GetInsertionCounter")},

		{CcHeader::ReadBufferedBillEvents, QObject::tr("ReadBufferedBillEvents")},
		{CcHeader::RouteBill, QObject::tr("RouteBill")},
		{CcHeader::ReadBufferedCredit, QObject::tr("ReadBufferedCredit")},

		{CcHeader::PerformSelfCheck, QObject::tr("PerformSelfCheck")},

		{CcHeader::GetInhibitStatus, QObject::tr("GetInhibitStatus")},
		{CcHeader::SetInhibitStatus, QObject::tr("SetInhibitStatus")},
		{CcHeader::GetMasterInhibitStatus, QObject::tr("GetMasterInhibitStatus")},
		{CcHeader::SetMasterInhibitStatus, QObject::tr("SetMasterInhibitStatus")},
		{CcHeader::SetBillOperatingMode, QObject::tr("SetBillOperatingMode")},

		{CcHeader::GetCountryScalingFactor, QObject::tr("GetCountryScalingFactor")},
		{CcHeader::GetVariableSet, QObject::tr("GetVariableSet")},
		{CcHeader::GetBillId, QObject::tr("GetBillId")},
		{CcHeader::GetCoinId, QObject::tr("GetCoinId")},

		{CcHeader::GetBaseYear, QObject::tr("GetBaseYear")},
		{CcHeader::GetCommsRevision, QObject::tr("GetCommsRevision")},
		{CcHeader::GetBuildCode, QObject::tr("GetBuildCode")},
		{CcHeader::GetSoftwareRevision, QObject::tr("GetSoftwareRevision")},
		{CcHeader::GetSerialNumber, QObject::tr("GetSerialNumber")},
		{CcHeader::GetProductCode, QObject::tr("GetProductCode")},
		{CcHeader::GetEquipmentCategory, QObject::tr("GetEquipmentCategory")},
		{CcHeader::GetManufacturer, QObject::tr("GetManufacturer")},

		{CcHeader::GetStatus, QObject::tr("GetStatus")},

		{CcHeader::GetPollingPriority, QObject::tr("GetPollingPriority")},
		{CcHeader::AddressPoll, QObject::tr("AddressPoll")},
		{CcHeader::SimplePoll, QObject::tr("SimplePoll")},

		{CcHeader::FactorySetUpAndTest, QObject::tr("FactorySetUpAndTest")},
	};
	return name_map.value(header, QString());
}



/// Equipment category
enum class CcCategory {
	Unknown,
	CoinAcceptor,
	Payout,
	Reel,
	BillValidator,
	CardReader,
	Changer,
	Display,
	Keypad,
	Dongle,
	Meter,
	Bootloader,
	Power,
	Printer,
	Rng,
	HopperScale,
	CoinFeeder,
	BillRecycler,
	Escrow,
	Debug,
};


/// Return equipment category from its reported name
inline CcCategory ccCategoryFromReportedName(QString reported_name)
{
	static QMap<QString, CcCategory> name_map = {
		{QStringLiteral("Coin Acceptor"), CcCategory::CoinAcceptor},
		{QStringLiteral("Payout"), CcCategory::Payout},
		{QStringLiteral("Reel"), CcCategory::Reel},
		{QStringLiteral("Bill Validator"), CcCategory::BillValidator},
		{QStringLiteral("Card Reader"), CcCategory::CardReader},
		{QStringLiteral("Changer"), CcCategory::Changer},
		{QStringLiteral("Display"), CcCategory::Display},
		{QStringLiteral("Keypad"), CcCategory::Keypad},
		{QStringLiteral("Dongle"), CcCategory::Dongle},
		{QStringLiteral("Meter"), CcCategory::Meter},
		{QStringLiteral("Bootloader"), CcCategory::Bootloader},
		{QStringLiteral("Power"), CcCategory::Power},
		{QStringLiteral("Printer"), CcCategory::Printer},
		{QStringLiteral("RNG"), CcCategory::Rng},
		{QStringLiteral("Hopper Scale"), CcCategory::HopperScale},
		{QStringLiteral("Coin Feeder"), CcCategory::CoinFeeder},
		{QStringLiteral("Bill Recycler"), CcCategory::BillRecycler},
		{QStringLiteral("Escrow"), CcCategory::Escrow},
		{QStringLiteral("Debug"), CcCategory::Debug},
	};
	// We replace _ with a space as an extension to support not-quite-compliant devices.
	return name_map.value(reported_name.replace(QLatin1Char('_'), QLatin1Char(' ')).trimmed(), CcCategory::Unknown);
}


/// Get default cctalk address for a device category
inline quint8 ccCategoryGetDefaultAddress(CcCategory category)
{
	static QMap<CcCategory, quint8> address_map = {
		{CcCategory::CoinAcceptor, 2},
		{CcCategory::Payout, 3},
		{CcCategory::Reel, 30},
		{CcCategory::BillValidator, 40},
		{CcCategory::CardReader, 50},
		{CcCategory::Changer, 55},
		{CcCategory::Display, 60},
		{CcCategory::Keypad, 70},
		{CcCategory::Dongle, 80},
		{CcCategory::Meter, 90},
		{CcCategory::Bootloader, 99},
		{CcCategory::Power, 100},
		{CcCategory::Printer, 110},
		{CcCategory::Rng, 120},
		{CcCategory::HopperScale, 130},
		{CcCategory::CoinFeeder, 140},
		{CcCategory::BillRecycler, 150},
		{CcCategory::Escrow, 160},
		{CcCategory::Debug, 240},
	};
	return address_map.value(category, 0);
}



/// Get cctalk device category from standard address
inline CcCategory ccCategoryFromAddress(quint8 address)
{
	if (address == 2 || (address >= 11 && address <= 17)) return CcCategory::CoinAcceptor;
	if (address == 3 || (address >= 4 && address <= 10)) return CcCategory::Payout;
	if (address == 30 || (address >= 31 && address <= 34)) return CcCategory::Reel;
	if (address == 40 || (address >= 41 && address <= 47)) return CcCategory::BillValidator;
	if (address == 50) return CcCategory::CardReader;
	if (address == 55) return CcCategory::Changer;
	if (address == 60) return CcCategory::Display;
	if (address == 70) return CcCategory::Keypad;
	if (address == 80 || (address >= 85 && address <= 89)) return CcCategory::Dongle;
	if (address == 90) return CcCategory::Meter;
	if (address == 99) return CcCategory::Bootloader;
	if (address == 100) return CcCategory::Power;
	if (address == 110) return CcCategory::Printer;
	if (address == 120) return CcCategory::Rng;
	if (address == 130) return CcCategory::HopperScale;
	if (address == 140) return CcCategory::CoinFeeder;
	if (address == 150) return CcCategory::BillRecycler;
	if (address == 160) return CcCategory::Escrow;
	if (address == 240 || address >= 241) return CcCategory::Debug;
	return CcCategory::Unknown;
}



/// Coin acceptor status, as returned by GetStatus command.
/// Unused.
enum class CcCoinAcceptorStatus : quint8 {
	Ok,
	CoinReturnMechanismActivated,
	CosMechanismActivated,
};



/// Fault code, as returned by PerformSelfCheck command.
/// Extra information may be returned in the second byte.
/// See spec part 3 table 3 and section 14.
enum class CcFaultCode : quint8 {
	Ok = 0,  ///< No fault
	EepromChecksumCorrupted = 1,
	FaultOnInductiveCoils = 2,  ///< Extra info: Coil number
	FaultOnCreditSensor = 3,
	FaultOnPiezoSensor = 4,
	FaultOnReflectiveSensor = 5,
	FaultOnDiameterSensor = 6,
	FaultOnWakeUpSensor = 7,
	FaultOnSorterExitSensors = 8,  ///< Extra info: Sensor number
	NvramChecksumCorrupted = 9,
	CoinDispensingError = 10,
	LowLevelSensorError = 11,  ///< Extra info: Hopper or tube number
	HighLevelSensorError = 12,  ///< Extra info: Hopper or tube number
	CoinCountingError = 13,
	KeypadError = 14,  ///< Extra info: Key number
	ButtonError = 15,
	DisplayError = 16,
	CoinAuditingError = 17,
	FaultOnRejectSensor = 18,
	FaultOnCoinReturnMechanism = 19,
	FaultOnCosMechanism = 20,
	FaultOnRimSensor = 21,
	FaultOnThermistor = 22,
	PayoutMotorFault = 23,  ///< Extra info: Hopper number
	PayoutTimeout = 24,  ///< Extra info: Hopper or tube number
	PayoutJammed = 25,  ///< Extra info: Hopper or tube number
	PayoutSensorFault = 26,  ///< Extra info: Hopper or tube number
	LevelSensorError = 27,  ///< Extra info: Hopper or tube number
	PersonalityModuleNotFitted = 28,
	PersonalityChecksumCorrupted = 29,
	RomChecksumMismatch = 30,
	MissingSlaveDevice = 31,  ///< Extra info: Slave address
	InternalCommsBad = 32,  ///< Extra info: Slave address
	SupplyVoltageOutsideOperatingLimits = 33,
	TemperatureOutsideOperatingLimits = 34,
	DceFault = 35,  ///< Extra info: 1 = coin, 2 = token
	FaultOnBillValidatorSensor = 36,  ///< Extra info: Sensor number
	FaultOnBillTransportMotor = 37,
	FaultOnStacker = 38,
	BillJammed = 39,
	RamTestFaul = 40,
	FaultOnStringSensor = 41,
	AcceptGateFailedOpen = 42,
	AcceptGateFailedClosed = 43,
	StackerMissing = 44,
	StackerFull = 45,
	FlashMemoryEraseFaul = 46,
	FlashMemoryWriteFail = 47,
	SlaveDeviceNotResponding = 48,  ///< Extra info: Device number
	FaultOnOptoSensor = 49,  ///< Extra info: Opto number
	BatteryFault = 50,
	DoorOpen = 51,
	MicroswitchFault = 52,
	RtcFault = 53,
	FirmwareError = 54,
	InitialisationError = 55,
	SupplyCurrentOutsideOperatingLimits = 56,
	ForcedBootloaderMode = 57,
	UnspecifiedFaultCode = 255,  ///< Extra info: Further vendor-specific information

	CustomCommandError = 254,  ///< Not in specification. Indicates a problem with getting the fault code.
};



/// Get displayable name
inline QString ccFaultCodeGetDisplayableName(CcFaultCode code)
{
	static QMap<CcFaultCode, QString> name_map = {
		{CcFaultCode::Ok, QObject::tr("No fault")},
		{CcFaultCode::EepromChecksumCorrupted, QObject::tr("EepromChecksumCorrupted")},
		{CcFaultCode::FaultOnInductiveCoils, QObject::tr("FaultOnInductiveCoils")},
		{CcFaultCode::FaultOnCreditSensor, QObject::tr("FaultOnCreditSensor")},
		{CcFaultCode::FaultOnPiezoSensor, QObject::tr("FaultOnPiezoSensor")},
		{CcFaultCode::FaultOnReflectiveSensor, QObject::tr("FaultOnReflectiveSensor")},
		{CcFaultCode::FaultOnDiameterSensor, QObject::tr("FaultOnDiameterSensor")},
		{CcFaultCode::FaultOnWakeUpSensor, QObject::tr("FaultOnWakeUpSensor")},
		{CcFaultCode::FaultOnSorterExitSensors, QObject::tr("FaultOnSorterExitSensors")},
		{CcFaultCode::NvramChecksumCorrupted, QObject::tr("NvramChecksumCorrupted")},
		{CcFaultCode::CoinDispensingError, QObject::tr("CoinDispensingError")},
		{CcFaultCode::LowLevelSensorError, QObject::tr("LowLevelSensorError")},
		{CcFaultCode::HighLevelSensorError, QObject::tr("HighLevelSensorError")},
		{CcFaultCode::CoinCountingError, QObject::tr("CoinCountingError")},
		{CcFaultCode::KeypadError, QObject::tr("KeypadError")},
		{CcFaultCode::ButtonError, QObject::tr("ButtonError")},
		{CcFaultCode::DisplayError, QObject::tr("DisplayError")},
		{CcFaultCode::CoinAuditingError, QObject::tr("CoinAuditingError")},
		{CcFaultCode::FaultOnRejectSensor, QObject::tr("FaultOnRejectSensor")},
		{CcFaultCode::FaultOnCoinReturnMechanism, QObject::tr("FaultOnCoinReturnMechanism")},
		{CcFaultCode::FaultOnCosMechanism, QObject::tr("FaultOnCosMechanism")},
		{CcFaultCode::FaultOnRimSensor, QObject::tr("FaultOnRimSensor")},
		{CcFaultCode::FaultOnThermistor, QObject::tr("FaultOnThermistor")},
		{CcFaultCode::PayoutMotorFault, QObject::tr("PayoutMotorFault")},
		{CcFaultCode::PayoutTimeout, QObject::tr("PayoutTimeout")},
		{CcFaultCode::PayoutJammed, QObject::tr("PayoutJammed")},
		{CcFaultCode::PayoutSensorFault, QObject::tr("PayoutSensorFault")},
		{CcFaultCode::LevelSensorError, QObject::tr("LevelSensorError")},
		{CcFaultCode::PersonalityModuleNotFitted, QObject::tr("PersonalityModuleNotFitted")},
		{CcFaultCode::PersonalityChecksumCorrupted, QObject::tr("PersonalityChecksumCorrupted")},
		{CcFaultCode::RomChecksumMismatch, QObject::tr("RomChecksumMismatch")},
		{CcFaultCode::MissingSlaveDevice, QObject::tr("MissingSlaveDevice")},
		{CcFaultCode::InternalCommsBad, QObject::tr("InternalCommsBad")},
		{CcFaultCode::SupplyVoltageOutsideOperatingLimits, QObject::tr("SupplyVoltageOutsideOperatingLimits")},
		{CcFaultCode::TemperatureOutsideOperatingLimits, QObject::tr("TemperatureOutsideOperatingLimits")},
		{CcFaultCode::DceFault, QObject::tr("DceFault")},
		{CcFaultCode::FaultOnBillValidatorSensor, QObject::tr("FaultOnBillValidatorSensor")},
		{CcFaultCode::FaultOnBillTransportMotor, QObject::tr("FaultOnBillTransportMotor")},
		{CcFaultCode::FaultOnStacker, QObject::tr("FaultOnStacker")},
		{CcFaultCode::BillJammed, QObject::tr("BillJammed")},
		{CcFaultCode::RamTestFaul, QObject::tr("RamTestFaul")},
		{CcFaultCode::FaultOnStringSensor, QObject::tr("FaultOnStringSensor")},
		{CcFaultCode::AcceptGateFailedOpen, QObject::tr("AcceptGateFailedOpen")},
		{CcFaultCode::AcceptGateFailedClosed, QObject::tr("AcceptGateFailedClosed")},
		{CcFaultCode::StackerMissing, QObject::tr("StackerMissing")},
		{CcFaultCode::StackerFull, QObject::tr("StackerFull")},
		{CcFaultCode::FlashMemoryEraseFaul, QObject::tr("FlashMemoryEraseFaul")},
		{CcFaultCode::FlashMemoryWriteFail, QObject::tr("FlashMemoryWriteFail")},
		{CcFaultCode::SlaveDeviceNotResponding, QObject::tr("SlaveDeviceNotResponding")},
		{CcFaultCode::FaultOnOptoSensor, QObject::tr("FaultOnOptoSensor")},
		{CcFaultCode::BatteryFault, QObject::tr("BatteryFault")},
		{CcFaultCode::DoorOpen, QObject::tr("DoorOpen")},
		{CcFaultCode::MicroswitchFault, QObject::tr("MicroswitchFault")},
		{CcFaultCode::RtcFault, QObject::tr("RtcFault")},
		{CcFaultCode::FirmwareError, QObject::tr("FirmwareError")},
		{CcFaultCode::InitialisationError, QObject::tr("InitialisationError")},
		{CcFaultCode::SupplyCurrentOutsideOperatingLimits, QObject::tr("SupplyCurrentOutsideOperatingLimits")},
		{CcFaultCode::ForcedBootloaderMode, QObject::tr("ForcedBootloaderMode")},
		{CcFaultCode::UnspecifiedFaultCode, QObject::tr("UnspecifiedFaultCode")},
		{CcFaultCode::CustomCommandError, QObject::tr("CustomCommandError")},
	};
	return name_map.value(code, QString());
}



/// Event code returned in Result B byte of ReadBufferedCredit command
/// when Result A byte is 0.
/// See spec part 3, table 2 and section 12.2.
enum class CcCoinAcceptorEventCode : quint8 {
	NoError = 0,
	RejectCoin = 1,
	InhibitedCoin = 2,
	MultipleWindow = 3,
	WakeupTimeout = 4,
	ValidationTimeout = 5,
	CreditSensorTimeout = 6,
	SorterOptoTimeout = 7,
	SecondCloseCoinError = 8,
	AcceptGateNotReady = 9,
	CreditSensorNotReady = 10,
	SorterNotReady = 11,
	RejectCoinNotCleared = 12,
	ValidationSensorNotReady = 13,
	CreditSensorBlocked = 14,
	SorterOptoBlocked = 15,
	CreditSequenceError = 16,
	CoinGoingBackwards = 17,
	CoinTooFastOverCreditSensor = 18,
	CoinTooSlowOverCreditSensor = 19,
	CosMechanismActivated = 20,
	DceOptoTimeout = 21,
	DceOptoNotSeen = 22,
	CreditSensorReachedTooEarly = 23,
	RejectCoinRepeatedSequentialTrip = 24,
	RejectSlug = 25,
	RejectSensorBlocked = 26,
	GamesOverload = 27,
	MaxCoinMeterPulsesExceeded = 28,
	AcceptGateOpenNotClosed = 29,
	AcceptGateClosedNotOpen = 30,
	ManifoldOptoTimeout = 31,
	ManifoldOptoBlocked = 32,
	ManifoldNotReady = 33,
	SecurityStatusChanged = 34,
	MotorException = 35,
	SwallowedCoin = 36,
	CoinTooFastOverValidationSensor = 37,
	CoinTooSlowOverValidationSensor = 38,
	CoinIncorrectlySorted = 39,
	ExternalLightAttack = 40,
	InhibitedCoinType1 = 128,
	InhibitedCoinType2 = 129,
	InhibitedCoinType3 = 130,
	InhibitedCoinType4 = 131,
	InhibitedCoinType5 = 132,
	InhibitedCoinType6 = 133,
	InhibitedCoinType7 = 134,
	InhibitedCoinType8 = 135,
	InhibitedCoinType9 = 136,
	InhibitedCoinType10 = 137,
	InhibitedCoinType11 = 138,
	InhibitedCoinType12 = 139,
	InhibitedCoinType13 = 140,
	InhibitedCoinType14 = 141,
	InhibitedCoinType15 = 142,
	InhibitedCoinType16 = 143,
	InhibitedCoinType17 = 144,
	InhibitedCoinType18 = 145,
	InhibitedCoinType19 = 146,
	InhibitedCoinType20 = 147,
	InhibitedCoinType21 = 148,
	InhibitedCoinType22 = 149,
	InhibitedCoinType23 = 150,
	InhibitedCoinType24 = 151,
	InhibitedCoinType25 = 152,
	InhibitedCoinType26 = 153,
	InhibitedCoinType27 = 154,
	InhibitedCoinType28 = 155,
	InhibitedCoinType29 = 156,
	InhibitedCoinType30 = 157,
	InhibitedCoinType31 = 158,
	InhibitedCoinType32 = 159,
	ReservedCreditCancelling1 = 160,
	ReservedCreditCancellingN = 191,
	DataBlockRequest = 253,
	CoinReturnMechanismActivated = 254,
	UnspecifiedAlarmCode = 255,
};



/// Get displayable name
inline QString ccCoinAcceptorEventCodeGetDisplayableName(CcCoinAcceptorEventCode code)
{
	static QMap<CcCoinAcceptorEventCode, QString> name_map = {
		{CcCoinAcceptorEventCode::NoError, QObject::tr("NoError")},
		{CcCoinAcceptorEventCode::RejectCoin, QObject::tr("RejectCoin")},
		{CcCoinAcceptorEventCode::InhibitedCoin, QObject::tr("InhibitedCoin")},
		{CcCoinAcceptorEventCode::MultipleWindow, QObject::tr("MultipleWindow")},
		{CcCoinAcceptorEventCode::WakeupTimeout, QObject::tr("WakeupTimeout")},
		{CcCoinAcceptorEventCode::ValidationTimeout, QObject::tr("ValidationTimeout")},
		{CcCoinAcceptorEventCode::CreditSensorTimeout, QObject::tr("CreditSensorTimeout")},
		{CcCoinAcceptorEventCode::SorterOptoTimeout, QObject::tr("SorterOptoTimeout")},
		{CcCoinAcceptorEventCode::SecondCloseCoinError, QObject::tr("SecondCloseCoinError")},
		{CcCoinAcceptorEventCode::AcceptGateNotReady, QObject::tr("AcceptGateNotReady")},
		{CcCoinAcceptorEventCode::CreditSensorNotReady, QObject::tr("CreditSensorNotReady")},
		{CcCoinAcceptorEventCode::SorterNotReady, QObject::tr("SorterNotReady")},
		{CcCoinAcceptorEventCode::RejectCoinNotCleared, QObject::tr("RejectCoinNotCleared")},
		{CcCoinAcceptorEventCode::ValidationSensorNotReady, QObject::tr("ValidationSensorNotReady")},
		{CcCoinAcceptorEventCode::CreditSensorBlocked, QObject::tr("CreditSensorBlocked")},
		{CcCoinAcceptorEventCode::SorterOptoBlocked, QObject::tr("SorterOptoBlocked")},
		{CcCoinAcceptorEventCode::CreditSequenceError, QObject::tr("CreditSequenceError")},
		{CcCoinAcceptorEventCode::CoinGoingBackwards, QObject::tr("CoinGoingBackwards")},
		{CcCoinAcceptorEventCode::CoinTooFastOverCreditSensor, QObject::tr("CoinTooFastOverCreditSensor")},
		{CcCoinAcceptorEventCode::CoinTooSlowOverCreditSensor, QObject::tr("CoinTooSlowOverCreditSensor")},
		{CcCoinAcceptorEventCode::CosMechanismActivated, QObject::tr("CosMechanismActivated")},
		{CcCoinAcceptorEventCode::DceOptoTimeout, QObject::tr("DceOptoTimeout")},
		{CcCoinAcceptorEventCode::DceOptoNotSeen, QObject::tr("DceOptoNotSeen")},
		{CcCoinAcceptorEventCode::CreditSensorReachedTooEarly, QObject::tr("CreditSensorReachedTooEarly")},
		{CcCoinAcceptorEventCode::RejectCoinRepeatedSequentialTrip, QObject::tr("RejectCoinRepeatedSequentialTrip")},
		{CcCoinAcceptorEventCode::RejectSlug, QObject::tr("RejectSlug")},
		{CcCoinAcceptorEventCode::RejectSensorBlocked, QObject::tr("RejectSensorBlocked")},
		{CcCoinAcceptorEventCode::GamesOverload, QObject::tr("GamesOverload")},
		{CcCoinAcceptorEventCode::MaxCoinMeterPulsesExceeded, QObject::tr("MaxCoinMeterPulsesExceeded")},
		{CcCoinAcceptorEventCode::AcceptGateOpenNotClosed, QObject::tr("AcceptGateOpenNotClosed")},
		{CcCoinAcceptorEventCode::AcceptGateClosedNotOpen, QObject::tr("AcceptGateClosedNotOpen")},
		{CcCoinAcceptorEventCode::ManifoldOptoTimeout, QObject::tr("ManifoldOptoTimeout")},
		{CcCoinAcceptorEventCode::ManifoldOptoBlocked, QObject::tr("ManifoldOptoBlocked")},
		{CcCoinAcceptorEventCode::ManifoldNotReady, QObject::tr("ManifoldNotReady")},
		{CcCoinAcceptorEventCode::SecurityStatusChanged, QObject::tr("SecurityStatusChanged")},
		{CcCoinAcceptorEventCode::MotorException, QObject::tr("MotorException")},
		{CcCoinAcceptorEventCode::SwallowedCoin, QObject::tr("SwallowedCoin")},
		{CcCoinAcceptorEventCode::CoinTooFastOverValidationSensor, QObject::tr("CoinTooFastOverValidationSensor")},
		{CcCoinAcceptorEventCode::CoinTooSlowOverValidationSensor, QObject::tr("CoinTooSlowOverValidationSensor")},
		{CcCoinAcceptorEventCode::CoinIncorrectlySorted, QObject::tr("CoinIncorrectlySorted")},
		{CcCoinAcceptorEventCode::ExternalLightAttack, QObject::tr("ExternalLightAttack")},
		{CcCoinAcceptorEventCode::InhibitedCoinType1, QObject::tr("InhibitedCoinType1")},
		{CcCoinAcceptorEventCode::InhibitedCoinType2, QObject::tr("InhibitedCoinType2")},
		{CcCoinAcceptorEventCode::InhibitedCoinType3, QObject::tr("InhibitedCoinType3")},
		{CcCoinAcceptorEventCode::InhibitedCoinType4, QObject::tr("InhibitedCoinType4")},
		{CcCoinAcceptorEventCode::InhibitedCoinType5, QObject::tr("InhibitedCoinType5")},
		{CcCoinAcceptorEventCode::InhibitedCoinType6, QObject::tr("InhibitedCoinType6")},
		{CcCoinAcceptorEventCode::InhibitedCoinType7, QObject::tr("InhibitedCoinType7")},
		{CcCoinAcceptorEventCode::InhibitedCoinType8, QObject::tr("InhibitedCoinType8")},
		{CcCoinAcceptorEventCode::InhibitedCoinType9, QObject::tr("InhibitedCoinType9")},
		{CcCoinAcceptorEventCode::InhibitedCoinType10, QObject::tr("InhibitedCoinType10")},
		{CcCoinAcceptorEventCode::InhibitedCoinType11, QObject::tr("InhibitedCoinType11")},
		{CcCoinAcceptorEventCode::InhibitedCoinType12, QObject::tr("InhibitedCoinType12")},
		{CcCoinAcceptorEventCode::InhibitedCoinType13, QObject::tr("InhibitedCoinType13")},
		{CcCoinAcceptorEventCode::InhibitedCoinType14, QObject::tr("InhibitedCoinType14")},
		{CcCoinAcceptorEventCode::InhibitedCoinType15, QObject::tr("InhibitedCoinType15")},
		{CcCoinAcceptorEventCode::InhibitedCoinType16, QObject::tr("InhibitedCoinType16")},
		{CcCoinAcceptorEventCode::InhibitedCoinType17, QObject::tr("InhibitedCoinType17")},
		{CcCoinAcceptorEventCode::InhibitedCoinType18, QObject::tr("InhibitedCoinType18")},
		{CcCoinAcceptorEventCode::InhibitedCoinType19, QObject::tr("InhibitedCoinType19")},
		{CcCoinAcceptorEventCode::InhibitedCoinType20, QObject::tr("InhibitedCoinType20")},
		{CcCoinAcceptorEventCode::InhibitedCoinType21, QObject::tr("InhibitedCoinType21")},
		{CcCoinAcceptorEventCode::InhibitedCoinType22, QObject::tr("InhibitedCoinType22")},
		{CcCoinAcceptorEventCode::InhibitedCoinType23, QObject::tr("InhibitedCoinType23")},
		{CcCoinAcceptorEventCode::InhibitedCoinType24, QObject::tr("InhibitedCoinType24")},
		{CcCoinAcceptorEventCode::InhibitedCoinType25, QObject::tr("InhibitedCoinType25")},
		{CcCoinAcceptorEventCode::InhibitedCoinType26, QObject::tr("InhibitedCoinType26")},
		{CcCoinAcceptorEventCode::InhibitedCoinType27, QObject::tr("InhibitedCoinType27")},
		{CcCoinAcceptorEventCode::InhibitedCoinType28, QObject::tr("InhibitedCoinType28")},
		{CcCoinAcceptorEventCode::InhibitedCoinType29, QObject::tr("InhibitedCoinType29")},
		{CcCoinAcceptorEventCode::InhibitedCoinType30, QObject::tr("InhibitedCoinType30")},
		{CcCoinAcceptorEventCode::InhibitedCoinType31, QObject::tr("InhibitedCoinType31")},
		{CcCoinAcceptorEventCode::InhibitedCoinType32, QObject::tr("InhibitedCoinType32")},
		{CcCoinAcceptorEventCode::ReservedCreditCancelling1, QObject::tr("ReservedCreditCancelling1")},
		{CcCoinAcceptorEventCode::ReservedCreditCancellingN, QObject::tr("ReservedCreditCancellingN")},
		{CcCoinAcceptorEventCode::DataBlockRequest, QObject::tr("DataBlockRequest")},
		{CcCoinAcceptorEventCode::CoinReturnMechanismActivated, QObject::tr("CoinReturnMechanismActivated")},
		{CcCoinAcceptorEventCode::UnspecifiedAlarmCode, QObject::tr("UnspecifiedAlarmCode")},
	};
	return name_map.value(code, QString());
}



/// Coin rejection type for each CcCoinAcceptorEventCode
enum class CcCoinRejectionType {
	Rejected,
	Accepted,
	Unknown,
};



/// Get displayable name
inline QString ccCoinRejectionTypeGetDisplayableName(CcCoinRejectionType type)
{
	static QMap<CcCoinRejectionType, QString> name_map = {
		{CcCoinRejectionType::Rejected, QObject::tr("Rejected")},
		{CcCoinRejectionType::Accepted, QObject::tr("Accepted")},
		{CcCoinRejectionType::Unknown, QObject::tr("Unknown")},
	};
	return name_map.value(type, QString());
}



/// See spec part 3, table 2 and section 12.2.
inline CcCoinRejectionType ccCoinAcceptorEventCodeGetRejectionType(CcCoinAcceptorEventCode code)
{
	switch (code) {
		case CcCoinAcceptorEventCode::NoError:
		case CcCoinAcceptorEventCode::SorterOptoTimeout:
		case CcCoinAcceptorEventCode::CreditSequenceError:
		case CcCoinAcceptorEventCode::CoinGoingBackwards:
		case CcCoinAcceptorEventCode::CoinTooFastOverCreditSensor:
		case CcCoinAcceptorEventCode::CoinTooSlowOverCreditSensor:
		case CcCoinAcceptorEventCode::CosMechanismActivated:
		case CcCoinAcceptorEventCode::CreditSensorReachedTooEarly:
		case CcCoinAcceptorEventCode::RejectSensorBlocked:
		case CcCoinAcceptorEventCode::GamesOverload:
		case CcCoinAcceptorEventCode::MaxCoinMeterPulsesExceeded:
		case CcCoinAcceptorEventCode::AcceptGateOpenNotClosed:
		case CcCoinAcceptorEventCode::ManifoldOptoTimeout:
		case CcCoinAcceptorEventCode::SwallowedCoin:
		case CcCoinAcceptorEventCode::CoinIncorrectlySorted:
		case CcCoinAcceptorEventCode::ExternalLightAttack:
		case CcCoinAcceptorEventCode::DataBlockRequest:
		case CcCoinAcceptorEventCode::CoinReturnMechanismActivated:
		case CcCoinAcceptorEventCode::UnspecifiedAlarmCode:
			return CcCoinRejectionType::Accepted;

		case CcCoinAcceptorEventCode::WakeupTimeout:
		case CcCoinAcceptorEventCode::ValidationTimeout:
		case CcCoinAcceptorEventCode::CreditSensorTimeout:
		case CcCoinAcceptorEventCode::DceOptoTimeout:
		case CcCoinAcceptorEventCode::SecurityStatusChanged:
		case CcCoinAcceptorEventCode::MotorException:
		case CcCoinAcceptorEventCode::ReservedCreditCancelling1:
		case CcCoinAcceptorEventCode::ReservedCreditCancellingN:
			return CcCoinRejectionType::Unknown;

		case CcCoinAcceptorEventCode::RejectCoin:
		case CcCoinAcceptorEventCode::InhibitedCoin:
		case CcCoinAcceptorEventCode::MultipleWindow:
		case CcCoinAcceptorEventCode::SecondCloseCoinError:  // rejected 1 or more
		case CcCoinAcceptorEventCode::AcceptGateNotReady:
		case CcCoinAcceptorEventCode::CreditSensorNotReady:
		case CcCoinAcceptorEventCode::SorterNotReady:
		case CcCoinAcceptorEventCode::RejectCoinNotCleared:
		case CcCoinAcceptorEventCode::ValidationSensorNotReady:
		case CcCoinAcceptorEventCode::CreditSensorBlocked:
		case CcCoinAcceptorEventCode::SorterOptoBlocked:
		case CcCoinAcceptorEventCode::DceOptoNotSeen:
		case CcCoinAcceptorEventCode::RejectCoinRepeatedSequentialTrip:
		case CcCoinAcceptorEventCode::RejectSlug:
		case CcCoinAcceptorEventCode::AcceptGateClosedNotOpen:
		case CcCoinAcceptorEventCode::ManifoldOptoBlocked:
		case CcCoinAcceptorEventCode::ManifoldNotReady:
		case CcCoinAcceptorEventCode::CoinTooFastOverValidationSensor:
		case CcCoinAcceptorEventCode::CoinTooSlowOverValidationSensor:
		case CcCoinAcceptorEventCode::InhibitedCoinType1:
		case CcCoinAcceptorEventCode::InhibitedCoinType2:
		case CcCoinAcceptorEventCode::InhibitedCoinType3:
		case CcCoinAcceptorEventCode::InhibitedCoinType4:
		case CcCoinAcceptorEventCode::InhibitedCoinType5:
		case CcCoinAcceptorEventCode::InhibitedCoinType6:
		case CcCoinAcceptorEventCode::InhibitedCoinType7:
		case CcCoinAcceptorEventCode::InhibitedCoinType8:
		case CcCoinAcceptorEventCode::InhibitedCoinType9:
		case CcCoinAcceptorEventCode::InhibitedCoinType10:
		case CcCoinAcceptorEventCode::InhibitedCoinType11:
		case CcCoinAcceptorEventCode::InhibitedCoinType12:
		case CcCoinAcceptorEventCode::InhibitedCoinType13:
		case CcCoinAcceptorEventCode::InhibitedCoinType14:
		case CcCoinAcceptorEventCode::InhibitedCoinType15:
		case CcCoinAcceptorEventCode::InhibitedCoinType16:
		case CcCoinAcceptorEventCode::InhibitedCoinType17:
		case CcCoinAcceptorEventCode::InhibitedCoinType18:
		case CcCoinAcceptorEventCode::InhibitedCoinType19:
		case CcCoinAcceptorEventCode::InhibitedCoinType20:
		case CcCoinAcceptorEventCode::InhibitedCoinType21:
		case CcCoinAcceptorEventCode::InhibitedCoinType22:
		case CcCoinAcceptorEventCode::InhibitedCoinType23:
		case CcCoinAcceptorEventCode::InhibitedCoinType24:
		case CcCoinAcceptorEventCode::InhibitedCoinType25:
		case CcCoinAcceptorEventCode::InhibitedCoinType26:
		case CcCoinAcceptorEventCode::InhibitedCoinType27:
		case CcCoinAcceptorEventCode::InhibitedCoinType28:
		case CcCoinAcceptorEventCode::InhibitedCoinType29:
		case CcCoinAcceptorEventCode::InhibitedCoinType30:
		case CcCoinAcceptorEventCode::InhibitedCoinType31:
		case CcCoinAcceptorEventCode::InhibitedCoinType32:
			return CcCoinRejectionType::Rejected;
	}
	return CcCoinRejectionType::Unknown;
}



/// Event code returned in Result B byte of ReadBufferedBillEvents command
/// when Result A byte is 0.
/// See spec table 7.
enum class CcBillValidatorErrorCode : quint8 {
	MasterInhibitActive = 0,
	BillReturnedFromEscrow = 1,
	InvalidBillValidationFail = 2,
	InvalidBillTransportProblem = 3,
	InhibitedBillOnSerial = 4,
	InhibitedBillOnDipSwitches = 5,
	BillJammedInTransportUnsafeMode = 6,
	BillJammedInStacker = 7,
	BillPulledBackwards = 8,
	BillTamper = 9,
	StackerOk = 10,
	StackerRemoved = 11,
	StackerInserted = 12,
	StackerFaulty = 13,
	StackerFull = 14,
	StackerJammed = 15,
	BillJammedInTransportSafeMode = 16,
	OptoFraudDetected = 17,
	StringFraudDetected = 18,
	AntiStringMechanismFaulty = 19,
	BarcodeDetected = 20,
	UnknownBillTypeStacked = 21,

	CustomNoError = 255,  ///< Not in specification, only for default-initializing variables.
};



/// Get displayable name
inline QString ccBillValidatorErrorCodeGetDisplayableName(CcBillValidatorErrorCode type)
{
	static QMap<CcBillValidatorErrorCode, QString> name_map = {
		{CcBillValidatorErrorCode::MasterInhibitActive, QObject::tr("MasterInhibitActive")},
		{CcBillValidatorErrorCode::BillReturnedFromEscrow, QObject::tr("BillReturnedFromEscrow")},
		{CcBillValidatorErrorCode::InvalidBillValidationFail, QObject::tr("InvalidBillValidationFail")},
		{CcBillValidatorErrorCode::InvalidBillTransportProblem, QObject::tr("InvalidBillTransportProblem")},
		{CcBillValidatorErrorCode::InhibitedBillOnSerial, QObject::tr("InhibitedBillOnSerial")},
		{CcBillValidatorErrorCode::InhibitedBillOnDipSwitches, QObject::tr("InhibitedBillOnDipSwitches")},
		{CcBillValidatorErrorCode::BillJammedInTransportUnsafeMode, QObject::tr("BillJammedInTransportUnsafeMode")},
		{CcBillValidatorErrorCode::BillJammedInStacker, QObject::tr("BillJammedInStacker")},
		{CcBillValidatorErrorCode::BillPulledBackwards, QObject::tr("BillPulledBackwards")},
		{CcBillValidatorErrorCode::BillTamper, QObject::tr("BillTamper")},
		{CcBillValidatorErrorCode::StackerOk, QObject::tr("StackerOk")},
		{CcBillValidatorErrorCode::StackerRemoved, QObject::tr("StackerRemoved")},
		{CcBillValidatorErrorCode::StackerInserted, QObject::tr("StackerInserted")},
		{CcBillValidatorErrorCode::StackerFaulty, QObject::tr("StackerFaulty")},
		{CcBillValidatorErrorCode::StackerFull, QObject::tr("StackerFull")},
		{CcBillValidatorErrorCode::StackerJammed, QObject::tr("StackerJammed")},
		{CcBillValidatorErrorCode::BillJammedInTransportSafeMode, QObject::tr("BillJammedInTransportSafeMode")},
		{CcBillValidatorErrorCode::OptoFraudDetected, QObject::tr("OptoFraudDetected")},
		{CcBillValidatorErrorCode::StringFraudDetected, QObject::tr("StringFraudDetected")},
		{CcBillValidatorErrorCode::AntiStringMechanismFaulty, QObject::tr("AntiStringMechanismFaulty")},
		{CcBillValidatorErrorCode::BarcodeDetected, QObject::tr("BarcodeDetected")},
		{CcBillValidatorErrorCode::UnknownBillTypeStacked, QObject::tr("UnknownBillTypeStacked")},

		{CcBillValidatorErrorCode::CustomNoError, QObject::tr("CustomNoError")},
	};
	return name_map.value(type, QString());
}



/// Success code returned in Result B byte of ReadBufferedBillEvents command
/// when Result A byte is 1-255.
enum class CcBillValidatorSuccessCode : quint8 {
	ValidatedAndAccepted = 0,  ///< Bill accepted, credit the customer
	ValidatedAndHeldInEscrow = 1,  ///< Bill held in escrow, decide whether to accept it

	CustomUnknown = 255,  ///< Not in specification, only for default-initializing variables.
};



/// Get displayable name
inline QString ccBillValidatorSuccessCodeGetDisplayableName(CcBillValidatorSuccessCode type)
{
	static QMap<CcBillValidatorSuccessCode, QString> name_map = {
		{CcBillValidatorSuccessCode::ValidatedAndAccepted, QObject::tr("ValidatedAndAccepted")},
		{CcBillValidatorSuccessCode::ValidatedAndHeldInEscrow, QObject::tr("ValidatedAndHeldInEscrow")},
		{CcBillValidatorSuccessCode::CustomUnknown, QObject::tr("CustomUnknown")},
	};
	return name_map.value(type, QString());
}



/// Each CcBillValidatorErrorCode and CcBillValidatorSuccessCode has an event type
/// associated with it. See ccBillValidatorEventCodeGetType() and ccBillValidatorSuccessCodeGetEventType().
/// See spec section 18.
enum class CcBillValidatorEventType {
	CustomUnknown = 0,  ///< Not in specification, only for default-initializing variables.

	// If Result A is 1-255
// 	Credit,  ///< Bill accepted, credit the customer
// 	PendingCredit,  ///< Bill held in escrow, decide whether to accept it

	// If Result A is 0
	Reject,  ///< Bill rejected and returned to customer
	FraudAttempt,  ///< Fraud detected. Possible machine alarm.
	FatalError,  ///< Service callout
	Status,  ///< Informational only
};



/// Get displayable name
inline QString ccBillValidatorEventTypeGetDisplayableName(CcBillValidatorEventType type)
{
	static QMap<CcBillValidatorEventType, QString> name_map = {
		{CcBillValidatorEventType::CustomUnknown, QObject::tr("CustomUnknown")},
// 		{CcBillValidatorEventType::Credit, QObject::tr("Credit")},
// 		{CcBillValidatorEventType::PendingCredit, QObject::tr("PendingCredit")},
		{CcBillValidatorEventType::Reject, QObject::tr("Reject")},
		{CcBillValidatorEventType::FraudAttempt, QObject::tr("FraudAttempt")},
		{CcBillValidatorEventType::FatalError, QObject::tr("FatalError")},
		{CcBillValidatorEventType::Status, QObject::tr("Status")},
	};
	return name_map.value(type, QString());
}



/// Get event type for CcBillValidatorErrorCode
inline CcBillValidatorEventType ccBillValidatorErrorCodeGetEventType(CcBillValidatorErrorCode status)
{
	static QMap<CcBillValidatorErrorCode, CcBillValidatorEventType> type_map = {
		{CcBillValidatorErrorCode::MasterInhibitActive, CcBillValidatorEventType::Status},
		{CcBillValidatorErrorCode::BillReturnedFromEscrow, CcBillValidatorEventType::Status},
		{CcBillValidatorErrorCode::InvalidBillValidationFail, CcBillValidatorEventType::Reject},
		{CcBillValidatorErrorCode::InvalidBillTransportProblem, CcBillValidatorEventType::Reject},
		{CcBillValidatorErrorCode::InhibitedBillOnSerial, CcBillValidatorEventType::Status},
		{CcBillValidatorErrorCode::InhibitedBillOnDipSwitches, CcBillValidatorEventType::Status},
		{CcBillValidatorErrorCode::BillJammedInTransportUnsafeMode, CcBillValidatorEventType::FatalError},
		{CcBillValidatorErrorCode::BillJammedInStacker, CcBillValidatorEventType::FatalError},
		{CcBillValidatorErrorCode::BillPulledBackwards, CcBillValidatorEventType::FraudAttempt},
		{CcBillValidatorErrorCode::BillTamper, CcBillValidatorEventType::FraudAttempt},
		{CcBillValidatorErrorCode::StackerOk, CcBillValidatorEventType::Status},
		{CcBillValidatorErrorCode::StackerRemoved, CcBillValidatorEventType::Status},
		{CcBillValidatorErrorCode::StackerInserted, CcBillValidatorEventType::Status},
		{CcBillValidatorErrorCode::StackerFaulty, CcBillValidatorEventType::FatalError},
		{CcBillValidatorErrorCode::StackerFull, CcBillValidatorEventType::Status},
		{CcBillValidatorErrorCode::StackerJammed, CcBillValidatorEventType::FatalError},
		{CcBillValidatorErrorCode::BillJammedInTransportSafeMode, CcBillValidatorEventType::FatalError},
		{CcBillValidatorErrorCode::OptoFraudDetected, CcBillValidatorEventType::FraudAttempt},
		{CcBillValidatorErrorCode::StringFraudDetected, CcBillValidatorEventType::FraudAttempt},
		{CcBillValidatorErrorCode::AntiStringMechanismFaulty, CcBillValidatorEventType::FatalError},
		{CcBillValidatorErrorCode::BarcodeDetected, CcBillValidatorEventType::Status},
		{CcBillValidatorErrorCode::UnknownBillTypeStacked, CcBillValidatorEventType::Status},
		{CcBillValidatorErrorCode::CustomNoError, CcBillValidatorEventType::FatalError},
	};
	return type_map.value(status, CcBillValidatorEventType::FatalError);
}

/*
/// Get event type for CcBillValidatorErrorCode
inline CcBillValidatorEventType ccBillValidatorSuccessCodeGetEventType(CcBillValidatorSuccessCode status)
{
	static QMap<CcBillValidatorSuccessCode, CcBillValidatorEventType> type_map = {
		{CcBillValidatorSuccessCode::ValidatedAndAccepted, CcBillValidatorEventType::Credit},
		{CcBillValidatorSuccessCode::ValidatedAndHeldInEscrow, CcBillValidatorEventType::PendingCredit},
		{CcBillValidatorSuccessCode::CustomUnknown, CcBillValidatorEventType::FatalError},
	};
	return type_map.value(status, CcBillValidatorEventType::FatalError);
}
*/


/// This is a parameter for RouteBill command.
enum class CcBillRouteCommandType : quint8 {
	ReturnBill = 0,  ///< Reject
	RouteToStacker = 1,  ///< Accept
	IncreaseTimeout = 255,  ///< Give more time to decide
};



/// Get displayable name
inline QString ccBillRouteCommandTypeGetDisplayableName(CcBillRouteCommandType type)
{
	static QMap<CcBillRouteCommandType, QString> name_map = {
		{CcBillRouteCommandType::ReturnBill, QObject::tr("ReturnBill")},
		{CcBillRouteCommandType::RouteToStacker, QObject::tr("RouteToStacker")},
		{CcBillRouteCommandType::IncreaseTimeout, QObject::tr("IncreaseTimeout")},
	};
	return name_map.value(type, QString());
}



/// This is a returned value of the RouteBill command.
enum class CcBillRouteStatus : quint8 {
	Routed = 0,  ///< The bill has been routed to stacker. Corresponds to ACK reply.
	EscrowEmpty = 254,  ///< The escrow is empty, cannot route
	FailedToRoute = 255,  ///< Failed to route
};



/// Get displayable name
inline QString ccBillRouteStatusGetDisplayableName(CcBillRouteStatus type)
{
	static QMap<CcBillRouteStatus, QString> name_map = {
		{CcBillRouteStatus::Routed, QObject::tr("Routed")},
		{CcBillRouteStatus::EscrowEmpty, QObject::tr("EscrowEmpty")},
		{CcBillRouteStatus::FailedToRoute, QObject::tr("FailedToRoute")},
	};
	return name_map.value(type, QString());
}



/// ccTalk event data, as returned by ReadBufferedBillEvents and ReadBufferedCredit commands
struct CcEventData {

	/// Container requirement
	CcEventData() = default;

	/// Constructor
	CcEventData(quint8 arg_result_A, quint8 arg_result_B, CcCategory device_category)
			: result_A(arg_result_A), result_B(arg_result_B)
	{
		if (device_category == CcCategory::CoinAcceptor) {
			if (result_A == 0) {  // error
				coin_id = 0;
				coin_error_code = CcCoinAcceptorEventCode(result_B);
			} else {
				coin_id = result_A;
				coin_sorter_path = result_B;
			}
		} else if (device_category == CcCategory::BillValidator) {
			if (result_A == 0) {  // error
				bill_id = 0;
				bill_error_code = CcBillValidatorErrorCode(result_B);
				bill_event_type = ccBillValidatorErrorCodeGetEventType(bill_error_code);
			} else {
				bill_id = result_A;
				bill_success_code = CcBillValidatorSuccessCode(result_B);
// 				bill_event_type = ccBillValidatorSuccessCodeGetEventType(bill_success_code);
			}
		}
	}

	[[nodiscard]] bool hasError() const
	{
		return result_A == 0;
	}


	quint8 result_A = 0;  ///< Credit (coin position) if in [1-255] range and error_code contains sorter_path. If 0, error_code contains CcCoinAcceptorEventCode.
	quint8 result_B = 0;  ///< If credit, then this is a sorter path (unused). If error, this is an error code.

	quint8 coin_id = 0;  ///< 1-16. Coin ID number (position in GetCoinId). 0 means error.
	CcCoinAcceptorEventCode coin_error_code = CcCoinAcceptorEventCode::NoError;
	quint8 coin_sorter_path = 0;  ///< 0 if unsupported.

	quint8 bill_id = 0;  ///< 1-16, Bill ID number (position in GetBillId). 0 means error.
	CcBillValidatorErrorCode bill_error_code = CcBillValidatorErrorCode::CustomNoError;  ///< If result A is 0.
	CcBillValidatorSuccessCode bill_success_code = CcBillValidatorSuccessCode::CustomUnknown;  ///< If result A is 1-255.
	CcBillValidatorEventType bill_event_type = CcBillValidatorEventType::CustomUnknown;  ///< Event type (both error and success)

};



/// Get coin values according to coin code (cctalk spec Appendix 3 (2.1))
inline quint64 ccCoinValueCodeGetValue(const QByteArray& three_char_code, quint8& decimal_places)
{
	// coin code -> (coin_value, decimal_places).
	static QMap<QByteArray, QPair<quint64, quint8>> value_map = {
		{"5m0", {5, 3}},  // 0.005
		{"10m", {1, 2}},  // 0.01
		{".01", {1, 2}},  // 0.01
		{"20m", {2, 2}},  // 0.02
		{".02", {2, 2}},  // 0.02
		{"25m", {25, 3}},  // 0.025
		{"50m", {5, 2}},  // 0.05
		{".05", {5, 2}},  // 0.05
		{".10", {1, 1}},  // 0.10
		{".20", {2, 1}},  // 0.20
		{".25", {25, 2}},  // 0.25
		{".50", {5, 1}},  // 0.50
		{"001", {1, 0}},  // 1
		{"002", {1, 0}},  // 2
		{"2.5", {25, 1}},  // 2.5
		{"005", {5, 0}},  // 5
		{"010", {10, 0}},  // 10
		{"020", {20, 0}},  // 20
		{"025", {25, 0}},  // 25
		{"050", {50, 0}},  // 50
		{"100", {100, 0}},  // 100
		{"200", {200, 0}},  // 200
		{"250", {250, 0}},  // 250
		{"500", {500, 0}},  // 500
		{"1K0", {1000, 0}},  // 1 000
		{"2K0", {2000, 0}},  // 2 000
		{"2K5", {2500, 0}},  // 2 500
		{"5K0", {5000, 0}},  // 5 000
		{"10K", {10000, 0}},  // 10 000
		{"20K", {20000, 0}},  // 20 000
		{"25K", {25000, 0}},  // 25 000
		{"50K", {50000, 0}},  // 50 000
		{"M10", {100000, 0}},  // 100 000
		{"M20", {200000, 0}},  // 200 000
		{"M25", {250000, 0}},  // 250 000
		{"M50", {500000, 0}},  // 500 000
		{"1M0", {1000000, 0}},  // 1 000 000
		{"2M0", {2000000, 0}},  // 2 000 000
		{"2M5", {2500000, 0}},  // 2 500 000
		{"5M0", {5000000, 0}},  // 5 000 000
		{"10M", {10000000, 0}},  // 10 000 000
		{"20M", {20000000, 0}},  // 20 000 000
		{"25M", {25000000, 0}},  // 25 000 000
		{"50M", {50000000, 0}},  // 50 000 000
		{"G10", {100000000, 0}},  // 100 000 000
	};

	QPair<quint64, quint8> value = value_map.value(three_char_code, QPair<quint64, quint8>());
	decimal_places = value.second;
	return value.first;
}



/// Country scaling data, as returned by GetCountryScalingFactor command.
struct CcCountryScalingData {
	/// Scaling factor from country scaling data.
	/// The bill identifier values should be
	/// multiplied by this to get cents.
	quint16 scaling_factor = 1;

	/// Decimal places from country scaling data. 2 for USD (10^2 cents in USD)
	quint8 decimal_places = 0;

	/// If country code is unsupported, this returns false.
	[[nodiscard]] bool isValid() const
	{
		return scaling_factor != 0 || decimal_places != 0;
	}
};



/// ccTalk coin / bill identifier, as returned by GetBillId and GetCoinId commands.
struct CcIdentifier {

	/// QMap requirement
	CcIdentifier() = default;

	/// Parse ID string and store the results
	explicit CcIdentifier(QByteArray arg_id_string)
			: id_string(std::move(arg_id_string))
	{
		if (arg_id_string.size() == 7) {  // Bills
			country = arg_id_string.left(2);
			issue_code = arg_id_string.at(arg_id_string.size() - 1);
			value_code = arg_id_string.mid(2, 4).toUInt();

		} else if (arg_id_string.size() == 6) {  // Coins
			country = arg_id_string.left(2);
			issue_code = arg_id_string.at(arg_id_string.size() - 1);
			value_code = ccCoinValueCodeGetValue(arg_id_string.mid(2, 3), coin_decimals);

		} else {
			DBG_ASSERT(0);
		}
	}

	/// Set country scaling data for bills and coins. For bills this can be retrieved
	/// from the device, while for coins it has to be predefined by us.
	void setCountryScalingData(CcCountryScalingData data)
	{
		country_scaling_data = data;
	}

	/// Get coin / bill value. The returned value should be divided by \c 10^divisor
	/// to get a value in country currency (e.g. USD).
	/// For coin acceptors the divisor is always 1.
	quint64 getValue(quint64& divisor) const
	{
		divisor = country_scaling_data.decimal_places + coin_decimals;
		return value_code * country_scaling_data.scaling_factor;
	}


	QByteArray id_string;  ///< Bill / coin identifier, e.g. "GE0005A" for the first ("A") issue of Georgian 5 lari (value code 0005).
	QByteArray country;  ///< Country identifier, e.g. "GE".
	char issue_code = 0;  ///< Issue code (A, B, C, ...), to differentiate various issues of the same-value coin.
	quint64 value_code = 0;  ///< Value code (before country scaling, if it's a bill).
	quint8 coin_decimals = 0;  ///< Value code should be divided by 10^coin_decimals to get the real value.

	CcCountryScalingData country_scaling_data;  ///< Coin / bill scaling data
};



}



#endif

