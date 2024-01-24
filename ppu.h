#ifndef PPU_H
#define PPU_H

class mmu;
class cpu5a22;

class ppu
{
private:

	int standard = 0; // 0 NTSC, 1 PAL
	int cgramIdx = 0;
	unsigned char cgram[512];
	unsigned short int vram[0x8000];
	unsigned char OAM[0x220];
	unsigned short int OAMAddr=0;
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

	int bgTileMapBaseAddress[4];
	unsigned short int bgScrollX[4];
	unsigned short int bgScrollY[4];
	unsigned char BGSCROLL_L1;
	unsigned char BGSCROLL_L2;

	const int vramViewerXsize = 256;
	const int vramViewerYsize = 256;
	unsigned char* vramViewerBitmap;
	void tileViewerRenderTile2bpp(int px, int py, int tileAddr);

	unsigned int ppuResolutionX = 256;
	unsigned int ppuResolutionY = 224;

	unsigned char bgColorAppo[4][256 * 4];
	unsigned char bgPriorityAppo[4][256];
	bool bgIsTransparentAppo[4][256];
	unsigned char objColorAppo[256 * 4];
	unsigned char objPriorityAppo[256];
	bool objIsTransparentAppo[256];
	void resetAppoBuffers();

	unsigned char* screenFramebuffer;

	void renderTile(int bpp,int px, int py, int tileNum, int palId, int bgnum, int xflip, int yflip);
	void renderSprite(int px, int py, int tileNum, int palId);

	void renderBackdrop();
	/*void renderBG(int bgnum, int bpp);*/
	void renderSprites();
	void renderSpritesScanline(int scanlinenum);

	void buildTilemapMap(unsigned short int tilemapMap[][64], int bgSize, int baseTileAddr);

	unsigned int scanline=0;
	unsigned int internalCyclesCounter = 0;

	unsigned int vblankStartScanline = 0xf0;
	unsigned int cyclesPerScanline = 1364/6;
	unsigned int hdmaStartingPos = (256 * 4)/6;
	unsigned int totScanlines = 262;
	bool hdmaStartedForThisLine = false;

public:

	ppu();
	void writeRegister(int reg, unsigned char val);
	void setINIDISP(unsigned char val) { iniDisp = val; }
	void setOBSEL(unsigned char val) { obSel = val; }
	void writeBgScrollX(int bgId, unsigned char val);
	void writeBgScrollY(int bgId, unsigned char val);
	unsigned char vmDataRead(unsigned int port);
	
	void writeOAMAddressLow(unsigned char val) 
	{ 
		OAMAddr = (OAMAddr & 0x0200) | (val << 1);
		//OAMAddr = (OAMAddr & 0xff00) | val; 
	}
	
	void writeOAMAddressHigh(unsigned char val) 
	{ 
		OAMAddr = ((val & 1) << 9) | (OAMAddr & 0x01fe);
		//OAMAddr = (OAMAddr & 0xff) | (val<<8); 
	}

	void writeOAM(unsigned char val);
	unsigned char readOAM() { return OAM[OAMAddr++ & 0x1ff]; }
	unsigned char* getOAMPtr() { return OAM; }

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

	unsigned short int* getVRAMPtr() { return vram; }
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

	~ppu();
};

#endif
