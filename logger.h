#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <vector>
#include <cstdio>

class logger
{
private:

	FILE* logFile;

	const int maxQueueDim = 100;
	std::vector<std::string> msgQueue;

public:

	logger();
	void logMsg(std::string msg);
	std::vector<std::string> getMessages();
	~logger();

};

#endif
