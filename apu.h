#ifndef APU_H
#define APU_H

// APU+SPC700

#include "logger.h"
#include "audioSystem.h"

class audioSystem;

struct spc700Timer 
{
	unsigned char cycles;
	unsigned char divider;
	unsigned char target;
	unsigned char counter;
	bool enabled;
};

struct spc700channel
{
	signed char leftVol;
	signed char rightVol;
	unsigned short int samplePitch;
	bool keyOn;
	bool keyOff;
	unsigned char sampleSourceEntry;
	unsigned short int BRRSampleStart;
	unsigned short int BRRLoopStart;
	unsigned char brrHeader;
	unsigned short int decodeOffset;
	int startDelay;
	unsigned char blockOffset; // offset within brr block
	unsigned char bufferOffset;
	unsigned short int pitchCounter;
	unsigned char adsrState; // 0: attack, 1: decay, 2: sustain, 3: release
	unsigned short int gain;
	bool useGain;
	unsigned char adsrRates[4]; // attack, decay, sustain, gain
	unsigned char sustainLevel;
	unsigned char gainSustainLevel;
	unsigned char gainMode;
	bool directGain;
	unsigned short int gainValue; // for direct gain
	unsigned short int preclampGain; // for bent increase

	signed short int decodeBuffer[12];

	float playingPos = 0.0;
};

class apu
{
private:

	unsigned char portsFromCPU[4];
	unsigned char portsFromSPC[4];

	spc700Timer timer[3];

	unsigned char bootRom[64];
	bool bootRomLoaded = false;
	bool romReadable = true;

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
	void doMoveToA(pAddrModeFun fn);
	void doMoveToX(pAddrModeFun fn);
	void doMoveToY(pAddrModeFun fn);
	void doMoveWithRead(pAddrModeFun fn, unsigned char val);
	void doCMPA(pAddrModeFun fn);
	void doCMPX(pAddrModeFun fn);
	void doCMPY(pAddrModeFun fn);
	void doCMP(pAddrModeFun fn, unsigned char val);
	void doDecX();
	void doInc(pAddrModeFun fn);
	void doAdc(pAddrModeFun fn);
	void doEor(pAddrModeFun fn);
	void doLsr(pAddrModeFun fn);
	void doAnd(pAddrModeFun fn);
	int doBNE();
	int doBranch(signed char offs, bool condition);


	// addressing modes
	unsigned short int addrModePC();
	unsigned short int addrModeX();
	unsigned short int addrImmediate8();
	unsigned short int addrIndirectX();
	unsigned short int addrIndirectY();
	unsigned short int addrDP();
	unsigned short int addrDPX();
	unsigned short int addrDPY();
	unsigned short int addrAbs();
	unsigned short int addrAbsX();
	unsigned short int addrAbsY();
	unsigned short int addrIndP();
	unsigned char addrAbsBit(unsigned short int* adr);

	typedef unsigned char (apu::* internalMemReader)(unsigned int);
	typedef void (apu::* internalMemWriter)(unsigned int,unsigned char);

	unsigned long int apuCycles = 0;

	// S-DSP

	unsigned char dspRam[0x80];

	unsigned char dspSelectedRegister = 0;
	void writeToDSPRegister(unsigned char val);
	void calcBRRSampleStart(int voiceNum);
	void dspCycle(signed short int& sampleOutL, signed short int& sampleOutR);
	signed short int dspGetSample(int ch);
	void dspDecodeBrr(int ch);
	void dspHandleGain(int ch);
	bool dspCheckCounter(int rate);

	signed char mainVolLeft = 0;
	signed char mainVolRight = 0;
	signed char echoVolLeft = 0;
	signed char echoVolRight = 0;

	unsigned char dspFlagReg = 0;
	unsigned short int dspDIR = 0;
	bool evenCycle;
	bool dspReset;
	bool mute;
	bool echoWrites;
	unsigned char noiseRate;

	unsigned short int counter=0;

public:

	apu();
	void reset();
	~apu();

	spc700channel channels[8];

	bool isBootRomLoaded();

	internalMemReader read8;
	internalMemWriter write8;

	unsigned char internalRead8(unsigned int addr);
	void internalWrite8(unsigned int addr,unsigned char val);

	unsigned char externalRead8(unsigned int addr);
	void externalWrite8(unsigned int addr, unsigned char val);

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

	// advance timers, etc.
	void step(audioSystem& theAudioSys);

	std::vector<std::string> getRegisters();
};

#endif
