#ifndef TESTMMU_H
#define TESTMMU_H

#include "genericMMU.h"

class testMMU: public genericMMU
{
private:

	unsigned char* snesRAM;

public:

	testMMU();
	~testMMU();
	void write8(unsigned int address, unsigned char val);
	unsigned char read8(unsigned int address);
};

#endif
