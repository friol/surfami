
#include "mmu.h"


mmu::mmu()
{
}

void mmu::write8(unsigned int address, unsigned char val)
{
	snesRAM[address] = val;
}

unsigned char mmu::read8(unsigned int address)
{
	return snesRAM[address];
}

mmu::~mmu()
{
}
