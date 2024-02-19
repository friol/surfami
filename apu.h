#ifndef APU_H
#define APU_H

// APU+SPC700

#include "logger.h"

class apu
{
private:

	unsigned char apuChan[4] = { 0xaa,0,0,0 };
	
	unsigned char bootRom[64];
	bool bootRomLoaded = false;

	// SPC700 registers
	unsigned char regA;
	unsigned char regX;
	unsigned char regY;
	unsigned char regSP;
	unsigned char regPSW;
	unsigned short int regPC;

	unsigned char* spc700ram;

public:

	apu();
	void reset();
	~apu();

	bool isBootRomLoaded();

	unsigned char read8(unsigned int addr);
	void write8(unsigned int addr,unsigned char val);

};

#endif
