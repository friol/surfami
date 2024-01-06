#ifndef CPU65816TESTER_H
#define CPU65816TESTER_H

#include <string>
#include <fstream>
#include <bitset>
#include "testMMU.h"
#include "cpu5a22.h"
#include "json.hpp"
using json = nlohmann::json;


class cpu65816tester
{
private:

	json testData;
	testMMU* pMMU;
	cpu5a22* pCPU;

public:

	cpu65816tester(testMMU& ammu,cpu5a22& acpu);
	int loadTest(std::string filename);
	int executeTest();
	~cpu65816tester();
};

#endif
