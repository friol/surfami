
#include "apu.h"

apu::apu()
{
}

unsigned char apu::read8(int address)
{
	if (address == 0x2140)
	{
		// 2140h - APUI00 - Main CPU to Sound CPU Communication Port 0 (R / W)
		return 0xaa;
	}
	else if (address == 0x2141)
	{
		// 2141h - APUI01 - Main CPU to Sound CPU Communication Port 1 (R/W)
		return 0xbb;
	}

	return 0;
}

apu::~apu()
{
}
