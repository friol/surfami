/* mem mgmt unit */

#ifndef MMU_H
#define MMU_H

#include <iomanip>
#include <sstream>
#include "ppu.h"

class mmu
{
private:

	ppu* pPPU;
	unsigned char snesRAM[0x10000];

	void DMAstart(unsigned char val);

public:

	mmu(ppu& thePPU);
	void write8(unsigned int address, unsigned char val);
	unsigned char read8(unsigned int address);
	~mmu();

};

#endif
