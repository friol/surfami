#ifndef SPC700TESTER_H
#define SPC700TESTER_H

#include <string>
#include <fstream>
#include <bitset>
#include "testMMU.h"
#include "apu.h"
#include "json.hpp"
using json = nlohmann::json;


class spc700tester
{
private:

	json testData;
	testMMU* pMMU;
	apu* pAPU;

public:

	spc700tester(testMMU& ammu, apu& theAPU);
	int loadTest(std::string filename);
	int executeTest();
	~spc700tester();
};

#endif
