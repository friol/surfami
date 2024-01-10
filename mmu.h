/* mem mgmt unit */

#ifndef MMU_H
#define MMU_H

#include <iomanip>
#include <sstream>
#include "ppu.h"
#include "apu.h"
#include "cpu5a22.h"
#include "genericMMU.h"

class mmu: public genericMMU
{
private:

	cpu5a22* pCPU;
	ppu* pPPU;
	apu* pAPU;
	unsigned char* snesRAM;

	bool nmiFlag = false;
	unsigned char nmiTimen = 0;

	void DMAstart(unsigned char val);

	unsigned char wram281x[3];

	bool isDownPressed = false;

public:

	mmu(ppu& thePPU,apu& theAPU);
	void setCPU(cpu5a22& theCPU) { pCPU = &theCPU; }
	void write8(unsigned int address, unsigned char val);
	unsigned char read8(unsigned int address);
	unsigned char* getInternalRAMPtr() { return snesRAM; }

	bool isVblankNMIEnabled() { return ((nmiTimen & 0x80) == 0x80); }
	void setNMIFlag() { nmiFlag = true; }
	void clearNMIFlag() { nmiFlag = false; }

	void pressDownKey() { isDownPressed = true; }
	~mmu();
};

#endif
