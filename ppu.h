#ifndef PPU_H
#define PPU_H

class mmu;
class cpu5a22;

class ppu
{
private:

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
	bool bgScrollXFlipFlop[4];
	bool bgScrollYFlipFlop[4];
	unsigned short int bgScrollX[4];
	unsigned short int bgScrollY[4];

	const int vramViewerXsize = 256;
	const int vramViewerYsize = 256;
	unsigned char* vramViewerBitmap;
	void tileViewerRenderTile2bpp(int px, int py, int tileAddr);

	unsigned int ppuResolutionX = 256;
	unsigned int ppuResolutionY = 224;
	unsigned char* screenFramebuffer;
	
	void renderTile(int bpp,int px, int py, int tileNum, int palId, int bgnum, int xflip, int yflip);
	void renderSprite(int px, int py, int tileNum, int palId);

	void renderBackdrop();
	void renderBG(int bgnum,int bpp);
	void renderSprites();

	void buildTilemapMap(unsigned short int tilemapMap[][64], int bgSize, int baseTileAddr);

	unsigned int scanline=0;
	unsigned int internalCyclesCounter = 0;

	const unsigned int vblankStartScanline = 0xf0;
	const unsigned int cyclesPerScanline = 1364;
	const unsigned int totScanlines = 262;

public:

	ppu();
	void writeRegister(int reg, unsigned char val);
	void setINIDISP(unsigned char val) { iniDisp = val; }
	void setOBSEL(unsigned char val) { obSel = val; }
	void writeBgScrollX(int bgId, unsigned char val);
	void writeBgScrollY(int bgId, unsigned char val);
	unsigned char vmDataRead(unsigned int port);
	
	void writeOAMAddressLow(unsigned char val) { OAMAddr = (OAMAddr & 0xff00) | val; }
	void writeOAMAddressHigh(unsigned char val) { OAMAddr = (OAMAddr & 0xff) | (val<<8); }
	void writeOAM(unsigned char val);
	unsigned char readOAM() { return OAM[OAMAddr++ & 0x1ff]; }

	void getPalette(unsigned char* destArr);
	int getCurrentBGMode();
	void tileViewerRenderTiles();
	void renderScreen();

	int getVRAMViewerXsize() { return vramViewerXsize; }
	int getVRAMViewerYsize() { return vramViewerYsize; }
	unsigned char* getVRAMViewerBitmap() { return vramViewerBitmap; }

	int getPPUResolutionX() { return ppuResolutionX; }
	int getPPUResolutionY() { return ppuResolutionY; }
	unsigned char* getPPUFramebuffer() { return screenFramebuffer; }

	void step(int numCycles,mmu& theMMU,cpu5a22& theCPU);
	bool isVBlankActive() { return scanline >= vblankStartScanline; }

	unsigned short int* getVRAMPtr() { return vram; }

	~ppu();
};

#endif
