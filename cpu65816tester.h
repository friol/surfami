#ifndef CPU65816TESTER_H
#define CPU65816TESTER_H

#include <string>
#include <fstream>
#include "json.hpp"
using json = nlohmann::json;


class cpu65816tester
{
private:

	json testData;

public:

	cpu65816tester();
	int loadTest(std::string filename);
	void executeTest();
	~cpu65816tester();
};

#endif
