
#include "debuggerSPC700.h"

dbgSPC700info listOfInstrs[]
{
	{0xcd,"MOV X,#$param0",2,1,true}, // validatedFC
	{0xbd,"MOV SP,X",1,0,true}, // validatedFC
	{0xe8,"MOV A,#$param0",2,1,true}, // validatedFC
	{0xc6,"MOV (X),A",1,0,true}, // validatedFC
	{0x1d,"DEC X",1,0,true}, // validatedFC
	{0xd0,"BNE param0",2,1,true}, // validatedFC
	{0x8f,"MOV $param0,$#param1",3,2,true}, // validatedFC
	{0x78,"CMP $param0,$#param1",3,2,true}, // validatedFC
	{0x2f,"BRA $param0",2,1,true}, // validatedFC
	{0xba,"MOVW YA,$param0",2,1,true}, // validatedFC
	{0xda,"MOVW $param0,YA",2,1,true}, // validatedFC
	{0xc4,"MOV $param0,A",2,1,true}, // validatedFC
	{0xdd,"MOV A,Y",1,0,true}, // validatedFC
	{0x5d,"MOV X,A",1,0,true}, // validatedFC
	{0xeb,"MOV Y,$param0",2,1,true}, // validatedFC
	{0x7e,"CMP Y,$param0",2,1,true}, // validatedFC
	{0xe4,"MOV A,$param0",2,1,true}, // validatedFC
	{0xcb,"MOV $param0,Y",2,1,true}, // validatedFC
	{0xd7,"MOV [$param0]+Y,A",2,1,true}, // validatedFC
	{0xfc,"INC Y",1,0,true}, // validatedFC
	{0xab,"INC $param0",2,1,true}, // validatedFC
	{0x10,"BPL param0",2,1,true}, // validatedFC
	{0x1f,"JMP [param0+X]",3,1,true}, // validatedFC
	{0x20,"CLRP",1,0,true}, // validatedFC
	{0xc5,"MOV $param0,A",3,1,true}, // validatedFC
	{0xaf,"MOV (X)+,A",1,0,true}, // validatedFC
	{0xc8,"CMP X,#param0",2,1,true}, // validatedFC
	{0xd5,"MOV !$param0+X,A",3,1,true}, // validatedFC
	{0x3d,"INC X",1,0,true}, // validatedFC
	{0xf5,"MOV A,!$param0+X",3,1,true}, // validatedFC
	{0xfd,"MOV Y,A",1,0,true}, // validatedFC
	{0x3f,"CALL !$param0",3,1,true}, // validatedFC
	{0xcc,"MOV !$param0,Y",3,1,true}, // validatedFC
	{0x6f,"RET",1,0,true}, // validatedFC
	{0xec,"MOV Y,!$param0",3,1,true}, // validatedFC
	{0xf0,"BEQ param0",2,1,true}, // validatedFC
	{0x6d,"PUSH Y",1,0,true}, // validatedFC
	{0xcf,"MUL YA",1,0,true}, // validatedFC
	{0x60,"CLRC",1,0,true}, // validatedFC
	{0x84,"ADC A,$param0",2,1,true}, // validatedFC
	{0x90,"BCC param0",2,1,true}, // validatedFC
	{0xee,"POP Y",1,0,true}, // validatedFC
	{0x30,"BMI param0",2,1,true}, // validatedFC
	{0xe5,"MOV A,!$param0",3,1,true}, // validatedFC
	{0x7d,"MOV A,X",1,0,true}, // validatedFC
	{0xf4,"MOV A,$param0+X",2,1,true}, // validatedFC


};

debuggerSPC700::debuggerSPC700()
{
	for (auto& instr : listOfInstrs)
	{
		debugInstrList.push_back(instr);
	}
}

std::vector<dbgSPC700info>* debuggerSPC700::getOpcodesList()
{
	return &debugInstrList;
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
	unsigned char opcode = theAPU->internalRead8(address);
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
				unsigned char curByte = theAPU->internalRead8(address + b);
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
			unsigned char secondByte = theAPU->internalRead8(address + 1);
			std::stringstream strr;
			strr << std::hex << std::setw(2) << std::setfill('0') << (int)secondByte;

			dis.replace(dis.find("param0"), sizeof("param0") - 1, strr.str());
			result += dis;
		}
		else if (curInstr.instrBytes == 3)
		{
			std::string dis = curInstr.disasm;
			unsigned char secondByte = theAPU->internalRead8(address + 1);
			unsigned char thirdByte = theAPU->internalRead8(address + 2);

			std::stringstream str2ndByte;
			str2ndByte << std::hex << std::setw(2) << std::setfill('0') << (int)secondByte;
			std::stringstream str3rdByte;
			str3rdByte << std::hex << std::setw(2) << std::setfill('0') << (int)thirdByte;
			std::stringstream strWord;
			strWord << std::hex << std::setw(4) << std::setfill('0') << (int)(secondByte|(thirdByte<<8));

			if (curInstr.numParams == 2)
			{
				dis.replace(dis.find("param0"), sizeof("param0") - 1, str3rdByte.str());
				dis.replace(dis.find("param1"), sizeof("param1") - 1, str2ndByte.str());
			}
			else if (curInstr.numParams == 1)
			{
				dis.replace(dis.find("param0"), sizeof("param0") - 1, strWord.str());
			}
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
