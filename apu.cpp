
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

	return 0;
}

void apu::write8(unsigned int address, unsigned char val)
{
	if ((address == 0x2140) || (address == 0x2141) || (address == 0x2142) || (address == 0x2143))
	{
		apuChan[address - 0x2140] = val;
	}
}

apu::~apu()
{
	delete(spc700ram);
}
