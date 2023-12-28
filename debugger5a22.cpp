
/* 5a22 debugger/disasm */

#include "debugger5a22.h"

static const debugInfoRec debugInstrList[] =
{
	{0x10,2,"BPL param0",Immediate8,false,false},
	{0x18,1,"CLC",None,false,false},
	{0x2c,3,"BIT param0",Absolute16,false,false},
	{0x3a,1,"DEC A",None,false,false},
	{0x4B,1,"PHK",None,false,false},
	{0x5b,1,"TCD",None,false,false},
	{0x5c,4,"JML param0",AbsoluteLong,false,false},
	{0x78,1,"SEI",None,false,false},
	{0x85,2,"STA param0",Immediate8,false,false},
	{0x8a,1,"TXA",None,false,false},
	{0x8c,3,"STY param0",Absolute16,false,false},
	{0x8d,3,"STA param0",Absolute16,false,false},
	{0x8e,3,"STX param0",Absolute16,false,false},
	{0x9c,3,"STZ param0",Absolute16,false,false},
	{0x9A,1,"TXS",None,false,false},
	{0xA0,2,"LDY param0",Immediate,true,false},
	{0xA2,2,"LDX param0",Immediate,true,false},
	{0xA9,2,"LDA param0",Immediate,false,true},
	{0xAB,1,"PLB",None,false,false},
	{0xB8,1,"CLV",None,false,false},
	{0xBD,3,"LDA param0,X",Absolute16,false,false},
	{0xC9,2,"CMP param0",Immediate,false,true},
	{0xC2,2,"REP param0",Immediate8,false,false},
	{0xCA,1,"DEX",None,false,false},
	{0xD0,2,"BNE param0",Immediate8,false,false},
	{0xE0,2,"CPX param0",Immediate,true,false},
	{0xe2,2,"SEP param0",Immediate8,false,false},
	{0xe8,1,"INX",None,false,false},
	{0xea,1,"NOP",None,false,false},
	{0xFB,1,"XCE",None,false,false},
};

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

		unsigned short int mem = bytez[1];
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
			std::string processedTemplate = processDisasmTemplate(debugInstrList[idx].disasmTemplate,debugInstrList[idx],curRec.bytez,*theCPU);
			curRec.disasmed = hexPaddedAddress+" "+formatBytes(curRec.bytez,4)+" "+processedTemplate;
			sizeBytes = (int)curRec.bytez.size();
		}
		else
		{
			curRec.disasmed = hexPaddedAddress + " " + formatBytes(curRec.bytez,4) + " " + "UNK";
		}

		retVec.push_back(curRec);
		curAddress += sizeBytes;
		instrsProcessed += 1;
	}


	return retVec;
}
