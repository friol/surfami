#ifndef DEBUGGER_H
#define DEBUGGER_H

#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <regex>

#include "cpu5a22.h"
#include "mmu.h"

enum AddressingMode
{
	None,
	Immediate, 
	Immediate8,
	Absolute16,
	AbsoluteLong,
	Relative,
	RelativeLong, 
	Direct 
};

class debugInfoRec
{
public:

	unsigned char opcode;
	int sizeBytes;
	std::string disasmTemplate;
	AddressingMode addressingMode;
	bool additionalByteIfXFlag;
	bool additionalByteIfAccuMemSize;
};

class disasmInfoRec
{
public:

	unsigned int address;
	std::vector<unsigned char> bytez;
	std::string disasmed;
};

class debugger5a22
{
private:

	int findOpcode(unsigned char opcode);
	std::string formatBytes(std::vector<unsigned char>& bytez, int maxbytez);
	std::string processDisasmTemplate(std::string disasmTmpl, const debugInfoRec& debugInfo, std::vector<unsigned char>& bytez, cpu5a22& theCPU);

public:

	std::vector<disasmInfoRec> debugCode(unsigned int pc, unsigned int numInstrs, cpu5a22* theCPU,mmu* theMMU);

};

#endif
