/**************************************************************************
Copyright: (C) 2014 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#ifndef ASYNC_SERIALIZER_H
#define ASYNC_SERIALIZER_H

#include <QObject>
#include <QVector>
#include <QTimer>

#include <functional>



/// Serialize multiple job executions
class AsyncSerializer : public QObject {
	Q_OBJECT
	public:

		/// Constructor argument
		using FinishHandler = std::function<void(AsyncSerializer* serializer)>;

		/// Job executor function.
		/// If the function wishes to prevent the consequent functions from executing,
		/// it should call stopSequence().
		using ExecutorFunc = std::function<void(AsyncSerializer* serializer)>;


		/// Constructor. finish_handler is executed when all functions are executed or
		/// when stop() is called manually.
		explicit AsyncSerializer(FinishHandler finish_handler, bool delete_this_on_finish = true);

		/// Add an executor function to function list.
		void add(const ExecutorFunc& func);

		/// Start executing the functions. This function is non-blocking.
		/// When a function call is finished, the next function from the list is executed
		/// on the next event loop iteration (QTimer::singleShot()).
		/// Returns true if execution has started.
		bool start();


		/// This should be called from executor function's "finished" callback.
		/// If \c queue_next is false, the next function doesn't get called and
		/// the executors list is cleared to avoid holding
		/// any variable references in executor functors/lambdas.
		void continueSequence(bool queue_next);


	protected slots:

		/// Handle the previous job result and execute the next function.
		void executeNextFunc();


	private:

		QTimer timer_;  ///< Single-shot timer
		FinishHandler finish_handler_;  ///< Finish handler
		bool delete_this_on_finish_ = false;  ///< Delete this object on finish
		QVector<ExecutorFunc> executors_;  ///< Executor functions
		int current_func_index_ = 0;  ///< An index of currently executing function in executors_.

};




#endif  // hg
