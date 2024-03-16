#ifndef PPU_H
#define PPU_H

#include <vector>
#include <string>

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

	const int vramViewerXsize = 256;
	const int vramViewerYsize = 256;
	unsigned char* vramViewerBitmap;
	void tileViewerRenderTile2bpp(int px, int py, int tileAddr);
	void tileViewerRenderTile4bpp(int px, int py, int tileAddr);
	void tileViewerRenderTile8bpp(int px, int py, int tileAddr);

	unsigned int coldataColor = 0;
	unsigned char bgColorAppo[4][256 * 4];
	unsigned char bgPriorityAppo[4][256];
	bool bgIsTransparentAppo[4][256];
	unsigned char objColorAppo[256 * 4];
	unsigned char objPriorityAppo[256];
	bool objIsTransparentAppo[256];
	void resetAppoBuffers();

	unsigned char* screenFramebuffer;

	void renderTile(int bpp,int px, int py, int tileNum, int palId, int bgnum, int xflip, int yflip);

	void renderBackdrop();
	void renderSpritesScanline(int scanlinenum);

	void calculateMode7Starts(int y);
	unsigned char getPixelForMode7(int x,int layer,bool priority);
	void renderMode7Scanline(int scanlinenum);

	void buildTilemapMap(unsigned short int tilemapMap[][64], int bgSize, int baseTileAddr);
	void calcOffsetPerTileScroll(unsigned short int bg3word, unsigned short int bg3word2, int bgnum, int& xscroll, int& yscroll);

	unsigned int scanline=0;
	unsigned int internalCyclesCounter = 0;

	unsigned int vblankStartScanline = 0xf0;
	unsigned int cyclesPerScanline = 1364/6;
	unsigned int hdmaStartingPos = (256 * 4)/6;
	unsigned int totScanlines = 262;
	bool hdmaStartedForThisLine = false;

	bool writeBreakpoint = false;
	bool opvctFlipFlop = false;
	bool oamPriRot = false;

public:

	bool getWriteBreakpoint() { return writeBreakpoint; }

	ppu();
	void writeRegister(int reg, unsigned char val);
	void setINIDISP(unsigned char val) 
	{ 
		iniDisp = val; 
		/*if ((screenDisabled & 0x80) && (!(val & 0x80)))
		{
			resetOAMAddress();
		}*/
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
		//if (cgwSel & 0x01)
		//{
		//	bool directMode = 1;
		//}
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
		if (opvctFlipFlop == false)
		{
			opvctFlipFlop = true;
			return scanline&0xff;
		}
		else
		{
			opvctFlipFlop = false;
			return (scanline >> 8) & 0x01;
		}
	}

	void resetOpvctFlipFlop() { opvctFlipFlop = false; }

	~ppu();
};

#endif
