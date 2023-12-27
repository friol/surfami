#ifndef PPU_H
#define PPU_H

class ppu
{
private:

	int cgramIdx = 0;
	unsigned char cgram[512];

	unsigned char vram[0x10000];

	unsigned char vmainVRAMAddrIncrMode;
	unsigned char vramAddressLower;
	unsigned char vramAddressUpper;

	int bgTileBaseAddress;

	const int vramViewerXsize = 256;
	const int vramViewerYsize = 256;
	unsigned char* vramViewerBitmap;
	void tileViewerRenderTile2bpp(int px, int py, int tileAddr);

public:

	ppu();
	void writeRegister(int reg, unsigned char val);
	void getPalette(unsigned char* destArr);
	void tileViewerRenderTiles();

	int getVRAMViewerXsize() { return vramViewerXsize; }
	int getVRAMViewerYsize() { return vramViewerYsize; }
	unsigned char* getVRAMViewerBitmap() { return vramViewerBitmap; }

	~ppu();

};

#endif
