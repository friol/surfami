
#include <time.h>
#include "logger.h"

logger::logger()
{
	logFile = fopen("log.log", "wt");
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

	char timestampBuf[1024];
	memset(timestampBuf, 0, 1024);
	time_t t = time(NULL);
	struct tm* lt = localtime(&t);
	snprintf(timestampBuf, 1024, "%02d/%02d/%02d %02d:%02d:%02d", lt->tm_mday, lt->tm_mon + 1,lt->tm_year % 100, lt->tm_hour, lt->tm_min, lt->tm_sec);
	fprintf(logFile, timestampBuf);
	fprintf(logFile, " ");
	fprintf(logFile, msg.c_str());
	fprintf(logFile, "\n");
}

std::vector<std::string> logger::getMessages()
{
	return msgQueue;
}

logger::~logger()
{
	fclose(logFile);
}
