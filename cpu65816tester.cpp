
#include "logger.h"
#include "cpu65816tester.h"

extern logger glbTheLogger;

//

cpu65816tester::cpu65816tester(testMMU& ammu, cpu5a22& acpu)
{
	pCPU = &acpu;
	pMMU = &ammu;
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
	unsigned int initialPC = testData[testId]["initial"]["pc"];
	unsigned short int initialSP = testData[testId]["initial"]["s"];
	unsigned char initialP= testData[testId]["initial"]["p"];
	unsigned short int initialA= testData[testId]["initial"]["a"];
	unsigned short int initialX= testData[testId]["initial"]["x"];
	unsigned short int initialY= testData[testId]["initial"]["y"];
	unsigned char initialDBR= testData[testId]["initial"]["dbr"];
	unsigned short int initialD= testData[testId]["initial"]["d"];
	unsigned char initialPBR= testData[testId]["initial"]["pbr"];
	unsigned char initialE= testData[testId]["initial"]["e"];

	for (auto& ramPos : testData[testId]["initial"]["ram"])
	{
		unsigned int address = ramPos[0];
		unsigned char val = ramPos[1];
		pMMU->write8(address, val);
	}

	pCPU->reset();
	pCPU->setState(initialPC, initialA, initialX, initialY, initialSP, initialDBR, initialD, initialPBR, initialP);

	glbTheLogger.logMsg("Performing test ["+testName+"]");

	pCPU->stepOne();

	unsigned int finalPC = testData[testId]["final"]["pc"];
	unsigned int finalA = testData[testId]["final"]["a"];
	unsigned int finalX = testData[testId]["final"]["x"];
	unsigned int finalY = testData[testId]["final"]["y"];
	unsigned short int finalSP = testData[testId]["final"]["s"];
	unsigned char finalDBR = testData[testId]["final"]["dbr"];
	unsigned short int finalD = testData[testId]["final"]["d"];
	unsigned char finalPBR = testData[testId]["final"]["pbr"];

	if (pCPU->getPC() != finalPC)
	{
		glbTheLogger.logMsg("PC ["+std::to_string(pCPU->getPC()) + "] and final ["+ std::to_string(finalPC) +"] PC don't match.");
	}
	if (pCPU->getA() != finalA)
	{
		glbTheLogger.logMsg("A [" + std::to_string(pCPU->getA()) + "] and final [" + std::to_string(finalA) + "] A don't match.");
	}
	if (pCPU->getX() != finalX)
	{
		glbTheLogger.logMsg("X [" + std::to_string(pCPU->getX()) + "] and final [" + std::to_string(finalX) + "] X don't match.");
	}
	if (pCPU->getY() != finalY)
	{
		glbTheLogger.logMsg("Y [" + std::to_string(pCPU->getY()) + "] and final [" + std::to_string(finalY) + "] Y don't match.");
	}
	if (pCPU->getSP() != finalSP)
	{
		glbTheLogger.logMsg("Y [" + std::to_string(pCPU->getSP()) + "] and final [" + std::to_string(finalY) + "] Y don't match.");
	}
	if (pCPU->getDBR() != finalDBR)
	{
		glbTheLogger.logMsg("DBR [" + std::to_string(pCPU->getDBR()) + "] and final [" + std::to_string(finalDBR) + "] don't match.");
	}
	if (pCPU->getD() != finalD)
	{
		glbTheLogger.logMsg("D [" + std::to_string(pCPU->getD()) + "] and final [" + std::to_string(finalD) + "] don't match.");
	}
	if (pCPU->getPB() != finalPBR)
	{
		glbTheLogger.logMsg("PBR [" + std::to_string(pCPU->getPB()) + "] and final [" + std::to_string(finalPBR) + "] don't match.");
	}

	glbTheLogger.logMsg("Test ended.");
}

cpu65816tester::~cpu65816tester()
{
}
