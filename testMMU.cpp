
#include "testMMU.h"

testMMU::testMMU()
{
	snesRAM = new unsigned char[0x1000000];

	for (int loc = 0;loc < 0x1000000;loc++)
	{
		snesRAM[loc] = 0;
	}
}

void testMMU::write8(unsigned int address, unsigned char val)
{
	snesRAM[address] = val;
}

unsigned char testMMU::read8(unsigned int address)
{
	return snesRAM[address];
}

testMMU::~testMMU()
{
	delete(snesRAM);
}
