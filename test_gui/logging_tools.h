/**************************************************************************
Copyright: (C) 2014 - 2021 Alexander Shaduri
License: BSD-3-Clause
***************************************************************************/

#ifndef LOGGING_TOOLS_H
#define LOGGING_TOOLS_H

#include <QString>
#include <QPair>
#include <QVector>



/// Message accumulator. Helps with identifying duplicate messages.
struct MessageAccumulator {
	MessageAccumulator(int buf_size) : buf(buf_size)
	{ }

	inline int push(QString msg)
	{
		if (buf[index].first == msg) {
			++(buf[index].second);
		} else {
			buf[index].first = std::move(msg);
			buf[index].second = 1;
		}
		index = (index+1) % buf.size();

		int min_repetitions = buf[index].second;
		for (auto p : buf) {
			min_repetitions = std::min(min_repetitions, p.second);
		}
		return min_repetitions;
	}

	QVector<QPair<QString, int>> buf;
	int index = 0;
};




#endif  // hg
