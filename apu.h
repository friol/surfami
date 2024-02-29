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
	unsigned short int addrImmediate8();

	typedef unsigned char (apu::* internalMemReader)(unsigned int);
	typedef void (apu::* internalMemWriter)(unsigned int,unsigned char);

public:

	apu();
	void reset();
	~apu();

	bool isBootRomLoaded();

	internalMemReader read8;
	internalMemWriter write8;

	unsigned char internalRead8(unsigned int addr);
	void internalWrite8(unsigned int addr,unsigned char val);

	unsigned char testMMURead8(unsigned int addr);
	void testMMUWrite8(unsigned int addr, unsigned char val);

	int stepOne();
	void setState(unsigned short int initialPC, unsigned char initialA, unsigned char initialX, unsigned char initialY,
		unsigned short int initialSP, unsigned char initialFlags);
	void useTestMMU();

	unsigned short int getPC() { return regPC; }
	unsigned short int getSP() { return regSP; }
	unsigned char getA() { return regA; }
	unsigned char getX() { return regX; }
	unsigned char getY() { return regY; }
	unsigned char getPSW();

	std::vector<std::string> getRegisters();
};

#endif
