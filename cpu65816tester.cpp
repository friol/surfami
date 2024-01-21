
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

int cpu65816tester::executeTest()
{
	glbTheLogger.logMsg("Starting 65816 test.");

	int testId = 0;
	for (auto& cpuTest : testData)
	{
		bool breakkk = false;

		std::string testName = cpuTest["name"];
		unsigned short int initialPC = testData[testId]["initial"]["pc"];
		unsigned short int initialSP = testData[testId]["initial"]["s"];
		unsigned char initialP = testData[testId]["initial"]["p"];
		unsigned short int initialA = testData[testId]["initial"]["a"];
		unsigned short int initialX = testData[testId]["initial"]["x"];
		unsigned short int initialY = testData[testId]["initial"]["y"];
		unsigned char initialDBR = testData[testId]["initial"]["dbr"];
		unsigned short int initialD = testData[testId]["initial"]["d"];
		unsigned char initialPBR = testData[testId]["initial"]["pbr"];
		unsigned char initialE = testData[testId]["initial"]["e"];

		unsigned char* pmem = pMMU->getInternalRAMPtr();
		for (int addr = 0;addr < 0x1000000;addr++)
		{
			pmem[addr] = 0;
		}

		for (auto& ramPos : testData[testId]["initial"]["ram"])
		{
			unsigned int address = ramPos[0];
			unsigned char val = ramPos[1];
			pMMU->write8(address, val);
		}

		std::string flagz = std::bitset<8>(initialP).to_string();
		glbTheLogger.logMsg("Performing test [" + testName + "] emulation mode: " + std::to_string(initialE));
		glbTheLogger.logMsg("NVMXDIZC");
		glbTheLogger.logMsg(flagz);

		pCPU->reset();
		pCPU->setState(initialPC, initialA, initialX, initialY, initialSP, initialDBR, initialD, initialPBR, initialP, initialE);
		int cycles=pCPU->stepOne();

		unsigned int finalPC = testData[testId]["final"]["pc"];
		unsigned int finalA = testData[testId]["final"]["a"];
		unsigned int finalX = testData[testId]["final"]["x"];
		unsigned int finalY = testData[testId]["final"]["y"];
		unsigned short int finalSP = testData[testId]["final"]["s"];
		unsigned char finalDBR = testData[testId]["final"]["dbr"];
		unsigned short int finalD = testData[testId]["final"]["d"];
		unsigned char finalPBR = testData[testId]["final"]["pbr"];
		unsigned char finalP = testData[testId]["final"]["p"];

		if (pCPU->getPC() != finalPC)
		{
			glbTheLogger.logMsg("PC [" + std::to_string(pCPU->getPC()) + "] and reference [" + std::to_string(finalPC) + "] PC don't match.");
			breakkk = true;
		}
		if (pCPU->getA() != finalA)
		{
			glbTheLogger.logMsg("A [" + std::to_string(pCPU->getA()) + "] and reference [" + std::to_string(finalA) + "] A don't match.");
			breakkk = true;
		}
		if (pCPU->getX() != finalX)
		{
			glbTheLogger.logMsg("X [" + std::to_string(pCPU->getX()) + "] and reference [" + std::to_string(finalX) + "] X don't match.");
			breakkk = true;
		}
		if (pCPU->getY() != finalY)
		{
			glbTheLogger.logMsg("Y [" + std::to_string(pCPU->getY()) + "] and reference [" + std::to_string(finalY) + "] Y don't match.");
			breakkk = true;
		}
		if (pCPU->getSP() != finalSP)
		{
			glbTheLogger.logMsg("SP [" + std::to_string(pCPU->getSP()) + "] and reference [" + std::to_string(finalSP) + "] don't match.");
			breakkk = true;
		}
		if (pCPU->getDBR() != finalDBR)
		{
			glbTheLogger.logMsg("DBR [" + std::to_string(pCPU->getDBR()) + "] and reference [" + std::to_string(finalDBR) + "] don't match.");
			breakkk = true;
		}
		if (pCPU->getD() != finalD)
		{
			glbTheLogger.logMsg("D [" + std::to_string(pCPU->getD()) + "] and reference [" + std::to_string(finalD) + "] don't match.");
			breakkk = true;
		}
		if (pCPU->getPB() != finalPBR)
		{
			glbTheLogger.logMsg("PBR (regPB) [" + std::to_string(pCPU->getPB()) + "] and reference [" + std::to_string(finalPBR) + "] don't match.");
			breakkk = true;
		}
		if (pCPU->getP() != finalP)
		{
			std::string cpuP = std::bitset<8>(pCPU->getP()).to_string();
			std::string finP = std::bitset<8>(finalP).to_string();

			glbTheLogger.logMsg("P [" + cpuP + "] and reference [" + finP + "] don't match.");
			breakkk = true;
		}

		for (auto& ramPos : testData[testId]["final"]["ram"])
		{
			unsigned int address = ramPos[0];
			unsigned char val = ramPos[1];
			unsigned char valAtAddress = pMMU->read8(address);
			if (valAtAddress != val)
			{
				glbTheLogger.logMsg("RAM at [" + std::to_string(address) + 
					"] value [" + std::to_string(valAtAddress) + 
					"] different from test val ["+std::to_string(val) + "].");
				return 1;
			}
		}

		// cycles
		int referenceCycles = 0;
		for (auto& cyc : testData[testId]["cycles"])
		{
			cyc = cyc;
			referenceCycles += 1;
		}

		if (referenceCycles != cycles)
		{
			glbTheLogger.logMsg("Emulated cycles [" + std::to_string(cycles) +
				"] different from reference val [" + std::to_string(referenceCycles) + "].");
			return 1;
		}


		if (breakkk)
		{
			glbTheLogger.logMsg("Testing ended with error");
			return 1;
		}

		testId += 1;
	}

	glbTheLogger.logMsg("Test ended successfully");
	return 0;
}

cpu65816tester::~cpu65816tester()
{
}
