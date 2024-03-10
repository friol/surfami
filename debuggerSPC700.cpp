
#include"logger.h"
#include "debuggerSPC700.h"

extern logger glbTheLogger;


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
	{0x75,"CMP A,!$param0+X",3,1,true}, // validatedFC
	{0xdb,"MOV $param0+X,Y",2,1,true}, // validatedFC
	{0xde,"CBNE $param0+X,$param1",3,2,true}, // validatedFC
	{0x8d,"MOV Y,#$param0",2,1,true}, // validatedFC
	{0x68,"CMP A,#param0",2,1,true}, // validatedFC
	{0x5e,"CMP Y,!$param0",3,1,true}, //validatedFC
	{0xc9,"MOV !$param0,X",3,1,true}, // validatedFC
	{0x5f,"JMP !$param0",3,1,true}, // validatedFC
	{0x80,"SETC",1,0,true}, // validatedFC
	{0x1c,"ASL A",1,0,true}, // validatedFC
	{0xf6,"MOV A,!$param0+Y",3,1,true}, // validatedFC
	{0xd4,"MOV #param0+X,A",2,1,true}, // validatedFC
	{0xd6,"MOV !$param0+Y,A",3,1,true}, // validatedFC
	{0xfe,"DBNZ Y,$param0",2,1,true}, // validatedFC
	{0x48,"EOR A,#$param0",2,1,true}, // validatedFC
	{0x4b,"LSR $param0",2,1,true}, // validatedFC
	{0x28,"AND A,#$param0",2,1,true}, // validatedFC
	{0x02,"SET1 $param0.0",2,1,true}, // validatedFC
	{0x22,"SET1 $param0.1",2,1,true}, // validatedFC
	{0x42,"SET1 $param0.2",2,1,true}, // validatedFC
	{0x62,"SET1 $param0.3",2,1,true}, // validatedFC
	{0x82,"SET1 $param0.4",2,1,true}, // validatedFC
	{0xa2,"SET1 $param0.5",2,1,true}, // validatedFC
	{0xc2,"SET1 $param0.6",2,1,true}, // validatedFC
	{0xe2,"SET1 $param0.7",2,1,true}, // validatedFC
	{0x6e,"DBNZ $param0,$param1",3,2,true}, // validatedFC
	{0xf7,"MOV A,[$param0]+Y",2,1,true}, // validatedFC
	{0x3a,"INCW $param0",2,1,true}, // validatedFC
	{0x2d,"PUSH A",1,0,true}, // validatedFC
	{0xae,"POP A",1,0,true}, // validatedFC
	{0xdc,"DEC Y",1,0,true}, // validatedFC
	{0xbc,"INC A",1,0,true}, // validatedFC
	{0x9c,"DEC A",1,0,true}, // validatedFC
	{0xac,"INC !$param0",3,1,true}, // validatedFC
	{0x13,"BBC $param0.0,$param1",3,2,true}, // fck
	{0x33,"BBC $param0.1,$param1",3,2,true}, // fck
	{0x53,"BBC $param0.2,$param1",3,2,true}, // fck
	{0x73,"BBC $param0.3,$param1",3,2,true}, // fck
	{0x93,"BBC $param0.4,$param1",3,2,true}, // fck
	{0xb3,"BBC $param0.5,$param1",3,2,true}, // fck
	{0xd3,"BBC $param0.6,$param1",3,2,true}, // fck
	{0xf3,"BBC $param0.7,$param1",3,2,true}, // fck
	{0xad,"CMP Y,#$param0",2,1,true}, // validatedFC
	{0x7a,"ADDW YA,#$param0",2,1,true}, // validatedFC
	{0xe7,"MOV A,[$param0+X]",2,1,true}, // validatedFC
	{0xb0,"BCS param0",2,1,true}, // validatedFC
	{0x09,"OR $param0,$param1",3,2,true}, // validatedFC
	{0xa8,"SBC param0",2,1,true}, // validatedFC
	{0x4d,"PUSH X",1,0,true}, // validatedFC
	{0x9e,"DIV YA,X",1,0,true}, // validatedFC
	{0x7c,"ROR A",1,0,true}, // validatedFC
	{0xce,"POP X",1,0,true}, // validatedFC
	{0x9a,"SUBW YA,$param0",2,1,true}, // validatedFC
	{0x9f,"XCN A",1,0,true}, // validatedFC
	{0x5c,"LSR A",1,0,true}, // validatedFC
	{0x08,"OR A,#$param0",2,1,true}, // validatedFC
	{0x24,"AND A,$param0",2,1,true}, // validatedFC
	{0x12,"CLR1 $param0.0",2,1,true}, // validatedFC
	{0x32,"CLR1 $param0.1",2,1,true}, // validatedFC
	{0x52,"CLR1 $param0.2",2,1,true}, // validatedFC
	{0x72,"CLR1 $param0.3",2,1,true}, // validatedFC
	{0x92,"CLR1 $param0.4",2,1,true}, // validatedFC
	{0xb2,"CLR1 $param0.5",2,1,true}, // validatedFC
	{0xd2,"CLR1 $param0.6",2,1,true}, // validatedFC
	{0xf2,"CLR1 $param0.7",2,1,true}, // validatedFC
	{0x65,"CMP A,!$param0",3,1,true}, // validatedFC
	{0x8c,"DEC !$param0",3,1,true}, // validatedFC
	{0xc0,"DI",1,0,true}, // validatedFC
	{0x8e,"POP PSW",1,0,true}, // validatedFC
	{0x04,"OR A,$param0",2,1,true}, // validatedFC
	{0x64,"CMP A,$param0",2,1,true}, // validatedFC
	{0xf8,"MOV X,$param0",2,1,true}, // validatedFC
	{0x3e,"CMP X,$param0",2,1,true}, // validatedFC
	{0x29,"AND param0,param1",3,2,true}, // validatedFC
	{0xfa,"MOV param0,param1",3,2,true}, // validatedFC
	{0xd8,"MOV $param0,X",2,1,true}, // validatedFC
	{0x88,"ADC A,#param0",2,1,true}, // validatedFC
	{0x9b,"DEC $param0+X",2,1,true}, // validatedFC
	{0xbb,"INC $param0+X",2,1,true}, // validatedFC
	{0x85,"ADC A,!$param0",3,1,true}, // validatedFC
	{0x0b,"ASL $param0",2,1,true}, // validatedFC
	{0xb6,"SBC A,!$param0+Y",3,1,true}, // validatedFC
	{0x96,"ADC A,!$param0+Y",3,1,true}, // validatedFC
	{0x03,"BBS $param0.0,$param1",3,2,true }, // validatedFC
	{0x23,"BBS $param0.1,$param1",3,2,true }, // validatedFC
	{0x43,"BBS $param0.2,$param1",3,2,true }, // validatedFC
	{0x63,"BBS $param0.3,$param1",3,2,true }, // validatedFC
	{0x83,"BBS $param0.4,$param1",3,2,true }, // validatedFC
	{0xa3,"BBS $param0.5,$param1",3,2,true }, // validatedFC
	{0xc3,"BBS $param0.6,$param1",3,2,true }, // validatedFC
	{0xe3,"BBS $param0.7,$param1",3,2,true }, // validatedFC
	{0x40,"SETP",1,0,true}, // validatedFC
	{0x74,"CMP A,$param0+x",2,1,true}, // validatedFC
	{0xfb,"MOV Y,$param0+x",2,1,true}, // validatedFC
	{0x95,"ADC A,!$param0+X",3,1,true}, // validatedFC
	{0xb5,"SBC A,!$param0+X",3,1,true}, // validatedFC
	{0xc7,"MOV $param0+X,A",2,1,true}, // validatedFC
	{0x98,"ADC $param0,#param1",3,2,true}, // validatedFC
	{0x2e,"CBNE $param0,#param1",3,2,true}, // validatedFC
	{0x97,"ADC A,[$param0]+Y",2,1,true}, // validatedFC
	{0xa4,"SBC A,$param0",3,1,false}, // validatedFC


};

debuggerSPC700::debuggerSPC700()
{
	for (auto& instr : listOfInstrs)
	{
		debugInstrList.push_back(instr);
	}

	glbTheLogger.logMsg("Opcodes added to debugger: " + std::to_string(debugInstrList.size()));
}

std::vector<dbgSPC700info>* debuggerSPC700::getOpcodesList()
{
	return &debugInstrList;
}

int debuggerSPC700::findOpcodeIndex(unsigned char opcode)
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

std::string debuggerSPC700::processDisasmTemplate(unsigned short int address, apu* theAPU, unsigned short int& readBytes)
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
		std::stringstream strr2;
		strr2 << std::hex << std::setw(2) << std::setfill('0') << (int)opcode;
		result += strr2.str() + " ";
		readBytes += 1;

		result += "         UNK";
		return result;
	}
	else
	{
		dbgSPC700info curInstr = listOfInstrs[tmplIdx];

		for (unsigned int b = 0;b < 4;b++)
		{
			if (b < curInstr.instrBytes)
			{
				unsigned char curByte = theAPU->internalRead8(address + b);
				std::stringstream strr2;
				strr2 << std::hex << std::setw(2) << std::setfill('0') << (int)curByte;
				result += strr2.str() + " ";
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
			std::stringstream strr2;
			strr2 << std::hex << std::setw(2) << std::setfill('0') << (int)secondByte;

			dis.replace(dis.find("param0"), sizeof("param0") - 1, strr2.str());
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
	for (unsigned int inum = 0;inum < numInstrs;inum++)
	{
		unsigned short int readBytes = 0;
		std::string curDisasm = processDisasmTemplate(curAddress, theAPU,readBytes);
		retVec.push_back(curDisasm);
		curAddress += readBytes;
	}

	return retVec;
}

debuggerSPC700::~debuggerSPC700()
{
}
