
#include "logger.h"

logger::logger()
{
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
}

std::vector<std::string> logger::getMessages()
{
	return msgQueue;
}

logger::~logger()
{

}
