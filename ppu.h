#ifndef PPU_H
#define PPU_H

class ppu
{
private:

	int cgramIdx = 0;
	unsigned char cgram[512];

	unsigned short int vram[0x8000];

	unsigned char vmainVRAMAddrIncrMode;
	unsigned char vramAddressLower;
	unsigned char vramAddressUpper;

	int bgMode;
	int bgTileBaseAddress;
	
	//int bg1TileMapBaseAddress = 0; // BG1
	int bgTileMapBaseAddress[4];

	const int vramViewerXsize = 256;
	const int vramViewerYsize = 256;
	unsigned char* vramViewerBitmap;
	void tileViewerRenderTile2bpp(int px, int py, int tileAddr);

	int ppuResolutionX = 256;
	int ppuResolutionY = 224;
	unsigned char* screenFramebuffer;
	void renderTile2bpp(int px, int py, int tileNum, int palId);
	void renderTile4bpp(int px, int py, int tileNum, int palId);
	void renderTile8bpp(int px, int py, int tileNum, int palId);

public:

	ppu();
	void writeRegister(int reg, unsigned char val);
	void getPalette(unsigned char* destArr);
	void tileViewerRenderTiles();
	void renderScreen();

	int getVRAMViewerXsize() { return vramViewerXsize; }
	int getVRAMViewerYsize() { return vramViewerYsize; }
	unsigned char* getVRAMViewerBitmap() { return vramViewerBitmap; }

	int getPPUResolutionX() { return ppuResolutionX; }
	int getPPUResolutionY() { return ppuResolutionY; }
	unsigned char* getPPUFramebuffer() { return screenFramebuffer; }

	~ppu();
};

#endif
