
#include "logger.h"
#include "cpu65816tester.h"

extern logger glbTheLogger;

//

cpu65816tester::cpu65816tester()
{
}

int cpu65816tester::loadTest(std::string filename)
{
	std::ifstream f(filename);
	testData = json::parse(f);
	return 0;
}

void cpu65816tester::executeTest()
{
	glbTheLogger.logMsg("Starting 65816 test.");

	int testId = 0;
	std::string testName = testData[testId]["name"];
	int initialPC = testData[testId]["initial"]["pc"];

	glbTheLogger.logMsg("Performing test "+testName+" Initial PC:"+std::to_string(initialPC));
}

cpu65816tester::~cpu65816tester()
{
}
