
/* guess what */

#include "logger.h"
#include "spc700tester.h"

extern logger glbTheLogger;

spc700tester::spc700tester(testMMU& ammu, apu& theAPU)
{
	pAPU = &theAPU;
	pMMU = &ammu;
	pAPU->useTestMMU();
}

int spc700tester::loadTest(std::string filename)
{
	std::ifstream f(filename);
	testData = json::parse(f);
	return 0;
}

int spc700tester::executeTest()
{
	glbTheLogger.logMsg("Starting SPC700 test.");

	int testId = 0;
	for (auto& cpuTest : testData)
	{
		bool breakkk = false;

		std::string testName = cpuTest["name"];
		unsigned short int initialPC = testData[testId]["initial"]["pc"];
		unsigned short int initialSP = testData[testId]["initial"]["sp"];
		unsigned char initialA = testData[testId]["initial"]["a"];
		unsigned char initialX = testData[testId]["initial"]["x"];
		unsigned char initialY = testData[testId]["initial"]["y"];
		unsigned char initialFlags = testData[testId]["initial"]["psw"];
		
		//unsigned char* pmem = pMMU->getInternalRAMPtr();
		for (int addr = 0;addr < 0x10000;addr++)
		{
			//pmem[addr] = 0;
			pAPU->testMMUWrite8(addr, 0);
		}

		for (auto& ramPos : testData[testId]["initial"]["ram"])
		{
			unsigned int address = ramPos[0];
			unsigned char val = ramPos[1];
			//pMMU->write8(address, val);
			pAPU->testMMUWrite8(address, val);
		}

		std::string flagz = std::bitset<8>(initialFlags).to_string();
		glbTheLogger.logMsg("Performing test [" + testName + "]");
		glbTheLogger.logMsg("NVPBHIZC");
		glbTheLogger.logMsg(flagz);

		pAPU->reset();
		pAPU->setState(initialPC, initialA, initialX, initialY, initialSP, initialFlags);
		int cycles = pAPU->stepOne();

		unsigned short int finalPC = testData[testId]["final"]["pc"];
		unsigned char finalA = testData[testId]["final"]["a"];
		unsigned char finalX = testData[testId]["final"]["x"];
		unsigned char finalY = testData[testId]["final"]["y"];
		unsigned short int finalSP = testData[testId]["final"]["sp"];
		unsigned char finalFlags = testData[testId]["final"]["psw"];
		
		if (pAPU->getPC() != finalPC)
		{
			glbTheLogger.logMsg("PC [" + std::to_string((pAPU->getPC())) + "] and reference [" + std::to_string(finalPC) + "] PC don't match.");
			breakkk = true;
		}
		if (pAPU->getA() != finalA)
		{
			glbTheLogger.logMsg("A [" + std::to_string(pAPU->getA()) + "] and reference [" + std::to_string(finalA) + "] A don't match.");
			breakkk = true;
		}
		if (pAPU->getX() != finalX)
		{
			glbTheLogger.logMsg("X [" + std::to_string(pAPU->getX()) + "] and reference [" + std::to_string(finalX) + "] X don't match.");
			breakkk = true;
		}
		if (pAPU->getY() != finalY)
		{
			glbTheLogger.logMsg("Y [" + std::to_string(pAPU->getY()) + "] and reference [" + std::to_string(finalY) + "] Y don't match.");
			breakkk = true;
		}
		if (pAPU->getSP() != finalSP)
		{
			glbTheLogger.logMsg("SP [" + std::to_string(pAPU->getSP()) + "] and reference [" + std::to_string(finalSP) + "] don't match.");
			breakkk = true;
		}
		if (pAPU->getPSW() != finalFlags)
		{
			std::string cpuP = std::bitset<8>(pAPU->getPSW()).to_string();
			std::string finP = std::bitset<8>(finalFlags).to_string();

			glbTheLogger.logMsg("Final PSW [" + cpuP + "] and reference [" + finP + "] don't match.");
			breakkk = true;
		}

		for (auto& ramPos : testData[testId]["final"]["ram"])
		{
			unsigned int address = ramPos[0];
			unsigned char val = ramPos[1];
			unsigned char valAtAddress = pAPU->testMMURead8(address);
			//unsigned char valAtAddress = pMMU->read8(address);
			if (valAtAddress != val)
			{
				glbTheLogger.logMsg("RAM at [" + std::to_string(address) +
					"] value [" + std::to_string(valAtAddress) +
					"] different from test val [" + std::to_string(val) + "].");
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

spc700tester::~spc700tester()
{
}
