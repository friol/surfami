
/* PPU - surFami */

#include "ppu.h"

ppu::ppu()
{
	for (int addr = 0;addr < 0x10000;addr++)
	{
		vram[addr] = 0xff;
	}

	vramViewerBitmap = new unsigned char[vramViewerXsize * vramViewerYsize * 4];
	for (int pos = 0;pos < (vramViewerXsize * vramViewerYsize * 4);pos++)
	{
		vramViewerBitmap[pos] = 0;
	}
}

void ppu::writeRegister(int reg, unsigned char val)
{
	if (reg == 0x2121)
	{
		// Register $2121 : Address for accessing CGRAM(1b / W)
		cgramIdx = ((int)val) * 2;
	}
	else if (reg == 0x2122)
	{
		// Register $2122: Data write to CGRAM (1b/W)
		cgram[cgramIdx] = val;
		cgramIdx += 1;
	}
	else if (reg == 0x2105)
	{
		// 2105h - BGMODE - BG Mode and BG Character Size (W)
		// TODO
		/*
			7    BG4 Tile Size(0 = 8x8, 1 = 16x16);\(BgMode0..4: variable 8x8 or 16x16)
			6    BG3 Tile Size(0 = 8x8, 1 = 16x16); (BgMode5: 8x8 acts as 16x8)
			5    BG2 Tile Size(0 = 8x8, 1 = 16x16); (BgMode6: fixed 16x8 ? )
			4    BG1 Tile Size(0 = 8x8, 1 = 16x16); / (BgMode7: fixed 8x8)
			3    BG3 Priority in Mode 1 (0 = Normal, 1 = High)
			2 - 0  BG Screen Mode(0..7 = see below)
		*/

	}
	else if (reg == 0x2115)
	{
		// 2115h - VMAIN - VRAM Address Increment Mode (W)
		vmainVRAMAddrIncrMode = val;
	}
	else if (reg == 0x2116)
	{
		// 2116h - VMADDL - VRAM Address (lower 8bit) (W)
		vramAddressLower = val;
	}
	else if (reg == 0x2117)
	{
		// 2117h - VMADDH - VRAM Address (higher 8bit) (W)
		vramAddressUpper = val;
	}
	else if ((reg == 0x2118)||(reg==0x2119))
	{
		// 2118h - VMDATAL - VRAM Data Write (lower 8bit) (W)
		// 2119h - VMDATAH - VRAM Data Write (upper 8bit) (W)

		int vramAddr = (vramAddressLower | (vramAddressUpper << 8));
		unsigned char _v_hi_lo = vmainVRAMAddrIncrMode >> 7;

		if (reg == 0x2118)
		{
			vram[vramAddr+1] = val;
		}
		else 
		{
			vram[vramAddr] = val;
		}

		if ((reg == 0x2118 && !_v_hi_lo) || (reg == 0x2119 && _v_hi_lo))
		{
			unsigned short int nextAddr = vramAddr;
			nextAddr += 2;
			vramAddressLower = nextAddr & 0xff;
			vramAddressUpper = nextAddr >> 8;
		}
	}
	else if (reg == 0x210b)
	{
		// 210Bh/210Ch - BG12NBA/BG34NBA - BG Character Data Area Designation (W)
		/*
		  15-12 BG4 Tile Base Address (in 4K-word steps)
		  11-8  BG3 Tile Base Address (in 4K-word steps)
		  7-4   BG2 Tile Base Address (in 4K-word steps)
		  3-0   BG1 Tile Base Address (in 4K-word steps)
		  Ignored in Mode 7 (Base is always zero).
		*/
		bgTileBaseAddress = bgTileBaseAddress | val;
	}
	else if (reg == 0x210c)
	{
		bgTileBaseAddress = (bgTileBaseAddress&0xff) | (val<<8);
	}


}

void ppu::getPalette(unsigned char* destArr)
{
	for (int b = 0;b < 512;b++)
	{
		destArr[b] = cgram[b];
	}
}

void ppu::tileViewerRenderTile2bpp(int px, int py, int tileAddr)
{
	unsigned char* pBuf;

	unsigned char grayArr[] = { 0,0x80,0xc0,0xff };

	for (int y = 0;y < 8;y++)
	{
		pBuf = &vramViewerBitmap[(px * 4) + ((py+y) * vramViewerXsize * 4)];

		int loByte = vram[tileAddr];
		int hiByte = vram[tileAddr+1];

		for (int x = 7;x>=0;x--)
		{
			int curCol = ((loByte >> x) & 1) + (((hiByte >> x) & 1) * 2);
			unsigned char col = grayArr[curCol];
			*pBuf = col; pBuf++; *pBuf = col; pBuf++; *pBuf = col; pBuf++; *pBuf = col; pBuf++;
		}

		tileAddr += 2;
	}
}

void ppu::tileViewerRenderTiles()
{
	int tileAddr = 0;
	for (int y = 0;y < 16;y++)
	{
		for (int x = 0;x < 16;x++)
		{
			tileViewerRenderTile2bpp(x * 8, y * 8, tileAddr);
			tileAddr += 16;
		}
	}
}

ppu::~ppu()
{
	delete vramViewerBitmap;
}
