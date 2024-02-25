
#include "debuggerSPC700.h"

dbgSPC700info listOfInstrs[]
{
	{0xcd,"MOV X,#$param0",2,1},
	{0xbd,"MOV SP,X",1,0},
	{0xe8,"MOV A,#$param0",2,1},
	{0xc6,"MOV (X),A",1,0},
	{0x1d,"DEC X",1,0},
	{0xd0,"BNE param0",2,1},
};

debuggerSPC700::debuggerSPC700()
{
}

unsigned int debuggerSPC700::findOpcodeIndex(unsigned char opcode)
{
	unsigned int idx = 0;
	for (auto& instr : listOfInstrs)
	{
		if (instr.opcode == opcode)
		{
			return idx;
		}

		idx += 1;
	}

	return -1;
}

std::string debuggerSPC700::processDisasmTemplate(unsigned short int address, apu* theAPU, unsigned int& readBytes)
{
	std::string result;

	std::stringstream strr;
	strr << std::hex << std::setw(4) << std::setfill('0') << address;
	result += strr.str() + " ";

	// find template idx
	unsigned char opcode = theAPU->read8(address);
	unsigned int tmplIdx = findOpcodeIndex(opcode);

	if (tmplIdx == -1)
	{
		std::stringstream strr;
		strr << std::hex << std::setw(2) << std::setfill('0') << (int)opcode;
		result += strr.str() + " ";
		readBytes += 1;

		result += "         UNK";
		return result;
	}
	else
	{
		dbgSPC700info curInstr = listOfInstrs[tmplIdx];

		for (int b = 0;b < 4;b++)
		{
			if (b < curInstr.instrBytes)
			{
				unsigned char curByte = theAPU->read8(address + b);
				std::stringstream strr;
				strr << std::hex << std::setw(2) << std::setfill('0') << (int)curByte;
				result += strr.str() + " ";
				readBytes += 1;
			}
			else
			{
				result += "   ";
			}
		}

		// disasm it

		if (curInstr.instrBytes == 1)
		{
			result += curInstr.disasm;
		}
		else if (curInstr.instrBytes == 2)
		{
			std::string dis = curInstr.disasm;
			unsigned char secondByte = theAPU->read8(address + 1);
			std::stringstream strr;
			strr << std::hex << std::setw(2) << std::setfill('0') << (int)secondByte;

			dis.replace(dis.find("param0"), sizeof("param0") - 1, strr.str());
			result += dis;
		}

	}

	return result;
}

std::vector<std::string> debuggerSPC700::disasmOpcodes(unsigned short int pc, unsigned int numInstrs, apu* theAPU)
{
	std::vector<std::string> retVec;

	unsigned short int curAddress = pc;
	for (int inum = 0;inum < numInstrs;inum++)
	{
		unsigned int readBytes = 0;
		std::string curDisasm = processDisasmTemplate(curAddress, theAPU,readBytes);
		retVec.push_back(curDisasm);
		curAddress += readBytes;
	}

	return retVec;
}

debuggerSPC700::~debuggerSPC700()
{
}
