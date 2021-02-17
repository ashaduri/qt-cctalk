/**************************************************************************
Copyright: (C) 2014 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#include <utility>

#include "async_serializer.h"
#include "debug.h"



AsyncSerializer::AsyncSerializer(FinishHandler finish_handler, bool delete_this_on_finish)
		: finish_handler_(std::move(finish_handler)), delete_this_on_finish_(delete_this_on_finish)
{
	timer_.setSingleShot(true);
}



void AsyncSerializer::add(const AsyncSerializer::ExecutorFunc& func)
{
	executors_ << func;
}



bool AsyncSerializer::start()
{
	if (executors_.empty()) {
		finish_handler_(this);
		if (delete_this_on_finish_) {
			delete this;
		}
		return false;
	}
	current_func_index_ = -1;
	executeNextFunc();
	return true;
}



void AsyncSerializer::continueSequence(bool queue_next)
{
	// If there are still some functions to go and the user wishes so, queue the next one.
	if (queue_next && (current_func_index_ + 1 < executors_.size())) {
		QObject::connect(&timer_, SIGNAL(timeout()), this, SLOT(executeNextFunc()), Qt::QueuedConnection);
		timer_.start(0);  // it's a single-shot timer
	} else {
		current_func_index_ = -1;
		executors_.clear();
		finish_handler_(this);
		delete this;
	}
}



void AsyncSerializer::executeNextFunc()
{
	++current_func_index_;  // initially -1

	QTimer::disconnect(&timer_, nullptr, nullptr, nullptr);

	ExecutorFunc func = executors_.value(current_func_index_);
	DBG_ASSERT(func);
	if (!func) {  // we reached the end (shouldn't happen)
		current_func_index_ = -1;
		return;
	}

	// Run the next job.
	func(this);  // This may call stop().
}



