
#include <chrono>
#include <time.h>
#include "logger.h"

logger::logger()
{
	errno_t err;
	err=fopen_s(&logFile,"surFami.log", "wt");
}

void logger::logMsg(std::string msg)
{
	if (msgQueue.size() < maxQueueDim)
	{
		msgQueue.push_back(msg);
	}
	else
	{
		msgQueue.erase(msgQueue.begin());
		msgQueue.push_back(msg);
	}

	if (log2file)
	{
		char timestampBuf[1024];
		memset(timestampBuf, 0, 1024);
		struct tm lt;
		time_t now = time(0);
		localtime_s(&lt, &now);

		std::chrono::system_clock::time_point chronot = std::chrono::system_clock::now();
		const std::chrono::duration<double> tse = chronot.time_since_epoch();
		std::chrono::seconds::rep milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(tse).count() % 1000;

		snprintf(timestampBuf, 1024, "%02d/%02d/%02d %02d:%02d:%02d.%llu", lt.tm_mday, lt.tm_mon + 1, lt.tm_year % 100, lt.tm_hour, lt.tm_min, lt.tm_sec, milliseconds);
		fprintf(logFile, timestampBuf);
		fprintf(logFile, " ");
		fprintf(logFile, msg.c_str());
		fprintf(logFile, "\n");
	}
}

std::vector<std::string> logger::getMessages()
{
	return msgQueue;
}

logger::~logger()
{
	fclose(logFile);
}
