
/* PPU - surFami */

#include "ppu.h"
#include "mmu.h"
#include "cpu5a22.h"

ppu::ppu()
{
	for (int bg = 0;bg < 4;bg++)
	{
		bgTileMapBaseAddress[bg] = 0;
		bgScrollX[bg] = 0;
		bgScrollY[bg] = 0;
		bgScrollXFlipFlop[bg] = false;
		bgScrollYFlipFlop[bg] = false;
	}

	for (int addr = 0;addr < 0x8000;addr++)
	{
		vram[addr] = 0;
	}

	for (int addr = 0;addr < 0x220;addr++)
	{
		OAM[addr] = 0;
	}

	screenFramebuffer = new unsigned char[ppuResolutionX * ppuResolutionY * 4];

	vramViewerBitmap = new unsigned char[vramViewerXsize * vramViewerYsize * 4];
	for (int pos = 0;pos < (vramViewerXsize * vramViewerYsize * 4);pos++)
	{
		vramViewerBitmap[pos] = 0;
	}
}

void ppu::writeBgScrollX(int bgId,unsigned char val)
{
	if (!bgScrollXFlipFlop[bgId]) 
	{
		bgScrollX[bgId] = (bgScrollX[bgId] & 0xff00) | val;
		bgScrollXFlipFlop[bgId] = true;
	}
	else 
	{
		bgScrollX[bgId] = (bgScrollX[bgId] & 0xff) | (val << 8);
		bgScrollXFlipFlop[bgId] = false;
	}
}

void ppu::writeBgScrollY(int bgId, unsigned char val)
{
	if (!bgScrollYFlipFlop[bgId])
	{
		bgScrollY[bgId] = (bgScrollY[bgId] & 0xff00) | val;
		bgScrollYFlipFlop[bgId] = true;
	}
	else
	{
		bgScrollY[bgId] = (bgScrollY[bgId] & 0xff) | (val << 8);
		bgScrollYFlipFlop[bgId] = false;
	}
}

void ppu::writeOAM(unsigned char val)
{
	if (OAMAddr >= 0x220)
	{
		OAMAddr = 0;
	}

	if (OAMAddr % 2 == 0) 
	{
		OAM_Lsb = val;
	}
	else if (OAMAddr % 2 == 1 && OAMAddr < 0x200) 
	{
		OAM[OAMAddr - 1] = OAM_Lsb;
		OAM[OAMAddr] = val;
	}
	if (OAMAddr > 0x1ff) 
	{
		OAM[OAMAddr] = val;
	}
	
	OAMAddr++;
}

unsigned char ppu::vmDataRead(unsigned int port)
{
	//	PPU - VMDATAL - VRAM Write - Low (R)
	//	PPU - VMDATAH - VRAM Write - High (R)

	unsigned short int adr = (vramAddressUpper << 8) | vramAddressLower;
	unsigned char _v_hi_lo = vmainVRAMAddrIncrMode >> 7;
	unsigned char _v_trans = (vmainVRAMAddrIncrMode & 0b1100) >> 2;
	unsigned char _v_step = vmainVRAMAddrIncrMode & 0b11;
	unsigned short int _t_st, _t_off, _t_in;
	switch (_v_trans)
	{ //	PPU - Apply address translation if necessary (leftshift thrice lower 8, 9 or 10 bits)
	case 0b00:
		break;
	case 0b01:		//	8 bit, aaaaaaaYYYxxxxx becomes aaaaaaaxxxxxYYY
		_t_st = (adr & 0b1111111100000000);
		_t_off = (adr & 0b11100000) >> 5;
		_t_in = (adr & 0b11111) << 3;
		adr = _t_st | _t_off | _t_in;
		break;
	case 0b10:		//	9 bit, aaaaaaYYYxxxxxP becomes aaaaaaxxxxxPYYY
		_t_st = (adr & 0b1111111000000000);
		_t_off = (adr & 0b111000000) >> 6;
		_t_in = (adr & 0b111111) << 3;
		adr = _t_st | _t_off | _t_in;
		break;
	case 0b11:		//	10 bit, aaaaaYYYxxxxxPP becomes aaaaaxxxxxPPYYY
		_t_st = (adr & 0b1111110000000000);
		_t_off = (adr & 0b1110000000) >> 7;
		_t_in = (adr & 0b1111111) << 3;
		adr = _t_st | _t_off | _t_in;
		break;
	}
	if (((port == 0x2139 && !_v_hi_lo) || (port == 0x213a && _v_hi_lo)) && _v_trans != 0)
	{
		unsigned short int _t = (vramAddressUpper << 8) | vramAddressLower;
		switch (_v_step)
		{
		case 0b00: _t += 1;	break;
		case 0b01: _t += 32; break;
		case 0b10: _t += 128; break;
		case 0b11: _t += 128; break;
		default: break;
		}
		vramAddressLower = _t & 0xff;
		vramAddressUpper = _t >> 8;
	}
	return (port == 0x2139) ? vram[adr & 0x7fff] & 0xff : vram[adr & 0x7fff] >> 8;
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
		unsigned char _v_trans = (vmainVRAMAddrIncrMode & 0b1100) >> 2;

		if (_v_trans != 0)
		{
			int err = 1;
			err = 2;
		}

		int adrIncrStep = 1;
		int addrIncrementStep = vmainVRAMAddrIncrMode & 0x03;
		if (addrIncrementStep != 0)
		{
			// 1 - 0   Address Increment Step(0..3 = Increment Word - Address by 1, 32, 128, 128)
			const int increments[] = {1, 32, 128, 128};
			adrIncrStep = increments[addrIncrementStep];
		}

		if (reg == 0x2118)
		{
			//if (vramAddr == 0x2000)
			//{
			//	int w = 1;
			//	w = 2;
			//}
			vram[vramAddr & 0x7fff] = (vram[vramAddr & 0x7fff] & 0xff00) | val;
		}
		else 
		{
			vram[vramAddr & 0x7fff] = (vram[vramAddr & 0x7fff] & 0xff) | (val << 8);
		}

		if ((reg == 0x2118 && !_v_hi_lo) || (reg == 0x2119 && _v_hi_lo))
		{
			unsigned int nextAddr = vramAddr;
			nextAddr+=adrIncrStep;
			nextAddr &= 0x7fff;
			vramAddressLower = nextAddr & 0xff;
			vramAddressUpper = (nextAddr >> 8)&0xff;
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
		bgTileBaseAddress = (bgTileBaseAddress&0xff00) | val;
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
	else if (reg == 0x212d)
	{
		subScreenDesignation = val;
	}
}

int ppu::getCurrentBGMode()
{
	int screenMode = (bgMode & 0x07);
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

void ppu::renderTile(int bpp, int px, int py, int tileNum, int palId, int bgnum, int xflip, int yflip)
{
	unsigned char* pBuf;

	const int numCols = (bpp == 2) ? 4 : (bpp == 4) ? 16 : 256;
	unsigned char* palArr=new unsigned char[3 * numCols];

	int colidx;
	if (bpp == 2)
	{
		if ((bgMode & 0x07) == 0)
		{
			colidx = ((palId * 4) * 2)+(0x40 * bgnum);
		}
		else
		{
			colidx = ((palId * 4) * 2);
		}
	}
	else
	{
		colidx = (palId * numCols) * 2;
	}

	for (int col = 0;col < numCols;col++)
	{
		unsigned int palcol = (((int)(cgram[colidx + 1] & 0x7f)) << 8) | cgram[colidx];
		int red = palcol & 0x1f; red <<= 3;
		int green = (palcol >> 5) & 0x1f; green <<= 3;
		int blue = (palcol >> 10) & 0x1f; blue <<= 3;

		palArr[(col * 3) + 0] = (unsigned char)red;
		palArr[(col * 3) + 1] = (unsigned char)green;
		palArr[(col * 3) + 2] = (unsigned char)blue;

		colidx += 2;
	}

	//

	int multiplier = (bpp == 2) ? 8 : (bpp == 4) ? 16 : 32;
	int tileAddr = tileNum * multiplier;
	tileAddr += ((bgTileBaseAddress >> (4 * bgnum)) & 0x0f) * 1024 * 4;
	tileAddr &= 0x7fff;

	int ybase = 0; int yend = 8; int yinc = 1;
	if (yflip)
	{
		ybase = 7; yend = -1; yinc = -1;
	}

	for (int y = ybase;y != yend;y += yinc)
	{
		unsigned int ppos = (px * 4) + ((py + y) * ppuResolutionX * 4);
		if ((ppos >= 0) && (ppos < (ppuResolutionX * ppuResolutionY * 4)))
		{
			pBuf = &screenFramebuffer[ppos];

			const unsigned char b_1 = vram[tileAddr] & 0xff;
			const unsigned char b_2 = vram[tileAddr] >> 8;
			const unsigned char b_3 = vram[tileAddr + 8] & 0xff;
			const unsigned char b_4 = vram[tileAddr + 8] >> 8;
			const unsigned char b_5 = vram[tileAddr + 16] & 0xff;
			const unsigned char b_6 = vram[tileAddr + 16] >> 8;
			const unsigned char b_7 = vram[tileAddr + 24] & 0xff;
			const unsigned char b_8 = vram[tileAddr + 24] >> 8;

			int theX = px;
			int xbase = 7; int xend = -1; int xinc = -1;
			if (xflip)
			{
				xbase = 0; xend = 8; xinc = 1;
			}

			for (int x = xbase;x != xend;x += xinc)
			{
				if ((ppos >= 0) && (ppos < (ppuResolutionX * ppuResolutionY * 4)))
				{
					unsigned short int curCol;

					if (bpp == 2)
					{
						curCol = ((b_1 >> x) & 1) + (2 * ((b_2 >> x) & 1));
					}
					else if (bpp == 4)
					{
						curCol = ((b_1 >> x) & 1) + (2 * ((b_2 >> x) & 1)) + (4 * ((b_3 >> x) & 1)) + (8 * ((b_4 >> x) & 1));
					}
					else
					{
						curCol = ((b_1 >> x) & 1) +
							(2 * ((b_2 >> x) & 1)) +
							(4 * ((b_3 >> x) & 1)) +
							(8 * ((b_4 >> x) & 1)) +
							(16 * ((b_5 >> x) & 1)) +
							(32 * ((b_6 >> x) & 1)) +
							(64 * ((b_7 >> x) & 1)) +
							(128 * ((b_8 >> x) & 1));
					}

					if (bpp == 2)
					{
						if (((curCol % 4) != 0) && ((theX >= 0) && (theX < (signed int)ppuResolutionX)))
						{
							*pBuf = palArr[(curCol * 3) + 0]; pBuf++;
							*pBuf = palArr[(curCol * 3) + 1]; pBuf++;
							*pBuf = palArr[(curCol * 3) + 2]; pBuf++;
							*pBuf = 0xff; pBuf++;
						}
						else pBuf += 4;
					}
					else if (bpp == 4)
					{
						if (((curCol % 16) != 0) && ((theX >= 0) && (theX < (signed int)ppuResolutionX)))
						{
							*pBuf = palArr[(curCol * 3) + 0]; pBuf++;
							*pBuf = palArr[(curCol * 3) + 1]; pBuf++;
							*pBuf = palArr[(curCol * 3) + 2]; pBuf++;
							*pBuf = 0xff; pBuf++;
						}
						else pBuf += 4;
					}
					else
					{
						*pBuf = palArr[(curCol * 3) + 0]; pBuf++;
						*pBuf = palArr[(curCol * 3) + 1]; pBuf++;
						*pBuf = palArr[(curCol * 3) + 2]; pBuf++;
						*pBuf = 0xff; pBuf++;
					}
				}

				ppos += 4;
				theX += 1;
			}

			tileAddr += 1;
		}
	}

	delete(palArr);
}

void ppu::buildTilemapMap(unsigned short int tilemapMap[][64], int bgSize, int baseTileAddr)
{
	/*
		All tilemaps are 32x32, bits 0-1 simply select the number of 32x32 tilemaps and how they're laid out in memory

		00  32x32   AA
					AA
		01  64x32   AB
					AB
		10  32x64   AA
					BB
		11  64x64   AB
					CD
	*/

	unsigned int tilemapPos = baseTileAddr;

	for (int y = 0;y < 32;y++)
	{
		for (int x = 0;x < 32;x++)
		{
			tilemapMap[y][x] = vram[tilemapPos];
			tilemapPos++;
		}
	}

	if ((bgSize == 0) || (bgSize == 2)) tilemapPos -= 0x400;

	for (int y = 0;y < 32;y++)
	{
		for (int x = 32;x < 64;x++)
		{
			tilemapMap[y][x] = vram[tilemapPos];
			tilemapPos++;
		}
	}

	if (bgSize == 0) tilemapPos -= 0x400;

	for (int y = 32;y < 64;y++)
	{
		for (int x = 0;x < 32;x++)
		{
			tilemapMap[y][x] = vram[tilemapPos];
			tilemapPos++;
		}
	}

	if (bgSize == 0) tilemapPos -= 0x400;
	else if (bgSize == 1) tilemapPos -= 0x800;
	else if (bgSize == 2) tilemapPos-=0x400;

	for (int y = 32;y < 64;y++)
	{
		for (int x = 32;x < 64;x++)
		{
			tilemapMap[y][x] = vram[tilemapPos];
			tilemapPos++;
		}
	}

}

void ppu::renderBG(int bgnum,int bpp)
{
	int baseTileAddr = ((bgTileMapBaseAddress[bgnum] >> 2) << 10) & 0x7fff;

	int bgSize = bgTileMapBaseAddress[bgnum] & 0x3;
	int xscroll = bgScrollX[bgnum];
	int yscroll = bgScrollY[bgnum];

	unsigned short int tilemapMap[64][64];
	buildTilemapMap(tilemapMap, bgSize, baseTileAddr);

	for (int y = 0;y < 29;y++)
	{
		int realy = y + (yscroll / 8);

		for (int x = 0;x < 33;x++)
		{
			int realx = x + (xscroll / 8);

			unsigned short int vramWord = tilemapMap[realy&0x3f][realx&0x3f];

			int tileNum = vramWord & 0x3ff;
			if (tileNum == 0x0e)
			{
				int brk = 1; brk = 2;
			}
			int palId = (vramWord >> 10) & 0x7;
			//int bgPri = (vramWord >> 13) & 0x01;
			int xflip= (vramWord >> 14) & 0x01;
			int yflip= (vramWord >> 15) & 0x01;

			renderTile(bpp, (x * 8) + ((8 - xscroll) % 8), (y * 8) + ((8-yscroll)%8), tileNum, palId, bgnum, xflip, yflip);
		}
	}
}

void ppu::renderSprites()
{
	int spriteDimensions[8][2][2] = {
		{{8,8},{16,16}},
		{{8,8},{32,32}},
		{{8,8},{64,64}},
		{{16,16},{32,32}},
		{{16,16},{64,64}},
		{{32,32},{64,64}},
		{{16,32},{32,64}},
		{{16,32},{32,32}},
	};

	int spriteDim = (obSel >> 5)&0x7;

	// 4 - 3   Gap between OBJ 0FFh and 100h(0 = None) (4K - word steps) (8K - byte steps)
	int spriteGap = (obSel >> 3) & 0x03;
	if (spriteGap != 0)
	{
		int err = 1; err = 2;
	}

	for (auto i = 127; i >= 0; i--)
	{
		const unsigned char byte1 = OAM[(i * 4)];
		const unsigned char byte2 = OAM[(i * 4) + 1];
		const unsigned char byte3 = OAM[(i * 4) + 2];
		const unsigned char byte4 = OAM[(i * 4) + 3];
		const unsigned char attr = (OAM[512 + (i / 4)] >> ((i % 4) * 2)) & 0b11;

		int x_pos = byte1;
		int y_pos = byte2;
		int tile_nr = ((byte4 & 1) << 8) | byte3;
		int y_flip = byte4 >> 7;
		int x_flip = (byte4 >> 6) & 1;
		int paletteNum = (byte4 >> 1) & 0b111;
		//priority = (byte4 >> 4) & 0b11;
		//int spriteSize = (attr >> 1) & 0x01;

		if (attr & 0x01) x_pos -= 256;

		const int spriteDimX = spriteDimensions[spriteDim][(attr>>1) & 1][0];
		const int spriteDimY = spriteDimensions[spriteDim][(attr>>1) & 1][1];

		const int numCols = 16;
		unsigned char palArr[3 * numCols];

		int colidx = 256+( paletteNum * 16 * 2);
		for (int col = 0;col < numCols;col++)
		{
			unsigned int palcol = (((int)(cgram[colidx + 1] & 0x7f)) << 8) | cgram[colidx];
			int red = palcol & 0x1f; red <<= 3;
			int green = (palcol >> 5) & 0x1f; green <<= 3;
			int blue = (palcol >> 10) & 0x1f; blue <<= 3;

			palArr[(col * 3) + 0] = (unsigned char)red;
			palArr[(col * 3) + 1] = (unsigned char)green;
			palArr[(col * 3) + 2] = (unsigned char)blue;

			colidx += 2;
		}

		//const unsigned int OAMBase = 0xc000 / 2;
		const unsigned int OAMBase = (obSel & 0x03) * 0x2000;

		int ybase = 0; int yend = spriteDimY; int yinc = 1; 
		if (y_flip)
		{
			ybase = spriteDimY-1; yend = -1; yinc = -1;
		}

		for (int y = ybase;y != yend;y+=yinc)
		{
			int realy = y;
			for (int x = 0;x != spriteDimX; x+=1)
			{
				const unsigned char shift_x = 7 - (x % 8);
				const unsigned int tile_address = OAMBase + (tile_nr + x / 8) * 0x10 + (y % 8) + (y / 8 * 0x100);
				const unsigned char b_1 = vram[tile_address] & 0xff;
				const unsigned char b_2 = vram[tile_address] >> 8;
				const unsigned char b_3 = vram[tile_address + 8] & 0xff;
				const unsigned char b_4 = vram[tile_address + 8] >> 8;
				const unsigned short int pixPalIdx = ((b_1 >> shift_x) & 1) +
					(2 * ((b_2 >> shift_x) & 1)) +
					(4 * ((b_3 >> shift_x) & 1)) +
					(8 * ((b_4 >> shift_x) & 1));

				if (pixPalIdx != 0)
				{
					int realx = x;
					if (x_flip) realx = spriteDimX - x - 1;
					if (y_flip) realy = spriteDimY - y - 1;
					unsigned int pixaddr = ((x_pos + realx) + ((y_pos + realy) * ppuResolutionX)) * 4;
					if (pixaddr < (ppuResolutionX * ppuResolutionY * 4))
					{
						unsigned char* pBuf = &screenFramebuffer[pixaddr];
						
						if (((x_pos + realx) >=0 ) && ((x_pos + realx) < 256))
						{
							*pBuf = palArr[(pixPalIdx * 3) + 0]; pBuf++;
							*pBuf = palArr[(pixPalIdx * 3) + 1]; pBuf++;
							*pBuf = palArr[(pixPalIdx * 3) + 2]; pBuf++;
							*pBuf = 0xff;
						}
						else
						{
							pBuf += 4;
						}
					}
				}


			}
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
	int screenMode = (bgMode & 0x07);

	int bgTileSize[4];
	bgTileSize[0] = (bgMode >> 4)&0x01;
	bgTileSize[1] = (bgMode >> 5)&0x01;
	bgTileSize[2] = (bgMode >> 6)&0x01;
	bgTileSize[3] = (bgMode >> 7)&0x01;

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
		if ((((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) || (((subScreenDesignation & 0x1f) & (1 << 1)) > 0)) renderBG(1, 4);
		if (((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) renderBG(0, 4);
		if (((mainScreenDesignation & 0x1f) & (1 << 2)) > 0) renderBG(2, 2);
	}
	else if (screenMode == 0x02)
	{
		// 1      16-color    16-color    opt     -         ;Normal
		renderBackdrop();
		if ((((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) || (((subScreenDesignation & 0x1f) & (1 << 1)) > 0)) renderBG(1, 4);
		if (((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) renderBG(0, 4);
	}
	else if (screenMode == 0x03)
	{
		// 3      256-color   16-color    -           -         ;Normal   
		renderBackdrop();
		if (((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) renderBG(1, 4);
		if (((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) renderBG(0, 8);
	}

	// now OAM Sprites
	renderSprites();

	// final pass, master brightness!
	int brightness = iniDisp & 0x0f;
	if ((iniDisp & 0x80) == 0x80) brightness = 0;

	if (brightness != 15)
	{
		for (unsigned int pos = 0;pos < (ppuResolutionX * ppuResolutionY * 4);pos += 4)
		{
			int val;
			val = screenFramebuffer[pos + 0];
			val *= brightness;
			val >>= 4;
			screenFramebuffer[pos + 0] = (unsigned char)val;

			val = screenFramebuffer[pos + 1];
			val *= brightness;
			val >>= 4;
			screenFramebuffer[pos + 1] = (unsigned char)val;

			val = screenFramebuffer[pos + 2];
			val *= brightness;
			val >>= 4;
			screenFramebuffer[pos + 2] = (unsigned char)val;
		}
	}
}

void ppu::step(int numCycles, mmu& theMMU, cpu5a22& theCPU)
{
	internalCyclesCounter += numCycles;

	if (internalCyclesCounter >= cyclesPerScanline)
	{
		scanline += 1;
		internalCyclesCounter %= cyclesPerScanline;

		// NMI
		if (scanline == vblankStartScanline)
		{
			theMMU.setNMIFlag();
			// trigger nmi if not blocked
			if (theMMU.isVblankNMIEnabled())
			{
				theCPU.triggerNMI();
			}
		}

		if (scanline >= totScanlines)
		{
			scanline = 0;
			theMMU.clearNMIFlag();
		}
	}
}

ppu::~ppu()
{
	delete(screenFramebuffer);
	delete(vramViewerBitmap);
}
