/**************************************************************************
Copyright: (C) 2014 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QCloseEvent>
#include <QScopedPointer>

#include "cctalk/bill_validator_device.h"
#include "cctalk/coin_acceptor_device.h"




// Forward declaration for QScopedPointer
namespace Ui {
	class MainWindow;
}



///	Main IDE window. The application exits when this window is closed.
class MainWindow : public QMainWindow {
	Q_OBJECT
	public:

		/// Construct the main IDE window, loading all its UI components (from plugins as well).
		MainWindow();

		/// Destructor
		~MainWindow() override;

		/// Window close event handler, reimplemented from QWidget::closeEvent().
		/// Invokes closeRequested().
		void closeEvent(QCloseEvent* ev) override;

		/// Try to close the program (also ask about unsaved documents, etc...).
		/// Window close and menu quit actions invoke this.
		/// \return false, this means that close was unsuccessful (e.g. the user declined to save).
		bool closeRequested();


	protected slots:

		/// Launch device-handling threads
		void runSerialThreads();


		/// Log message to message log
		void logMessage(QString msg);


		/// Button click callback
		void onStartStopBillValidatorClicked();

		/// Button click callback
		void onToggleBillAcceptClicked();


		/// Button click callback
		void onStartStopCoinAcceptorClicked();

		/// Button click callback
		void onToggleCoinAcceptClicked();


		/// Action handler
		void onQuitActionTriggered();


	private:

		QScopedPointer<Ui::MainWindow> ui;  ///< Designer-created class instance

		qtcc::BillValidatorDevice bill_validator_;  ///< Bill validator communicator (launches separate thread)
		qtcc::CoinAcceptorDevice coin_acceptor_;  ///< Coin acceptor communicator (launches separate thread)

};



#endif  // hg

/// @}
