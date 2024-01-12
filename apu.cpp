
#include "apu.h"
#include <cstdlib> 

apu::apu()
{
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
}
