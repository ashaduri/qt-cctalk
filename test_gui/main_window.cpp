/**************************************************************************
Copyright: (C) 2014 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#include <QMessageBox>
#include <QTimer>
#include <Qt>
#include <QSerialPortInfo>
#include <QAction>
#include <QPointer>
#include <cmath>

#include "cctalk/helpers/debug.h"
#include "app_settings.h"
#include "main_window.h"
#include "ui_main_window.h"
#include "gui_application.h"
#include "cctalk_tools.h"



using qtcc::CcDeviceState;



MainWindow::MainWindow()
{
	ui.reset(new Ui::MainWindow());
	ui->setupUi(this);

	// Still show the tooltips if the window is unfocusted.
	setAttribute(Qt::WA_AlwaysShowToolTips);

	// Main menubar
	{
		QPointer<QAction> action_quit(new QAction(QObject::tr("&Quit"), this));
		connect(action_quit.data(), SIGNAL(triggered()), this, SLOT(onQuitActionTriggered()));
		ui->menu_file->addAction(action_quit.data());
	}

	connect(ui->start_stop_bill_validator_button, SIGNAL(clicked(bool)), this, SLOT(onStartStopBillValidatorClicked()));
	connect(ui->toggle_bill_accept_button, SIGNAL(clicked(bool)), this, SLOT(onToggleBillAcceptClicked()));

	connect(ui->start_stop_coin_acceptor_button, SIGNAL(clicked(bool)), this, SLOT(onStartStopCoinAcceptorClicked()));
	connect(ui->toggle_coin_accept_button, SIGNAL(clicked(bool)), this, SLOT(onToggleCoinAcceptClicked()));

	// Restore geometry
	if (AppSettings::valueExists(QStringLiteral("main_window/geometry"))) {
		restoreGeometry(AppSettings::getValue<QByteArray>(QStringLiteral("main_window/geometry")));
	} else {
		move(QPoint(10, 10));
		resize(QSize(900, 600));
	}

	// Restore toolbar / dockwidget states
	if (AppSettings::valueExists(QStringLiteral("main_window/window_state"))) {
		restoreState(AppSettings::getValue<QByteArray>(QStringLiteral("main_window/window_state")));
	}

	QTimer::singleShot(0, this, SLOT(runSerialThreads()));
}



MainWindow::~MainWindow()
{
	debug_out_dump("MainWindow deleting...");
}



void MainWindow::closeEvent(QCloseEvent* ev)
{
	if (this->closeRequested()) {
		ev->accept();  // accept close
	} else {
		ev->ignore();
	}
}



bool MainWindow::closeRequested()
{
	// Save geometry and toolbar / dock window state
	AppSettings::setValue(QStringLiteral("main_window/geometry"), saveGeometry());
	AppSettings::setValue(QStringLiteral("main_window/window_state"), saveState());

	GuiApplication::quit();

	return true;  // won't reach
}



void MainWindow::logMessage(QString msg)
{
	msg = ccProcessLoggingMessage(msg, true);
	if (!msg.isEmpty()) {
		ui->status_log_textedit->append(msg);
	}
}



void MainWindow::runSerialThreads()
{
	// Set cctalk options
	QString setup_error = setUpCctalkDevices(&bill_validator_, &coin_acceptor_, [=](QString message) {
		logMessage(message);
	});
	if (!setup_error.isEmpty()) {
		logMessage(setup_error);
		return;
	}

	// Bill validator
	{
		connect(&bill_validator_, &qtcc::CctalkDevice::creditAccepted, [this]([[maybe_unused]] quint8 id, const qtcc::CcIdentifier& identifier) {
			const char* prop_name = "integral_value";

			quint64 existing_value = ui->entered_bills_lineedit->property(prop_name).toULongLong();
			quint64 divisor = 0;
			quint64 value = identifier.getValue(divisor);  // in cents

			existing_value += value;
			ui->entered_bills_lineedit->setProperty(prop_name, existing_value);
			ui->entered_bills_lineedit->setText(QString::number(double(existing_value) / std::pow(10., int(divisor)), 'f', 2));
		});
	}

	// Coin acceptor
	{
		connect(&coin_acceptor_, &qtcc::CctalkDevice::creditAccepted, [this]([[maybe_unused]] quint8 id, const qtcc::CcIdentifier& identifier) {
			const char* prop_name = "integral_value";

			quint64 existing_value = ui->entered_coins_lineedit->property(prop_name).toULongLong();
			quint64 divisor = 0;
			quint64 value = identifier.getValue(divisor);  // in cents

			existing_value += value;
			ui->entered_coins_lineedit->setProperty(prop_name, existing_value);
			ui->entered_coins_lineedit->setText(QString::number(double(existing_value) / std::pow(10., int(divisor)), 'f', 2));
		});
	}
}



void MainWindow::onStartStopBillValidatorClicked()
{
	if (bill_validator_.getDeviceState() == CcDeviceState::ShutDown) {
		bill_validator_.getLinkController().openPort([this](const QString& error_msg) {
			if (error_msg.isEmpty()) {
				bill_validator_.initialize([]([[maybe_unused]] const QString& init_error_msg) { });
			}
		});
	} else {
		bill_validator_.shutdown([this]([[maybe_unused]] const QString& error_msg) {
			// Close the port once the device is "shut down"
			bill_validator_.getLinkController().closePort();
		});
	}
}



void MainWindow::onToggleBillAcceptClicked()
{
	bool accepting = bill_validator_.getDeviceState() == CcDeviceState::NormalAccepting;
	bool rejecting = bill_validator_.getDeviceState() == CcDeviceState::NormalRejecting;
	if (accepting || rejecting) {
		CcDeviceState new_state = accepting ? CcDeviceState::NormalRejecting : CcDeviceState::NormalAccepting;
		bill_validator_.requestSwitchDeviceState(new_state, []([[maybe_unused]] const QString& error_msg) {
			// nothing
		});
	} else {
		logMessage(tr("! Cannot toggle bill accept mode, the device is in %1 state.")
				.arg(ccDeviceStateGetDisplayableName(bill_validator_.getDeviceState())));
	}
}



void MainWindow::onStartStopCoinAcceptorClicked()
{
	if (coin_acceptor_.getDeviceState() == CcDeviceState::ShutDown) {
		coin_acceptor_.getLinkController().openPort([this](const QString& error_msg) {
			if (error_msg.isEmpty()) {
				coin_acceptor_.initialize([]([[maybe_unused]] const QString& init_error_msg) { });
			}
		});
	} else {
		coin_acceptor_.shutdown([this]([[maybe_unused]] const QString& error_msg) {
			// Close the port once the device is "shut down"
			coin_acceptor_.getLinkController().closePort();
		});
	}
}



void MainWindow::onToggleCoinAcceptClicked()
{
	bool accepting = coin_acceptor_.getDeviceState() == CcDeviceState::NormalAccepting;
	bool rejecting = coin_acceptor_.getDeviceState() == CcDeviceState::NormalRejecting;
	if (accepting || rejecting) {
		CcDeviceState new_state = accepting ? CcDeviceState::NormalRejecting : CcDeviceState::NormalAccepting;
		coin_acceptor_.requestSwitchDeviceState(new_state, []([[maybe_unused]] const QString& error_msg) {
			// nothing
		});
	} else {
		logMessage(tr("! Cannot toggle coin accept mode, the device is in %1 state.")
				.arg(ccDeviceStateGetDisplayableName(bill_validator_.getDeviceState())));
	}
}



void MainWindow::onQuitActionTriggered()
{
	this->closeRequested();
}




