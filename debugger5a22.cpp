
/* 5a22 debugger/disasm */

#include "debugger5a22.h"

static const debugInfoRec debugInstrData[] =
{
	{0x03,2,"ORA param0",Immediate8,false,false,true}, // validated
	{0x06,2,"ASL param0",Immediate8,false,false,true}, // validated
	{0x08,1,"PHP",None,false,false,true}, // validated
	{0x05,2,"ORA param0",Immediate8,false,false,true}, // validated
	{0x09,2,"ORA param0",Immediate,false,true,true}, // validated
	{0x0A,1,"ASL A",None,false,false,true}, // validated
	{0x0B,1,"PHD",None,false,false,true}, // validated
	{0x0C,3,"TSB param0",Absolute16,false,false,true}, // validated
	{0x0D,3,"ORA param0",Absolute16,false,false,true}, // validated
	{0x0e,3,"ASL param0",Absolute16,false,false,true}, // validated
	{0x0f,4,"ORA param0",AbsoluteLong,false,false,true}, // validated
	{0x10,2,"BPL param0",Immediate8,false,false,true}, // validated
	{0x14,2,"TRB param0",Immediate8,false,false,true},
	{0x17,2,"ORA [param0],Y",Immediate8,false,false,true}, // validated
	{0x18,1,"CLC",None,false,false,true}, // validated
	{0x1A,1,"INC A",None,false,false,true}, // validated
	{0x1B,1,"TCS",None,false,false,true}, // validated
	{0x1d,3,"ORA param0",Absolute16,false,false,true}, // validated
	{0x20,3,"JSR param0",Absolute16,false,false,true}, // validated
	{0x21,2,"AND param0,X",Immediate8,false,false,true}, // validated
	{0x22,4,"JSL param0",AbsoluteLong,false,false,true}, // validated
	{0x24,2,"BIT param0",Immediate,false,false,true}, // validated
	{0x25,2,"AND param0",Immediate8,false,false,true}, // validated
	{0x26,2,"ROL param0",Immediate8,false,false,true}, // validated
	{0x27,2,"AND [param0]",Immediate8,false,false,true}, // validated
	{0x28,1,"PLP",None,false,false,true}, // validated
	{0x29,2,"AND param0",Immediate,false,true,true}, // validated
	{0x2a,1,"ROL",None,false,false,true}, // validated
	{0x2b,1,"PLD",None,false,false,true}, // validated
	{0x2c,3,"BIT param0",Absolute16,false,false,true}, // validated
	{0x2d,3,"AND param0",Absolute16,false,false,true}, // validated
	{0x2f,4,"AND param0",AbsoluteLong,false,false,true}, // validated
	{0x30,2,"BMI param0",Immediate8,false,false,true}, // validated
	{0x32,2,"AND (param0)",Immediate8,false,false,true}, // validated
	{0x38,1,"SEC",None,false,false,true}, // validated
	{0x3a,1,"DEC A",None,false,false,true}, // validated
	{0x44,3,"MVP param0",Absolute16,false,false,true}, // fails
	{0x46,2,"LSR param0",Immediate8,false,false,true}, // validated
	{0x48,1,"PHA",None,false,false,true}, // validated
	{0x49,2,"EOR param0",Immediate,false,true,true}, // validated
	{0x4a,1,"LSR A",None,false,false,true}, // validated
	{0x4c,3,"JMP param0",Absolute16,false,false,true}, // validated
	{0x4d,3,"EOR param0",Absolute16,false,false,true}, // validated
	{0x4e,3,"LSR param0",Absolute16,false,false,true},
	{0x4B,1,"PHK",None,false,false,true}, // validated
	{0x50,2,"BVC param0",Immediate8,false,false,true},
	{0x54,3,"MVN param0",Absolute16,false,false,true},
	{0x55,2,"EOR param0,X",Absolute16,false,false,true}, // validated
	{0x58,1,"CLI",None,false,false,true}, // validated
	{0x5a,1,"PHY",None,false,false,true}, // validated
	{0x5b,1,"TCD",None,false,false,true}, // validated
	{0x5c,4,"JML param0",AbsoluteLong,false,false,true}, // validated
	{0x5d,3,"EOR param0,X",Absolute16,false,false,true}, // validated
	{0x62,3,"PER param0,X",Absolute16,false,false,true}, // validated
	{0x60,1,"RTS",None,false,false,true}, // validated
	{0x64,2,"STZ param0",Immediate8,false,false,true}, // validated
	{0x65,2,"ADC param0",Immediate8,false,false,true},
	{0x68,1,"PLA",None,false,false,true}, // validated
	{0x69,2,"ADC param0",Immediate,false,true,true},
	{0x6a,1,"ROR A",None,false,false,true}, // validated
	{0x6b,1,"RTL",None,false,false,true}, // validated
	{0x6d,3,"ADC param0",Absolute16,false,false,true},
	{0x6e,3,"ROR param0",Absolute16,false,false,true}, // validated
	{0x70,2,"BVS param0",Immediate8,false,false,true}, // validated
	{0x73,2,"ADC (param0,S),Y",Immediate8,false,false,true},
	{0x74,2,"STZ param0,X",Immediate8,false,false,true}, // validated
	{0x75,2,"ADC param0,X",Immediate8,false,false,true},
	{0x77,2,"ADC [param0],Y",Immediate8,false,false,true},
	{0x78,1,"SEI",None,false,false,true}, // validated
	{0x7a,1,"PLY",None,false,false,true}, // validated
	{0x7c,3,"JMP (param0,X)",Absolute16,false,false,true}, // validated
	{0x80,2,"BRA param0",Immediate8,false,false,true}, // validated
	{0x82,3,"BRL param0",Absolute16,false,false,true}, // validated
	{0x84,2,"STY param0",Immediate8,false,false,true}, // validated
	{0x85,2,"STA param0",Immediate8,false,false,true}, // validated
	{0x86,2,"STX param0",Immediate8,false,false,true}, // validated
	{0x88,1,"DEY",None,false,false,true}, // validated
	{0x89,2,"BIT param0",Immediate,false,true,true}, // validated
	{0x8a,1,"TXA",None,false,false,true}, // validated
	{0x8b,1,"PHB",None,false,false,true}, // validated
	{0x8c,3,"STY param0",Absolute16,false,false,true}, // validated
	{0x8d,3,"STA param0",Absolute16,false,false,true}, // validated
	{0x8e,3,"STX param0",Absolute16,false,false,true}, // validated
	{0x8f,4,"STA param0",AbsoluteLong,false,false,true}, // validated
	{0x90,2,"BCC param0",Immediate8,false,false,true}, // validated
	{0x91,2,"STA (param0),Y",Immediate8,false,false,true}, // validated
	{0x92,2,"STA (param0)",Immediate8,false,false,true}, // validated
	{0x95,2,"STA _dp_ param0,X",Immediate8,false,false,true}, // validated
	{0x97,2,"STA [param0],Y",Immediate8,false,false,true}, // validated
	{0x98,1,"TYA",None,false,false,true}, // validated
	{0x99,3,"STA param0,Y",Absolute16,false,false,true}, // validated
	{0x9c,3,"STZ param0",Absolute16,false,false,true}, // validated
	{0x9A,1,"TXS",None,false,false,true}, // validated
	{0x9b,1,"TXY",None,false,false,true}, // validated
	{0x9d,3,"STA param0,X",Absolute16,false,false,true}, // validated
	{0x9e,3,"STZ param0,X",Absolute16,false,false,true}, // validated
	{0x9f,4,"STA param0,X",AbsoluteLong,false,false,true}, // validated
	{0xA0,2,"LDY param0",Immediate,true,false,true}, // validated
	{0xA2,2,"LDX param0",Immediate,true,false,true}, // validated
	{0xA3,2,"LDA param0,S",Immediate8,false,false,true}, // validated
	{0xA4,2,"LDY param0",Immediate8,false,false,true}, // validated
	{0xA5,2,"LDA param0",Immediate8,false,false,true}, // validated
	{0xA6,2,"LDX param0",Immediate8,false,false,true}, // validated
	{0xA7,2,"LDA [param0]",Immediate8,false,false,true}, // validated
	{0xA8,1,"TAY",None,false,false,true}, // validated
	{0xA9,2,"LDA param0",Immediate,false,true,true}, // validated
	{0xAA,1,"TAX",None,false,false,true}, // validated
	{0xAB,1,"PLB",None,false,false,true}, // validated
	{0xAC,3,"LDY param0",Absolute16,false,false,true}, // validated
	{0xad,3,"LDA param0",Absolute16,false,false,true}, // validated
	{0xae,3,"LDX param0",Absolute16,false,false,true}, // validated
	{0xaf,4,"LDA param0",AbsoluteLong,false,false,true}, // validated
	{0xB0,2,"BCS param0",Immediate8,false,false,true}, // validated
	{0xb1,2,"LDA (param0),Y",Immediate8,false,false,true}, // validated
	{0xb2,2,"LDA (param0)",Immediate8,false,false,true}, // validated
	{0xb3,2,"LDA (param0,S),Y",Immediate8,false,false,true}, // validated
	{0xB5,2,"LDA param0,X",Immediate8,false,false,true}, // validated
	{0xB7,2,"LDA [param0],Y",Immediate8,false,false,true}, // validated
	{0xB9,3,"LDA param0,Y",Absolute16,false,false,true}, // validated
	{0xB8,1,"CLV",None,false,false,true}, // validated
	{0xbb,1,"TYX",None,false,false,true}, // validated
	{0xBC,3,"LDY param0,X",Absolute16,false,false,true}, // validated
	{0xBE,3,"LDX param0,Y",Absolute16,false,false,true}, // validated
	{0xBf,4,"LDA param0,X",AbsoluteLong,false,false,true}, // validated
	{0xBD,3,"LDA param0,X",Absolute16,false,false,true}, // validated
	{0xC0,2,"CPY param0",Immediate,true,false,true}, // validated
	{0xC5,2,"CMP param0",Immediate8,false,false,true}, // validated
	{0xC6,2,"DEC param0",Immediate8,false,false,true}, // validated
	{0xC8,1,"INY",None,false,false,true}, // validated
	{0xC9,2,"CMP param0",Immediate,false,true,true}, // validated
	{0xC2,2,"REP param0",Immediate8,false,false,true}, // validated
	{0xC4,2,"CPY param0",Immediate8,false,false,true}, // validated
	{0xCD,3,"CMP param0",Absolute16,false,false,true}, // validated
	{0xCA,1,"DEX",None,false,false,true}, // validated
	{0xCE,3,"DEC param0",Absolute16,false,false,true}, // validated
	{0xD0,2,"BNE param0",Immediate8,false,false,true}, // validated
	{0xD6,2,"DEC param0,X",Immediate8,false,false,true}, // validated
	{0xd8,1,"CLD",None,false,false,true}, // validated
	{0xD9,3,"CMP param0,Y",Absolute16,false,false,true}, // validated
	{0xda,1,"PHX",None,false,false ,true}, // validated
	{0xDD,3,"CMP param0,X",Absolute16,false,false,true}, // validated
	{0xDE,3,"DEC param0,X",Absolute16,false,false,true}, // validated
	{0xdf,4,"CMP param0,X",AbsoluteLong,false,false ,true}, // validated
	{0xE0,2,"CPX param0",Immediate,true,false,true}, // validated
	{0xe2,2,"SEP param0",Immediate8,false,false,true}, // validated
	{0xe4,2,"CPX param0",Immediate8,false,false,true}, // validated
	{0xe5,2,"SBC param0",Immediate8,false,false,true},
	{0xe6,2,"INC param0",Immediate8,false,false,true}, // validated
	{0xe8,1,"INX",None,false,false,true}, // validated
	{0xe9,2,"SBC param0",Immediate,false,true,true},
	{0xeb,1,"XBA",None,false,false,true}, // validated
	{0xEC,3,"CPX param0",Absolute16,false,false,true}, // validated
	{0xED,3,"SBC param0",Absolute16,false,false,true},
	{0xEE,3,"INC param0",Absolute16,false,false ,true}, // validated
	{0xea,1,"NOP",None,false,false,true}, // validated
	{0xf0,2,"BEQ param0",Immediate8,false,false,true}, // validated
	{0xF4,3,"PEA param0",Absolute16,false,false,true}, // validated
	{0xF9,3,"SBC param0,Y",Absolute16,false,false,true},
	{0xFA,1,"PLX",None,false,false,true}, // validated
	{0xFB,1,"XCE",None,false,false,true}, // validated
	{0xfc,3,"JSR (param0,X)",Absolute16,false,false ,true}, // fuck
	{0xff,4,"SBC param0,X",AbsoluteLong,false,false ,true},
};

debugger5a22::debugger5a22()
{
	for (auto& instr : debugInstrData)
	{
		debugInstrList.push_back(instr);
	}
}

std::vector<debugInfoRec>* debugger5a22::getOpcodesList()
{
	return &debugInstrList;
}

int debugger5a22::findOpcode(unsigned char opcode)
{
	int idx = 0;
	for (auto& instr : debugInstrList)
	{
		if (instr.opcode == opcode)
		{
			return idx;
		}
		idx += 1;
	}

	return -1;
}

std::string debugger5a22::formatBytes(std::vector<unsigned char>& bytez,int maxbytez)
{
	std::string retString;

	int theb = 0;
	for (auto& b : bytez)
	{
		std::stringstream strr;
		strr << std::hex << std::setw(2) << std::setfill('0') << (int)b;
		retString+=strr.str()+" ";
		theb += 1;
	}

	for (int rem = 0;rem < (maxbytez - theb);rem++)
	{
		retString += "   ";
	}

	return retString;
}

std::string debugger5a22::processDisasmTemplate(std::string disasmTmpl,const debugInfoRec& debugInfo,std::vector<unsigned char>& bytez, cpu5a22& theCPU)
{
	std::string res = disasmTmpl;

	if (debugInfo.addressingMode == None)
	{
		return res;
	}
	else if (debugInfo.addressingMode == Immediate8)
	{
		std::stringstream strr;
		std::string hexPaddedConst;

		strr << std::hex << std::setw(2) << std::setfill('0') << (int)bytez[1];
		hexPaddedConst = strr.str();

		hexPaddedConst = "#$$" + hexPaddedConst;
		res = std::regex_replace(res, std::regex("param0"), hexPaddedConst);
	}
	else if (debugInfo.addressingMode == Immediate)
	{
		std::stringstream strr;
		std::string hexPaddedConst;

		if (debugInfo.additionalByteIfXFlag)
		{
			if (theCPU.getIndexSize() == 1)
			{
				strr << std::hex << std::setw(2) << std::setfill('0') << (int)bytez[1];
				hexPaddedConst = strr.str();
			}
			else
			{
				unsigned short int mem = bytez[1];
				mem |= ((int)bytez[2]) << 8;
				strr << std::hex << std::setw(4) << std::setfill('0') << mem;
				hexPaddedConst = strr.str();
			}
		}
		else if (debugInfo.additionalByteIfAccuMemSize)
		{
			if (theCPU.getAccuMemSize() == 1)
			{
				strr << std::hex << std::setw(2) << std::setfill('0') << (int)bytez[1];
				hexPaddedConst = strr.str();
			}
			else
			{
				unsigned short int mem = bytez[1];
				mem |= ((int)bytez[2]) << 8;
				strr << std::hex << std::setw(4) << std::setfill('0') << mem;
				hexPaddedConst = strr.str();
			}
		}

		hexPaddedConst = "#$$" + hexPaddedConst;
		res = std::regex_replace(res, std::regex("param0"), hexPaddedConst);
	}
	else if (debugInfo.addressingMode == Absolute16)
	{
		std::stringstream strr;
		std::string hexPaddedConst;

		unsigned short int mem = bytez[1];
		mem |= ((int)bytez[2]) << 8;
		strr << std::hex << std::setw(4) << std::setfill('0') << mem;
		hexPaddedConst = strr.str();

		hexPaddedConst = "$$" + hexPaddedConst;
		res = std::regex_replace(res, std::regex("param0"), hexPaddedConst);
	}
	else if (debugInfo.addressingMode == AbsoluteLong)
	{
		std::stringstream strr;
		std::string hexPaddedConst;

		unsigned int mem = bytez[1];
		mem |= ((int)bytez[2]) << 8;
		mem |= ((int)bytez[3]) << 16;
		strr << std::hex << std::setw(6) << std::setfill('0') << mem;
		hexPaddedConst = strr.str();

		hexPaddedConst = "$$" + hexPaddedConst;
		res = std::regex_replace(res, std::regex("param0"), hexPaddedConst);
	}

	return res;
}

std::vector<disasmInfoRec> debugger5a22::debugCode(unsigned int pc, unsigned int numInstrs, cpu5a22* theCPU, mmu* theMMU)
{
	unsigned int instrsProcessed = 0;
	std::vector<disasmInfoRec> retVec;
	int curAddress = pc;

	while (instrsProcessed < numInstrs)
	{
		disasmInfoRec curRec;
		curRec.address = curAddress;

		std::stringstream strr;
		strr << std::hex << std::setw(6) << std::setfill('0') << curAddress;
		std::string hexPaddedAddress(strr.str());

		unsigned char opcode = theMMU->read8(curAddress);
		curRec.bytez.push_back(opcode);
		int idx = findOpcode(opcode);

		if (idx == -1)
		{
			curRec.disasmed= hexPaddedAddress + " " + formatBytes(curRec.bytez, 4) + " " + "UNK";
			retVec.push_back(curRec);
			curAddress += 1;
			instrsProcessed += 1;
		}
		else
		{
			// additional bytes
			if (debugInstrList[idx].sizeBytes > 1)
			{
				for (int b = 0;b < debugInstrList[idx].sizeBytes - 1;b++)
				{
					unsigned char bytez = theMMU->read8(curAddress + b + 1);
					curRec.bytez.push_back(bytez);
				}
			}

			if (debugInstrList[idx].additionalByteIfXFlag)
			{
				// add another byte if X flag is zero
				if (theCPU->getIndexSize() == false)
				{
					unsigned char bytez = theMMU->read8(curAddress + debugInstrList[idx].sizeBytes);
					curRec.bytez.push_back(bytez);
				}
			}

			if (debugInstrList[idx].additionalByteIfAccuMemSize)
			{
				// add another byte if M flag is zero
				if (theCPU->getAccuMemSize() == false)
				{
					unsigned char bytez = theMMU->read8(curAddress + debugInstrList[idx].sizeBytes);
					curRec.bytez.push_back(bytez);
				}
			}

			int sizeBytes = 1;
			if (idx != -1)
			{
				std::string processedTemplate = processDisasmTemplate(debugInstrList[idx].disasmTemplate, debugInstrList[idx], curRec.bytez, *theCPU);
				curRec.disasmed = hexPaddedAddress + " " + formatBytes(curRec.bytez, 4) + " " + processedTemplate;
				sizeBytes = (int)curRec.bytez.size();
			}
			else
			{
				curRec.disasmed = hexPaddedAddress + " " + formatBytes(curRec.bytez, 4) + " " + "UNK";
			}

			retVec.push_back(curRec);
			curAddress += sizeBytes;
			instrsProcessed += 1;
		}
	}

	return retVec;
}
