/**************************************************************************
Copyright: (C) 2014 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#ifndef GUI_APPLICATION_H
#define GUI_APPLICATION_H

#include <QApplication>
#include <QScopedPointer>


class MainWindow;


/// This class handles application startup / quit and encapsulates QGuiApplication-derived Application.
class GuiApplication : public QApplication {
	Q_OBJECT
	public:

		/// Constructor. The arguments are the same as in QGuiApplication::QGuiApplication().
		/// Construct only ONCE.
		GuiApplication(int& par_argc, char**& par_argv);

		/// Destructor (scoped pointer member requirement)
		~GuiApplication() override;

		/// Initialize everything and run the main loop. Use quit() to exit it.
		/// \return exit status.
		int run();

		/// Manually stop the main loop. quitCleanup() is called right after exiting the main loop.
		/// Note that if the application is being stopped by the windowing system, only
		/// quitCleanup() is called.
		static void quit();


	protected slots:

		/// Slot. This is called whenever the main loop exits.
		void quitCleanup();


	private:

		QScopedPointer<MainWindow> main_window_;  ///< The main window

};




#endif

