/* mem mgmt unit */

#ifndef MMU_H
#define MMU_H

#include <iomanip>
#include <sstream>
#include "ppu.h"
#include "apu.h"

class mmu
{
private:

	ppu* pPPU;
	apu* pAPU;
	unsigned char* snesRAM;

	void DMAstart(unsigned char val);

public:

	mmu(ppu& thePPU,apu& theAPU);
	void write8(unsigned int address, unsigned char val);
	unsigned char read8(unsigned int address);
	unsigned char* getInternalRAMPtr() { return snesRAM; }
	~mmu();

};

#endif
