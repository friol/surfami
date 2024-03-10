#ifndef DEBUGGERSPC700_H
#define DEBUGGERSPC700_H

#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include "apu.h"

struct dbgSPC700info
{
	unsigned char opcode;
	std::string disasm;
	unsigned int instrBytes;
	unsigned int numParams;
	bool isValidatedTomHarte;
};

class debuggerSPC700
{
private:

	std::vector<dbgSPC700info> debugInstrList;

	int findOpcodeIndex(unsigned char opcode);
	std::string processDisasmTemplate(unsigned short int address, apu* theAPU,unsigned short int& readBytes);

public:

	debuggerSPC700();
	std::vector<std::string> disasmOpcodes(unsigned short int pc, unsigned int numInstrs, apu* theAPU);
	std::vector<dbgSPC700info>* getOpcodesList();
	~debuggerSPC700();
};

#endif
