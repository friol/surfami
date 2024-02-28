
#include "apu.h"
#include <cstdlib> 
#include <fstream>
#include <vector>
#include <iomanip>
#include <sstream>

extern logger glbTheLogger;

apu::apu()
{
	// allocate memory
	spc700ram = new unsigned char[0x10000];

	for (int i = 0;i < 0x10000;i++)
	{
		spc700ram[i] = 0;
	}

	// load ipl rom
	std::vector<unsigned char> vec;
	std::string bootRomName = "roms/ipl.rom";

	std::ifstream file(bootRomName, std::ios::binary);
	if (!file)
	{
		return;
	}

	file.unsetf(std::ios::skipws);

	std::streampos fileSize;
	file.seekg(0, std::ios::end);
	fileSize = file.tellg();

	if (fileSize != 64)
	{
		return;
	}

	file.seekg(0, std::ios::beg);

	vec.reserve(fileSize);
	vec.insert(vec.begin(), std::istream_iterator<unsigned char>(file), std::istream_iterator<unsigned char>());

	for (int i = 0;i < 64;i++)
	{
		bootRom[i] = vec[i];
	}

	bootRomLoaded = true;

	// the ipl rom goes to 0xffc0-0xffff in the SPC ram
	for (int i = 0;i < 64;i++)
	{
		spc700ram[0xffc0 + i] = bootRom[i];
	}
}

bool apu::isBootRomLoaded()
{
	return bootRomLoaded;
}

void apu::reset()
{
	glbTheLogger.logMsg("APU reset");

	//regPC = 0xfffe;
	regA = 0;
	regX = 0;
	regY = 0;
	regSP = 0;

	unsigned char pcLow = spc700ram[0xfffe];
	unsigned char pcHi = spc700ram[0xffff];
	regPC = pcLow | (pcHi << 8);

	std::stringstream strstrdgb;
	strstrdgb << std::hex << std::setw(4) << std::setfill('0') << (int)regPC;
	std::string sInitialPC = strstrdgb.str();

	glbTheLogger.logMsg("SPC700 initial PC: 0x"+ sInitialPC);
}

std::vector<std::string> apu::getRegisters()
{
	std::vector<std::string> result;

	std::string row0;

	std::stringstream strstrdgb;
	strstrdgb << std::hex << std::setw(2) << std::setfill('0') << (int)regA;
	row0+="A:"+strstrdgb.str();

	std::stringstream strstrdgb2;
	strstrdgb2 << std::hex << std::setw(2) << std::setfill('0') << (int)regX;
	row0 += " X:" + strstrdgb2.str();

	std::stringstream strstrdgb3;
	strstrdgb3 << std::hex << std::setw(2) << std::setfill('0') << (int)regY;
	row0 += " Y:" + strstrdgb3.str();

	std::stringstream strstrdgb4;
	strstrdgb4 << std::hex << std::setw(4) << std::setfill('0') << (int)regPC;
	row0 += " PC:" + strstrdgb4.str();

	std::stringstream strstrdgb5;
	strstrdgb5 << std::hex << std::setw(4) << std::setfill('0') << (int)regSP;
	row0 += " SP:" + strstrdgb5.str();

	result.push_back(row0);
	result.push_back("");
	result.push_back("NVPBHIZC");

	std::string flagsBitstring;

	if (flagN) flagsBitstring += "1";
	else flagsBitstring += "0";
	if (flagV) flagsBitstring += "1";
	else flagsBitstring += "0";
	if (flagP) flagsBitstring += "1";
	else flagsBitstring += "0";
	if (flagB) flagsBitstring += "1";
	else flagsBitstring += "0";
	if (flagH) flagsBitstring += "1";
	else flagsBitstring += "0";
	flagsBitstring += "0";
	if (flagZ) flagsBitstring += "1";
	else flagsBitstring += "0";
	if (flagC) flagsBitstring += "1";
	else flagsBitstring += "0";

	result.push_back(flagsBitstring);

	return result;
}

unsigned char apu::read8(unsigned int address)
{
	if (address == 0x2140)
	{
		// 2140h - APUI00 - Main CPU to Sound CPU Communication Port 0 (R / W)
		unsigned char value = apuChan[address-0x2140];
		apuChan[address - 0x2140] = 0xaa;
		return value;
	}
	else if (address == 0x2141)
	{
		// 2141h - APUI01 - Main CPU to Sound CPU Communication Port 1 (R/W)
		unsigned char value = apuChan[address - 0x2140];
		apuChan[address - 0x2140] = 0xbb;
		return value;
	}
	else if (address == 0x2142)
	{
		unsigned char value = apuChan[address - 0x2140];
		apuChan[address - 0x2140] = 0x00;
		return value;
	}
	else if (address == 0x2143)
	{
		unsigned char value = apuChan[address - 0x2140];
		apuChan[address - 0x2140] = 0x00;
		return value;
	}
	else
	{
		return spc700ram[address];
	}

	return 0;
}

void apu::write8(unsigned int address, unsigned char val)
{
	if ((address == 0x2140) || (address == 0x2141) || (address == 0x2142) || (address == 0x2143))
	{
		apuChan[address - 0x2140] = val;
	}
	else
	{
		spc700ram[address] = val;
	}
}

void apu::doFlagsNZ(unsigned char val)
{
	flagN = (val >> 7);
	flagZ = (val == 0);
}

void apu::doMoveToX(pAddrModeFun fn)
{
	unsigned short int addr=(this->*fn)();
	unsigned char val = read8(addr);
	regX = val;
	doFlagsNZ(regX);
}

void apu::doMoveToA(pAddrModeFun fn)
{
	unsigned short int addr = (this->*fn)();
	unsigned char val = read8(addr);
	regA = val;
	doFlagsNZ(regA);
}

void apu::doDecX()
{
	regX -= 1;
	doFlagsNZ(regX);
}

void apu::doMoveWithRead(pAddrModeFun fn, unsigned char val)
{
	unsigned short int addr = (this->*fn)();
	read8(addr);
	write8(addr,val);
}

int apu::doBNE()
{
	signed char off = read8(regPC+1);
	if (!flagZ) 
	{
		regPC += off+2;
		return 4;
	}

	regPC += 2;
	return 2;
}

// addressing modez

unsigned short int apu::addrModePC()
{
	return regPC + 1;
}

unsigned short int apu::addrModeX()
{
	unsigned short int adr = regX | flagP;
	return adr;
}

int apu::stepOne()
{
	unsigned char nextOpcode = read8(regPC);
	int cycles = 0;

	switch (nextOpcode)
	{
		case 0xcd:
		{
			// MOV X,imm
			doMoveToX(&apu::addrModePC);
			regPC += 2;
			cycles = 2;
			break;
		}
		case 0xbd:
		{
			// MOV SP,X
			regSP = 0x100 | regX; 
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0xe8:
		{
			// MOV A,imm
			doMoveToA(&apu::addrModePC);
			regPC += 2;
			cycles = 2;
			break;
		}
		case 0xc6:
		{
			// MOV (X),A
			doMoveWithRead(&apu::addrModeX, regA);
			regPC += 1;
			cycles = 4;
			break;
		}
		case 0x1d:
		{
			// DEC X
			doDecX();
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0xd0:
		{
			// BNE rel
			cycles = doBNE();
			break;
		}
		case 0x8f:
		{
			// MOV dp, #imm



			cycles = 5;
			break;
		}
		default:
		{
			// unknown opcode
			std::stringstream strr;
			strr << std::hex << std::setw(2) << std::setfill('0') << (int)nextOpcode;
			std::stringstream staddr;
			staddr << std::hex << std::setw(6) << std::setfill('0') << regPC;
			glbTheLogger.logMsg("Unknown opcode [" + strr.str() + "] at 0x" + staddr.str());
			return -1;
		}
	}

	return cycles;
}

apu::~apu()
{
	delete(spc700ram);
}
