/* mem mgmt unit */

#ifndef MMU_H
#define MMU_H

#include <iomanip>
#include <sstream>
#include <string>
#include <fstream>
#include "ppu.h"
#include "apu.h"
#include "cpu5a22.h"
#include "genericMMU.h"

struct DMA 
{
public:
	bool enabled = false;
	bool repeat = false;
	bool terminated = false;
	unsigned char direction = 0;
	unsigned char addressing_mode = 0;
	unsigned char dma_mode = 0;
	unsigned char IO = 0;
	unsigned int indirect_address = 0;
	unsigned char line_counter = 0;
	unsigned int aaddress = 0;
	unsigned int address = 0;

	DMA() {};
	void hdmaEnable(bool e) 
	{
		enabled = e;
	}
};

class mmu: public genericMMU
{
private:

	cpu5a22* pCPU;
	ppu* pPPU;
	apu* pAPU;
	unsigned char* snesRAM;

	bool nmiFlag = false;
	unsigned char nmiTimen = 0;
	
	bool hasSRAM = false;
	std::string sramFileName = "";
	int standard = 0; // 0 NTSC, 1 PAL

	void DMAstart(unsigned char val);
	DMA HDMAS[8];
	void mmuDMATransfer(unsigned char dma_mode, unsigned char dma_dir, unsigned char dma_step,unsigned int& cpu_address, unsigned char io_address);
	//const int hdmaAmount = 128;

	unsigned char wram281x[3];

	bool isKeyDownPressed = false;
	bool isKeyUpPressed = false;
	bool isKeyLeftPressed = false;
	bool isKeyRightPressed = false;
	bool isKeyStartPressed = false;
	bool isKeySelectPressed = false;
	bool isKeyAPressed = false;
	bool isKeyBPressed = false;
	bool isKeyXPressed = false;
	bool isKeyYPressed = false;

public:

	mmu(ppu& thePPU,apu& theAPU);
	void setCPU(cpu5a22& theCPU) { pCPU = &theCPU; }
	void write8(unsigned int address, unsigned char val);
	unsigned char read8(unsigned int address);
	unsigned char* getInternalRAMPtr() { return snesRAM; }
	void hasSram(std::string& sramName) { hasSRAM = true; sramFileName = sramName; }
	unsigned char get4200() { return nmiTimen; }

	void resetHDMA();
	void startHDMA();

	bool isVblankNMIEnabled() { return ((nmiTimen & 0x80) == 0x80); }
	void setNMIFlag() { nmiFlag = true; }
	void clearNMIFlag() { nmiFlag = false; }
	void setStandard(int val) { standard = val; }

	void pressSelectKey(bool val) { isKeySelectPressed = val; }
	void pressStartKey(bool val) { isKeyStartPressed = val; }
	void pressRightKey(bool val) { isKeyRightPressed = val; }
	void pressLeftKey(bool val) { isKeyLeftPressed = val; }
	void pressUpKey(bool val) { isKeyUpPressed = val; }
	void pressDownKey(bool val) { isKeyDownPressed = val; }
	void pressAKey(bool val) { isKeyAPressed = val; }
	void pressBKey(bool val) { isKeyBPressed = val; }
	void pressXKey(bool val) { isKeyXPressed = val; }
	void pressYKey(bool val) { isKeyYPressed = val; }
	~mmu();
};

#endif
