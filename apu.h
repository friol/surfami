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

	unsigned char* spc700ram;

	// SPC700 registers
	unsigned char regA;
	unsigned char regX;
	unsigned char regY;
	unsigned short int regSP;
	unsigned char regPSW;
	unsigned short int regPC;

	bool flagN = false; // negative flag (high bit of result was set)
	bool flagV = false; // overflow flag (signed overflow indication)
	bool flagP = false; // direct page flag (moves the direct page to $0100 if set)
	bool flagB = false; // break
	bool flagH = false; // half-carry flag (carry between low and high nibble)
	bool flagI = false; // interrupt enable (unused: the S-SMP has no attached interrupts)
	bool flagZ = false; // zero flag (set if last result was zero)
	bool flagC = false; // carry flag

	void doFlagsNZ(unsigned char val);

	typedef unsigned short int (apu::*pAddrModeFun)(void);
	void doMoveToX(pAddrModeFun fn);
	void doMoveToA(pAddrModeFun fn);
	void doMoveWithRead(pAddrModeFun fn, unsigned char val);
	void doDecX();
	int doBNE();

	// addressing modes
	unsigned short int addrModePC();
	unsigned short int addrModeX();

public:

	apu();
	void reset();
	~apu();

	bool isBootRomLoaded();

	unsigned char read8(unsigned int addr);
	void write8(unsigned int addr,unsigned char val);

	int stepOne();

	unsigned short int getPC() { return regPC; }

	std::vector<std::string> getRegisters();

};

#endif
