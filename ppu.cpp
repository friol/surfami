
/* PPU - surFami */

#include "ppu.h"

ppu::ppu()
{
	for (int bg = 0;bg < 3;bg++)
	{
		bgTileMapBaseAddress[bg] = 0;
	}

	for (int addr = 0;addr < 0x8000;addr++)
	{
		vram[addr] = 0;
	}

	screenFramebuffer = new unsigned char[ppuResolutionX * ppuResolutionY * 4];

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
		
		bgMode = val;
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
			vram[vramAddr & 0x7fff] = (vram[vramAddr & 0x7fff] & 0xff00) | val;
		}
		else 
		{
			vram[vramAddr & 0x7fff] = (vram[vramAddr & 0x7fff] & 0xff) | (val << 8);
		}

		if ((reg == 0x2118 && !_v_hi_lo) || (reg == 0x2119 && _v_hi_lo))
		{
			unsigned int nextAddr = vramAddr;
			nextAddr++;
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
	else if ((reg == 0x2107)|| (reg == 0x2108)|| (reg == 0x2109)|| (reg == 0x210A))
	{
		// BGx Screen Base and Screen Size
		bgTileMapBaseAddress[reg-0x2107] = val;
	}
	else if (reg == 0x212c)
	{
		mainScreenDesignation = val;
	}
}

int ppu::getCurrentBGMode()
{
	int screenMode = (bgMode & 0x03);
	return screenMode;
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

		int hiByte = vram[tileAddr]&0xff;
		int loByte = vram[tileAddr]>>8;

		for (int x = 7;x>=0;x--)
		{
			int curCol = ((loByte >> x) & 1) + (((hiByte >> x) & 1) * 2);
			unsigned char col = grayArr[curCol];
			*pBuf = col; pBuf++; *pBuf = col; pBuf++; *pBuf = col; pBuf++; *pBuf = col; pBuf++;
		}

		tileAddr += 1;
	}
}

void ppu::tileViewerRenderTiles()
{
	int tileAddr = 0;
	for (int y = 0;y < 24;y++)
	{
		for (int x = 0;x < 16;x++)
		{
			tileViewerRenderTile2bpp(x * 8, y * 8, tileAddr);
			tileAddr += 8;
		}
	}
}

void ppu::renderBackdrop()
{
	unsigned int backdropColor = (((int)(cgram[1] & 0x7f)) << 8) | cgram[0];
	unsigned char red = backdropColor & 0x1f; red <<= 3;
	unsigned char green = (backdropColor >> 5) & 0x1f; green <<= 3;
	unsigned char blue = (backdropColor >> 10) & 0x1f; blue <<= 3;

	for (unsigned int pos = 0;pos < (ppuResolutionX * ppuResolutionY * 4);pos+=4)
	{
		screenFramebuffer[pos] = red;
		screenFramebuffer[pos+1] = green;
		screenFramebuffer[pos+2] = blue;
		screenFramebuffer[pos+3] = 0xff;
	}
}

void ppu::renderTile2bpp(int px, int py, int tileNum,int palId)
{
	unsigned char* pBuf;
	unsigned char palArr[3*4];

	int colidx = (palId * 4) * 2;

	for (int col = 0;col < 4;col++)
	{
		unsigned int palcol = (((int)(cgram[colidx + 1] & 0x7f)) << 8) | cgram[colidx];
		int red = palcol & 0x1f; red <<= 3;
		int green = (palcol >> 5) & 0x1f; green <<= 3;
		int blue = (palcol >> 10) & 0x1f; blue <<= 3;

		palArr[(col * 3) + 0] = red;
		palArr[(col * 3) + 1] = green;
		palArr[(col * 3) + 2] = blue;

		colidx += 2;
	}

	int tileAddr = tileNum * 8;

	for (int y = 0;y < 8;y++)
	{
		pBuf = &screenFramebuffer[(px * 4) + ((py + y) * ppuResolutionX * 4)];

		int loByte = vram[tileAddr] & 0xff;
		int hiByte = vram[tileAddr] >> 8;

		for (int x = 7;x >= 0;x--)
		{
			int curCol = ((loByte >> x) & 1) + (((hiByte >> x) & 1) * 2);

			if ((curCol % 4) != 0)
			{
				*pBuf = palArr[(curCol * 3) + 0]; pBuf++;
				*pBuf = palArr[(curCol * 3) + 1]; pBuf++;
				*pBuf = palArr[(curCol * 3) + 2]; pBuf++;
				*pBuf = 0xff; pBuf++;
			}
			else
			{
				pBuf+=4;
			}
		}

		tileAddr += 1;
	}

}

void ppu::renderTile4bpp(int px, int py, int tileNum, int palId)
{
	unsigned char* pBuf;
	const int numCols = 16;
	unsigned char palArr[3 * numCols];

	int colidx = (palId * numCols) * 2;

	for (int col = 0;col < numCols;col++)
	{
		unsigned int palcol = (((int)(cgram[colidx + 1] & 0x7f)) << 8) | cgram[colidx];
		int red = palcol & 0x1f; red <<= 3;
		int green = (palcol >> 5) & 0x1f; green <<= 3;
		int blue = (palcol >> 10) & 0x1f; blue <<= 3;

		palArr[(col * 3) + 0] = red;
		palArr[(col * 3) + 1] = green;
		palArr[(col * 3) + 2] = blue;

		colidx += 2;
	}

	int tileAddr = tileNum * 16;

	for (int y = 0;y < 8;y++)
	{
		pBuf = &screenFramebuffer[(px * 4) + ((py + y) * ppuResolutionX * 4)];

		//int loByte = vram[tileAddr] & 0xff;
		//int hiByte = vram[tileAddr] >> 8;

		const unsigned char b_1 = vram[tileAddr] & 0xff;
		const unsigned char b_2 = vram[tileAddr] >> 8;
		const unsigned char b_3 = vram[tileAddr + 8] & 0xff;
		const unsigned char b_4 = vram[tileAddr + 8] >> 8;


		for (int x = 7;x >= 0;x--)
		{
			//int curCol = ((loByte >> x) & 1) + (((hiByte >> x) & 1) * 2);
			const unsigned short int curCol = ((b_1 >> x) & 1) +
				(2 * ((b_2 >> x) & 1)) +
				(4 * ((b_3 >> x) & 1)) +
				(8 * ((b_4 >> x) & 1));

			*pBuf = palArr[(curCol * 3) + 0]; pBuf++;
			*pBuf = palArr[(curCol * 3) + 1]; pBuf++;
			*pBuf = palArr[(curCol * 3) + 2]; pBuf++;
			*pBuf = 0xff; pBuf++;
		}

		tileAddr += 1;
	}

}

void ppu::renderTile8bpp(int px, int py, int tileNum, int palId)
{
	unsigned char* pBuf;
	const int numCols = 256;
	unsigned char palArr[3 * numCols];

	int colidx = (palId * numCols) * 2;

	for (int col = 0;col < numCols;col++)
	{
		unsigned int palcol = (((int)(cgram[colidx + 1] & 0x7f)) << 8) | cgram[colidx];
		int red = palcol & 0x1f; red <<= 3;
		int green = (palcol >> 5) & 0x1f; green <<= 3;
		int blue = (palcol >> 10) & 0x1f; blue <<= 3;

		palArr[(col * 3) + 0] = red;
		palArr[(col * 3) + 1] = green;
		palArr[(col * 3) + 2] = blue;

		colidx += 2;
	}

	int tileAddr = tileNum * 32;

	for (int y = 0;y < 8;y++)
	{
		pBuf = &screenFramebuffer[(px * 4) + ((py + y) * ppuResolutionX * 4)];

		//int loByte = vram[tileAddr] & 0xff;
		//int hiByte = vram[tileAddr] >> 8;

		const unsigned char b_1 = vram[tileAddr] & 0xff;
		const unsigned char b_2 = vram[tileAddr] >> 8;
		const unsigned char b_3 = vram[tileAddr + 8] & 0xff;
		const unsigned char b_4 = vram[tileAddr + 8] >> 8;
		const unsigned char b_5 = vram[tileAddr + 16] & 0xff;
		const unsigned char b_6 = vram[tileAddr + 16] >> 8;
		const unsigned char b_7 = vram[tileAddr + 24] & 0xff;
		const unsigned char b_8 = vram[tileAddr + 24] >> 8;

		for (int x = 7;x >= 0;x--)
		{
			const unsigned short int curCol = ((b_1 >> x) & 1) +
				(2 * ((b_2 >> x) & 1)) +
				(4 * ((b_3 >> x) & 1)) +
				(8 * ((b_4 >> x) & 1)) +
				(16 * ((b_5 >> x) & 1)) +
				(32 * ((b_6 >> x) & 1)) +
				(64 * ((b_7 >> x) & 1)) +
				(128 * ((b_8 >> x) & 1));

			*pBuf = palArr[(curCol * 3) + 0]; pBuf++;
			*pBuf = palArr[(curCol * 3) + 1]; pBuf++;
			*pBuf = palArr[(curCol * 3) + 2]; pBuf++;
			*pBuf = 0xff; pBuf++;
		}

		tileAddr += 1;
	}

}

void ppu::renderBG(int bgnum,int bpp)
{
	int tileAddr = ((bgTileMapBaseAddress[bgnum] >> 2) << 10) & 0x7fff;

	for (int y = 0;y < 28;y++)
	{
		for (int x = 0;x < 32;x++)
		{
				int vramWord = vram[tileAddr];
				int tileNum = vramWord & 0x3ff;
				int palId = (vramWord >> 10) & 0x7;

				if (bpp==2) renderTile2bpp(x * 8, y * 8, tileNum, palId);
				else if (bpp==4) renderTile4bpp(x * 8, y * 8, tileNum, palId);
				else if (bpp==8) renderTile8bpp(x * 8, y * 8, tileNum, palId);
				tileAddr++;
		}
	}
}

void ppu::renderScreen()
{
	/*
		7-2  SC Base Address in VRAM (in 1K-word steps, aka 2K-byte steps)
		1-0  SC Size (0=One-Screen, 1=V-Mirror, 2=H-Mirror, 3=Four-Screen)
						(0=32x32, 1=64x32, 2=32x64, 3=64x64 tiles)
					(0: SC0 SC0    1: SC0 SC1  2: SC0 SC0  3: SC0 SC1   )
					(   SC0 SC0       SC0 SC1     SC1 SC1     SC2 SC3   )
		Specifies the BG Map addresses in VRAM. The "SCn" screens consists of 32x32 tiles each.
	*/

	// rendering depends on screen mode
	int screenMode = (bgMode & 0x03);
	if (screenMode == 0)
	{
		// 0      4-color     4-color     4-color     4-color   ;Normal   
		renderBackdrop();
		for (int bg = 3;bg >= 0;bg--)
		{
			if (((mainScreenDesignation&0x1f) & (1 << bg)) > 0)
			{
				renderBG(bg,2);
			}
		}
	}
	else if (screenMode == 0x01)
	{
		// 1      16-color    16-color    4-color     -         ;Normal
		renderBackdrop();
		if (((mainScreenDesignation & 0x1f) & (1 << 2)) > 0) renderBG(2,2);
		if (((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) renderBG(1,4);
		if (((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) renderBG(0,4);
	}
	else if (screenMode == 0x03)
	{
		// 3      256-color   16-color    -           -         ;Normal   
		renderBackdrop();
		if (((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) renderBG(1, 4);
		if (((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) renderBG(0, 8);
	}
}

ppu::~ppu()
{
	delete(screenFramebuffer);
	delete(vramViewerBitmap);
}
