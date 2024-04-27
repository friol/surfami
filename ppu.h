#ifndef PPU_H
#define PPU_H

#include <vector>
#include <string>
#include "logger.h"

extern logger glbTheLogger;

class mmu;
class cpu5a22;

class ppu
{
private:

	unsigned int ppuResolutionX = 256;
	unsigned int ppuResolutionY = 224;

	unsigned char* brightnessLookup;

	int standard = 0; // 0 NTSC, 1 PAL
	int cgramIdx = 0;
	
	unsigned char cgram[512];
	unsigned char palarrLookup[256 * 3];

	unsigned short int vram[0x8000];
	unsigned char OAM[0x220];
	unsigned short int OAMAddr=0;
	unsigned short int OAMAddrSave = 0;
	unsigned char OAM_Lsb = 0;

	unsigned char vmainVRAMAddrIncrMode;
	unsigned char vramAddressLower;
	unsigned char vramAddressUpper;

	int bgMode;
	int bgTileBaseAddress=0;
	int mainScreenDesignation=0;
	int subScreenDesignation = 0;
	unsigned char iniDisp = 0;
	unsigned char obSel = 0;
	unsigned char screenDisabled = 0;
	unsigned char cgwSel = 0;

	int bgTileMapBaseAddress[4];
	unsigned short int bgScrollX[4];
	unsigned short int bgScrollY[4];
	unsigned char BGSCROLL_L1;
	unsigned char BGSCROLL_L2;

	bool isBgActive[4] = { true,true,true,true };

	// mode se7en
	signed short int m7matrix[8] = { 0,0,0,0,0,0,0,0 };
	int m7prev = 0;
	bool m7largeField;
	bool m7charFill;
	bool m7xFlip;
	bool m7yFlip;
	bool m7extBg;
	int m7startX;
	int m7startY;

	// 

	const int vramViewerXsize = 256;
	const int vramViewerYsize = 256;
	unsigned char* vramViewerBitmap;
	void tileViewerRenderTile2bpp(int px, int py, int tileAddr);
	void tileViewerRenderTile4bpp(int px, int py, int tileAddr);
	void tileViewerRenderTile8bpp(int px, int py, int tileAddr);

	unsigned char coldataColor[3] = { 0,0,0 };
	
	unsigned char bgColorAppo[4][256 * 4];
	unsigned char bgPriorityAppo[4][256];
	bool bgIsTransparentAppo[4][256];
	unsigned char objColorAppo[256 * 4];
	unsigned char objPriorityAppo[256];
	bool objIsTransparentAppo[256];
	unsigned char objPaletteAppo[256];

	unsigned char bgColorAppoHires[4][512 * 4];
	unsigned char bgPriorityAppoHires[4][512];
	bool bgIsTransparentAppoHires[4][512];
	unsigned char objColorAppoHires[512 * 4];
	unsigned char objPriorityAppoHires[512];
	bool objIsTransparentAppoHires[512];

	unsigned char clampAdd(unsigned char a, unsigned char b, bool div2);
	unsigned char clampSub(unsigned char a, unsigned char b, bool div2);
	void mixLayers(int scanlinenum,int screenMode);
	void doColorMath(int maincol, int subcol, unsigned char redm, unsigned char greenm, unsigned char bluem,
		unsigned char reds, unsigned char greens, unsigned char blues,
		unsigned char& red, unsigned char& green, unsigned char& blue,
		unsigned char objPalette);
	void upskaleToHires(unsigned int scanlinenn);

	void resetAppoBuffers();
	void resetAppoBuffersHires();

	unsigned char* screenFramebuffer;
	unsigned char* screenFramebufferHires;

	void renderBackdrop();
	void renderSpritesScanline(int scanlinenum);

	void calculateMode7Starts(int y);
	unsigned char getPixelForMode7(int x,int layer,bool priority);
	void renderMode7Scanline(int scanlinenum);

	void buildTilemapMap(unsigned short int tilemapMap[][64], int bgSize, int baseTileAddr, unsigned int rowtobuild);
	void calcOffsetPerTileScroll(unsigned short int bg3word, unsigned short int bg3word2, int bgnum, int& xscroll, int& yscroll);

	void setPalarrColor(int idx);

	unsigned int scanline=0;

	unsigned int vblankStartScanline = 0xf0;
	unsigned int cyclesPerScanline = 1364/6;
	unsigned int hdmaStartingPos = (256 * 4)/6;
	unsigned int totScanlines = 262;
	bool hdmaStartedForThisLine = false;

	bool writeBreakpoint = false;
	bool opvctFlipFlop = false;
	bool oamPriRot = false;

	bool applyWindow(int x);

	int windowxpos[2][2];
	unsigned char windowTMW=0;
	unsigned char windowTSW=0;
	unsigned char w12sel = 0;
	unsigned char w34sel = 0;
	unsigned char wobjsel = 0;

public:

	unsigned char openBus = 0;
	unsigned char m_ppu2_open_bus = 0;
	unsigned int internalCyclesCounter = 0;

	bool getWriteBreakpoint() { return writeBreakpoint; }

	ppu();
	void writeRegister(int reg, unsigned char val);
	void setINIDISP(unsigned char val) 
	{ 
		iniDisp = val; 
		if ((screenDisabled & 0x80) && (!(val & 0x80)))
		{
			// TODO
			//resetOAMAddress();
			//OAMAddrSave = OAMAddr;
		}
		screenDisabled = val & 0x80;
	}
	
	void setOBSEL(unsigned char val) { obSel = val; }
	void writeBgScrollX(int bgId, unsigned char val);
	void writeBgScrollY(int bgId, unsigned char val);
	unsigned char vmDataRead(unsigned int port);

	void writeM7HOFS(unsigned char val);
	void writeM7VOFS(unsigned char val);
	void writeM7Matrix(int mtxparm,unsigned char val);
	void writeM7SEL(unsigned char val);

	void setCGWSEL(unsigned char val) 
	{ 
		cgwSel = val; 
		if (cgwSel & 0x01)
		{
			//glbTheLogger.logMsg("CGWSEL bit 0 direct color");
		}
	}

	unsigned char cgaddsub = 0;
	void setCGADDSUB(unsigned char val)
	{
		cgaddsub = val;
	}

	unsigned char setini = 0;
	void setSETINI(unsigned char val)
	{
		setini = val;
	}

	unsigned char mosaicReg = 0;
	void writeMosaic(unsigned char val)
	{
		mosaicReg = val;
	}

	void writeOAMAddressLow(unsigned char val) 
	{ 
		OAMAddr = (OAMAddr & 0x0200) | (val << 1);
		OAMAddrSave = OAMAddr;
	}
	
	void writeOAMAddressHigh(unsigned char val) 
	{ 
		if (val & 0x80)
		{
			oamPriRot = true;
		}
		else
		{
			oamPriRot = false;
		}

		OAMAddr = ((val & 1) << 9) | (OAMAddr & 0x01fe);
		OAMAddrSave = OAMAddr;
	}

	void writeOAM(unsigned char val);
	unsigned char readOAM() { return OAM[OAMAddr++ & 0x1ff]; }
	unsigned char* getOAMPtr() { return OAM; }

	unsigned char cgramRead(unsigned short int offset);

	void writeSubscreenFixedColor(unsigned char val);

	void getPalette(unsigned char* destArr);
	int getCurrentBGMode();
	void tileViewerRenderTiles();

	void renderBGScanline(int bgnum, int bpp, int scanlinenum);
	void renderTileScanline(int bpp, int px, int py, int tileNum, int palId, int bgnum, int xflip, int yflip, int scanlinenum, unsigned char bgpri);
	void renderScanline(int scanlinenum);
	void renderBackdropScanline(int scanlinenum);

	int getVRAMViewerXsize() { return vramViewerXsize; }
	int getVRAMViewerYsize() { return vramViewerYsize; }
	unsigned char* getVRAMViewerBitmap() { return vramViewerBitmap; }

	int getPPUResolutionX() { return ppuResolutionX; }
	int getPPUResolutionY() { return ppuResolutionY; }
	unsigned char* getPPUFramebuffer() { return screenFramebuffer; }
	unsigned char* getHiresFramebuffer() { return screenFramebufferHires; }

	void toggleBgActive(int bgnum) { isBgActive[bgnum] = !isBgActive[bgnum]; }

	void step(int numCycles,mmu& theMMU,cpu5a22& theCPU);
	bool isVBlankActive() { return scanline >= vblankStartScanline; }
	int getInternalCyclesCounter() { return internalCyclesCounter; }

	std::vector<std::string> getM7Matrix();
	unsigned char getMPY(unsigned int addr);

	unsigned short int* getVRAMPtr() { return vram; }
	unsigned char* getCGRAMPtr() { return cgram; }
	void setStandard(int val) 
	{ 
		standard = val; 
		if (standard == 0)
		{
			vblankStartScanline = 0xe0;
			totScanlines = 262;
		}
		else
		{
			vblankStartScanline = 0xe0;
			totScanlines = 312;
		}
	}

	int getCurrentScanline() 
	{ 
		return scanline;
	}

	int getCurrentVideomode()
	{
		return (bgMode & 0x07);
	}

	unsigned char m_stat78 = 0;

	~ppu();
};

#endif
