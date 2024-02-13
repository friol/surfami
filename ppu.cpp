
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
	for (unsigned int pos = 0;pos < (ppuResolutionX * ppuResolutionY * 4);pos++)
	{
		screenFramebuffer[pos] = 0;
	}

	vramViewerBitmap = new unsigned char[vramViewerXsize * vramViewerYsize * 4];
	for (int pos = 0;pos < (vramViewerXsize * vramViewerYsize * 4);pos++)
	{
		vramViewerBitmap[pos] = 0;
	}
}

void ppu::writeM7HOFS(unsigned char val)
{
	m7matrix[6] = ((val << 8) | m7prev) & 0x1fff;
	m7prev = val;
}

void ppu::writeM7VOFS(unsigned char val)
{
	m7matrix[7] = ((val << 8) | m7prev) & 0x1fff;
	m7prev = val;
}

void ppu::writeM7Matrix(int mtxparm, unsigned char val)
{
	if (mtxparm >= 8)
	{
		int err = 1;
	}

	if (mtxparm < 4)
	{
		m7matrix[mtxparm] = (signed short int)((val << 8) | m7prev);
	}
	else
	{
		m7matrix[mtxparm] = ((val << 8) | m7prev) & 0x1fff;
	}

	m7prev = val;
}

void ppu::writeM7SEL(unsigned char val)
{
	/*
		211Ah - M7SEL - Rotation/Scaling Mode Settings (W)
		7-6   Screen Over (see below)
		5-2   Not used
		1     Screen V-Flip (0=Normal, 1=Flipped)     ;\flip 256x256 "screen"
		0     Screen H-Flip (0=Normal, 1=Flipped)     ;/

		Screen Over (when exceeding the 128x128 tile BG Map size):
		0=Wrap within 128x128 tile area
		1=Wrap within 128x128 tile area (same as 0)
		2=Outside 128x128 tile area is Transparent
		3=Outside 128x128 tile area is filled by Tile 00h
	*/

	m7largeField = val & 0x80;
	m7charFill = val & 0x40;
	m7yFlip = val & 0x2;
	m7xFlip = val & 0x1;
}

void ppu::writeSubscreenFixedColor(unsigned char val)
{
	bool r = ((val >> 5) & 1) == 1;
	bool g = ((val >> 6) & 1) == 1;
	bool b = ((val >> 7) & 1) == 1;
	unsigned char intensity = val & 0b11111;
	if (r) coldataColor = (coldataColor & 0b11111'11111'00000'1) | (intensity << 1);
	if (g) coldataColor = (coldataColor & 0b11111'00000'11111'1) | (intensity << 6);
	if (b) coldataColor = (coldataColor & 0b00000'11111'11111'1) | (intensity << 11);
}

void ppu::writeBgScrollX(int bgId,unsigned char val)
{
	if (bgId >= 4)
	{
		int err = 1;
	}

	bgScrollX[bgId] = (val << 8) | (BGSCROLL_L1 & ~7) | ((bgScrollX[bgId] >> 8) & 7);
	BGSCROLL_L1 = val;
}

void ppu::writeBgScrollY(int bgId, unsigned char val)
{
	bgScrollY[bgId] = (val << 8) | BGSCROLL_L1;
	BGSCROLL_L1 = val;
}

void ppu::writeOAM(unsigned char val)
{
	unsigned char latch_bit = OAMAddr & 1;
	unsigned short int address = OAMAddr;
	OAMAddr = (OAMAddr + 1) & 0x03ff;
	
	if (latch_bit == 0)
	{
		OAM_Lsb = val;
	}
	if (address & 0x0200)
	{
		if (address >= 0x220)
		{
			// TODO: why does this happen?
			int err = 1;
			return;
		}

		OAM[address] = val;
	}
	else if (latch_bit == 1)
	{
		if ((address & ~1) >= 0x220)
		{
			int err = 1;
		}

		OAM[(address & ~1) + 1] = val;
		OAM[(address & ~1)] = OAM_Lsb;
	}
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
	{ 
		//	PPU - Apply address translation if necessary (leftshift thrice lower 8, 9 or 10 bits)
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
		cgramIdx &= 0x1ff;
		if (cgramIdx & 0x01) val &= 0x7f;
		cgram[cgramIdx] = val;
		cgramIdx += 1;
		cgramIdx &= 0x1ff;
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

		unsigned short int vramAddr = (vramAddressLower | (vramAddressUpper << 8));
		unsigned short int writeAddr = vramAddr;
		unsigned char _v_hi_lo = vmainVRAMAddrIncrMode >> 7;
		unsigned char _v_trans = (vmainVRAMAddrIncrMode & 0b1100) >> 2;

		if (_v_trans != 0)
		{
			int err = 1;
			err = 2;
		}

		switch (_v_trans) 
		{		
			//	PPU - Apply address translation if necessary (leftshift thrice lower 8, 9 or 10 bits)
			case 0b00:
				break;
			case 0b01: {		//	8 bit, aaaaaaaYYYxxxxx becomes aaaaaaaxxxxxYYY
				unsigned short int _t_st = (vramAddr & 0b1111111100000000);
				unsigned short int _t_off = (vramAddr & 0b11100000) >> 5;
				unsigned short int _t_in = (vramAddr & 0b11111) << 3;
				writeAddr = _t_st | _t_off | _t_in;
				break;
			}
			case 0b10: {		//	9 bit, aaaaaaYYYxxxxxP becomes aaaaaaxxxxxPYYY
				unsigned short int _t_st = (vramAddr & 0b1111111000000000);
				unsigned short int _t_off = (vramAddr & 0b111000000) >> 6;
				unsigned short int _t_in = (vramAddr & 0b111111) << 3;
				writeAddr = _t_st | _t_off | _t_in;
				break;
			}
			case 0b11: {		//	10 bit, aaaaaYYYxxxxxPP becomes aaaaaxxxxxPPYYY
				unsigned short int _t_st = (vramAddr & 0b1111110000000000);
				unsigned short int _t_off = (vramAddr & 0b1110000000) >> 7;
				unsigned short int _t_in = (vramAddr & 0b1111111) << 3;
				writeAddr = _t_st | _t_off | _t_in;
				break;
			}
		}

		unsigned short int adrIncrStep = 1;
		unsigned int addrIncrementStep = vmainVRAMAddrIncrMode & 0x03;
		if (addrIncrementStep != 0)
		{
			// 1 - 0   Address Increment Step(0..3 = Increment Word - Address by 1, 32, 128, 128)
			const unsigned short int increments[] = {1, 32, 128, 128};
			adrIncrStep = increments[addrIncrementStep];
		}

		if ((reg == 0x2118 && !_v_hi_lo) || (reg == 0x2119 && _v_hi_lo))
		{
			unsigned short int nextAddr = vramAddr;
			nextAddr += adrIncrStep;
			//nextAddr &= 0xffff;
			vramAddressLower = nextAddr & 0xff;
			vramAddressUpper = (nextAddr >> 8) & 0xff;
		}

		if (vramAddr == 0x1500)
		{
			writeBreakpoint = true;
		}

		if (reg == 0x2118)
		{
			vram[writeAddr & 0x7fff] = (vram[writeAddr & 0x7fff] & 0xff00) | val;
		}
		else 
		{
			vram[writeAddr & 0x7fff] = (vram[writeAddr & 0x7fff] & 0xff) | (val << 8);
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

unsigned char ppu::getMPY(unsigned int addr)
{
	int result = m7matrix[0] * (m7matrix[1] >> 8);
	unsigned char openbus = (result >> (8 * (addr - 0x2134))) & 0xff;
	return openbus;
}

std::vector<std::string> ppu::getM7Matrix()
{
	std::vector<std::string> m7vec;

	for (int i = 0;i < 4;i++)
	{
		m7vec.push_back("m7matrix["+std::to_string(i)+"] "+std::to_string(m7matrix[i]));
	}

	return m7vec;
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
	int tileAddr = 0x0000;
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

void ppu::renderBackdropScanline(int scanlinenum)
{
	//if (scanlinenum >= ppuResolutionY) return;

	unsigned int backdropColor = (((int)(cgram[1] & 0x7f)) << 8) | cgram[0];
	unsigned char red = backdropColor & 0x1f; red <<= 3;
	unsigned char green = (backdropColor >> 5) & 0x1f; green <<= 3;
	unsigned char blue = (backdropColor >> 10) & 0x1f; blue <<= 3;

	unsigned int pos = ppuResolutionX * scanlinenum * 4;

	for (unsigned int x=0;x<ppuResolutionX;x++)
	{
		screenFramebuffer[pos] = red;
		screenFramebuffer[pos + 1] = green;
		screenFramebuffer[pos + 2] = blue;
		screenFramebuffer[pos + 3] = 0xff;
		pos += 4;
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

void ppu::renderTileScanline(int bpp, int px, int py, int tileNum, int palId, int bgnum, int xflip, int yflip,int scanlinenum,unsigned char bgpri)
{
	unsigned char* pBuf;

	// palette setup

	const int numCols = (bpp == 2) ? 4 : (bpp == 4) ? 16 : 256;
	unsigned char* palArr = new unsigned char[3 * numCols];

	int colidx;
	if (bpp == 2)
	{
		if ((bgMode & 0x07) == 0)
		{
			colidx = ((palId * 4) * 2) + (0x40 * bgnum);
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

	int scanlineMod8 = (py+scanlinenum) % 8;

	int ybase = 0; int yend = 8; int yinc = 1; 
	if (yflip)
	{
		ybase = 7; yend = -1; yinc = -1; 
		tileAddr += (7 - scanlineMod8);
	}
	else
	{
		tileAddr += scanlineMod8;
	}

	unsigned int ppos = (px * 4) + (scanlinenum * ppuResolutionX * 4);
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

				if ((theX >= 0) && (theX < 256))
				{
					unsigned char* pBgColorAppo = &bgColorAppo[bgnum][theX * 4];
					unsigned char* pBgPriAppo = &bgPriorityAppo[bgnum][theX];
					bool* pBgIsTranspAppo = &bgIsTransparentAppo[bgnum][theX];

					if (bpp == 2)
					{
						if ((((curCol % 4) != 0) && ((theX >= 0) && (theX < (signed int)ppuResolutionX))))
						{
							//*pBuf = palArr[(curCol * 3) + 0]; pBuf++;
							//*pBuf = palArr[(curCol * 3) + 1]; pBuf++;
							//*pBuf = palArr[(curCol * 3) + 2]; pBuf++;
							//*pBuf = 0xff; pBuf++;

							*pBgColorAppo = palArr[(curCol * 3) + 0]; pBgColorAppo++;
							*pBgColorAppo = palArr[(curCol * 3) + 1]; pBgColorAppo++;
							*pBgColorAppo = palArr[(curCol * 3) + 2]; pBgColorAppo++;
							*pBgColorAppo = 0xff; pBgColorAppo++;
							*pBgPriAppo = bgpri; pBgPriAppo++;
							*pBgIsTranspAppo = false; pBgIsTranspAppo++;
						}
						else
						{
							//pBuf += 4;
							pBgColorAppo += 4;
							*pBgPriAppo = bgpri; pBgPriAppo++;
							*pBgIsTranspAppo = true; pBgIsTranspAppo++;
						}
					}
					else if (bpp == 4)
					{
						if ((((curCol % 16) != 0) && ((theX >= 0) && (theX < (signed int)ppuResolutionX))))
						{
							//*pBuf = palArr[(curCol * 3) + 0]; pBuf++;
							//*pBuf = palArr[(curCol * 3) + 1]; pBuf++;
							//*pBuf = palArr[(curCol * 3) + 2]; pBuf++;
							//*pBuf = 0xff; pBuf++;

							*pBgColorAppo = palArr[(curCol * 3) + 0]; pBgColorAppo++;
							*pBgColorAppo = palArr[(curCol * 3) + 1]; pBgColorAppo++;
							*pBgColorAppo = palArr[(curCol * 3) + 2]; pBgColorAppo++;
							*pBgColorAppo = 0xff; pBgColorAppo++;
							*pBgPriAppo = bgpri; pBgPriAppo++;
							*pBgIsTranspAppo = false; pBgIsTranspAppo++;
						}
						else
						{
							//pBuf += 4;
							pBgColorAppo += 4;
							*pBgPriAppo = bgpri; pBgPriAppo++;
							*pBgIsTranspAppo = true; pBgIsTranspAppo++;
						}
					}
					else
					{
						//*pBuf = palArr[(curCol * 3) + 0]; pBuf++;
						//*pBuf = palArr[(curCol * 3) + 1]; pBuf++;
						//*pBuf = palArr[(curCol * 3) + 2]; pBuf++;
						//*pBuf = 0xff; pBuf++;

						*pBgColorAppo = palArr[(curCol * 3) + 0]; pBgColorAppo++;
						*pBgColorAppo = palArr[(curCol * 3) + 1]; pBgColorAppo++;
						*pBgColorAppo = palArr[(curCol * 3) + 2]; pBgColorAppo++;
						*pBgColorAppo = 0xff; pBgColorAppo++;
						*pBgPriAppo = bgpri; pBgPriAppo++;
						*pBgIsTranspAppo = false; pBgIsTranspAppo++;
					}
				}
			}

			ppos += 4;
			theX += 1;
		}

		tileAddr += 1;
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
			tilemapMap[y][x] = vram[tilemapPos&0x7fff];
			tilemapPos++;
		}
	}

	if ((bgSize == 0) || (bgSize == 2)) tilemapPos -= 0x400;

	for (int y = 0;y < 32;y++)
	{
		for (int x = 32;x < 64;x++)
		{
			tilemapMap[y][x] = vram[tilemapPos & 0x7fff];
			tilemapPos++;
		}
	}

	if (bgSize == 0) tilemapPos -= 0x400;

	for (int y = 32;y < 64;y++)
	{
		for (int x = 0;x < 32;x++)
		{
			tilemapMap[y][x] = vram[tilemapPos & 0x7fff];
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
			tilemapMap[y][x] = vram[tilemapPos & 0x7fff];
			tilemapPos++;
		}
	}
}

void ppu::calcOffsetPerTileScroll(unsigned short int bg3word, unsigned short int bg3word2, int bgnum, int& xscroll, int& yscroll)
{
	/*
	15  bit  8   7  bit  0
		---- ----   ---- ----
		V21. ..SS   SSSS Ssss
		|||    ||   |||| ||||
		|||    ++---++++-++++- New scroll value for this tile. For horizontal values, the bottom three bits are ignored
		||+------------------- Override scroll value for layer 1
		|+-------------------- Override scroll value for layer 2
		+--------------------- Mode 4 only: Scroll direction (0 = horizontal, 1 = vertical)
	*/

	int screenMode = (bgMode & 0x07);

	if (screenMode == 2)
	{
		if ((bg3word & 0x4000) && (bgnum == 1))
		{
			xscroll = (xscroll & 0x07) | (bg3word & 0x3f8);
			yscroll = bg3word2 & 0x3ff;
		}
		else if ((bg3word & 0x2000) && (bgnum == 0))
		{
			xscroll = (xscroll & 0x07) | (bg3word & 0x3f8);
			yscroll = bg3word2 & 0x3ff;
		}
	}
	else if (screenMode == 4)
	{
		if ((bg3word & 0x4000) && (bgnum == 1))
		{
			int scrollDir = bg3word & 0x8000;
			if (scrollDir == 0)
			{
				// horz
				xscroll = (xscroll & 0x07) | (bg3word & 0x3f8);
			}
			else
			{
				// vert
				yscroll = bg3word & 0x3ff;
			}
		}
		else if ((bg3word & 0x2000) && (bgnum == 0))
		{
			int scrollDir = bg3word & 0x8000;
			if (scrollDir == 0)
			{
				// horz
				xscroll = (xscroll & 0x07) | (bg3word & 0x3f8);
			}
			else
			{
				// vert
				yscroll = bg3word & 0x3ff;
			}
		}
	}
}

void ppu::renderBGScanline(int bgnum, int bpp, int scanlinenum)
{
	int baseTileAddr = ((bgTileMapBaseAddress[bgnum] >> 2) << 10) & 0x7fff;

	int bgSize = bgTileMapBaseAddress[bgnum] & 0x3;
	int xscroll = bgScrollX[bgnum] & 0x3ff;
	int yscroll = bgScrollY[bgnum] & 0x3ff;

	unsigned short int tilemapMap[64][64];
	buildTilemapMap(tilemapMap, bgSize, baseTileAddr);

	/* 8x8 or 16x16 tiles for modes 0-4 */
	int bgTileSize = (bgMode & (0x10 << bgnum));

	int tileDim = 8;
	int tileDimX = 33;
	int tileDimY = 29;

	if (bgTileSize > 0)
	{
		tileDim = 16;
		tileDimX = 17;
		tileDimY = 15;
	}

	for (int y = 0;y < tileDimY;y+=1)
	{
		if (((y * tileDim) + ((scanlinenum) % tileDim)) == scanlinenum)
		{
			for (int x = 0;x < tileDimX;x+= 1)
			{
				if (x != 0)
				{
					// opt
					int screenMode = bgMode & 0x07;

					if ((screenMode == 2) || (screenMode == 4))
					{
						int bg3baseTileAddr = ((bgTileMapBaseAddress[2] >> 2) << 10) & 0x7fff;

						int bg3xscroll = bgScrollX[2] & 0x3ff;
						int bg3realx = x - 1 + (bg3xscroll / 8);
						//int bg3yscroll = bgScrollY[2] & 0x3ff;

						unsigned short int bg3word = 0, bg3word2 = 0;
						if ((bgMode & 0x07) == 2)
						{
							bg3word = vram[bg3baseTileAddr + (bg3realx & 0x3f)];
							bg3word2 = vram[bg3baseTileAddr + (bg3realx & 0x3f) + 32];
						}
						else if ((bgMode & 0x07) == 4)
						{
							bg3word = vram[bg3baseTileAddr + (bg3realx & 0x3f)];
						}

						calcOffsetPerTileScroll(bg3word, bg3word2, bgnum, xscroll, yscroll);
					}
				}

				int realx = x + (xscroll / tileDim);
				int realy = y + (yscroll / tileDim);

				if (((yscroll % tileDim) + (scanlinenum % tileDim)) > (tileDim-1))
				{
					realy += 1;
				}

				int anderx = 0x3f; int andery = 0x3f;
				if ((bgSize == 0) || (bgSize == 1)) andery = 0x1f;
				if ((bgSize == 0) || (bgSize == 2)) anderx = 0x1f;

				unsigned short int vramWord = tilemapMap[realy & andery][realx & anderx];
				int tileNum = vramWord & 0x3ff;
				int palId = (vramWord >> 10) & 0x7;
				unsigned char bgPri = (vramWord >> 13) & 0x01;
				int xflip = (vramWord >> 14) & 0x01;
				int yflip = (vramWord >> 15) & 0x01;

				if (tileDim == 8)
				{
					renderTileScanline(bpp, (x * 8) - (xscroll % tileDim), yscroll, tileNum, palId, bgnum, xflip, yflip, scanlinenum, bgPri);
				}
				else
				{
					if (((yscroll+scanlinenum) % tileDim) < 8)
					{
						renderTileScanline(bpp, (x * 16) - (xscroll % tileDim), yscroll, tileNum, palId, bgnum, xflip, yflip, scanlinenum, bgPri);
						renderTileScanline(bpp, ((x * 16) + 8) - (xscroll % tileDim), yscroll, (tileNum + 1) & 0x3ff, palId, bgnum, xflip, yflip, scanlinenum, bgPri);
					}
					else
					{
						renderTileScanline(bpp, (x * 16) - (xscroll % tileDim), yscroll, (tileNum+16) & 0x3ff, palId, bgnum, xflip, yflip, scanlinenum, bgPri);
						renderTileScanline(bpp, ((x * 16) + 8) - (xscroll % tileDim), yscroll, (tileNum + 17) & 0x3ff, palId, bgnum, xflip, yflip, scanlinenum, bgPri);
					}
				}
			}
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
		/*if (y_flip)
		{
			ybase = spriteDimY-1; yend = -1; yinc = -1;
		}*/

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
					if ((pixaddr>=0) && (pixaddr < (ppuResolutionX * ppuResolutionY * 4)))
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

void ppu::renderSpritesScanline(int scanlinenum)
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

	int spriteDim = (obSel >> 5) & 0x7;

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
		unsigned char priority = (byte4 >> 4) & 0b11;

		if (attr & 0x01) x_pos -= 256;

		const int spriteDimX = spriteDimensions[spriteDim][(attr >> 1) & 1][0];
		const int spriteDimY = spriteDimensions[spriteDim][(attr >> 1) & 1][1];

		const int numCols = 16;
		unsigned char palArr[3 * numCols];

		int colidx = 256 + (paletteNum * 16 * 2);
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

		const unsigned int OAMBase = (obSel & 0x03) * 0x2000;

		int ybase = 0; int yend = spriteDimY; int yinc = 1;

		for (int y = ybase;y != yend;y += yinc)
		{
			int realy = y;
			if (y_flip) realy = spriteDimY - y - 1;

			if (((y_pos + realy)&0xff) == scanlinenum)
			{
				for (int x = 0;x != spriteDimX; x += 1)
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

					int realx = x;
					if (x_flip) realx = spriteDimX - x - 1;
					int theEx = (x_pos + realx);
					//if (theEx >= 256) theEx %= 256;
					unsigned int pixaddr = (theEx + (scanlinenum * ppuResolutionX)) * 4;
					if ((pixaddr >= 0) && (pixaddr < (ppuResolutionX * ppuResolutionY * 4)))
					{
						if ( ((theEx >= 0) && (theEx < 256)) && (pixPalIdx != 0) )
						{
							unsigned char* pObjColorAppo = &objColorAppo[theEx * 4];
							unsigned char* pObjPriAppo = &objPriorityAppo[theEx];
							bool* pObjTranspAppo = &objIsTransparentAppo[theEx];

							*pObjColorAppo = palArr[(pixPalIdx * 3) + 0]; pObjColorAppo++;
							*pObjColorAppo = palArr[(pixPalIdx * 3) + 1]; pObjColorAppo++;
							*pObjColorAppo = palArr[(pixPalIdx * 3) + 2]; pObjColorAppo++;
							*pObjColorAppo = 0xff; pObjColorAppo++;
							*pObjPriAppo = priority; 
							*pObjTranspAppo = false; 
						}
						else
						{
							if ((theEx >= 0) && (theEx < 256))
							{
								unsigned char* pObjPriAppo = &objPriorityAppo[theEx];
								*pObjPriAppo = priority; 
							}
						}
					}
				}
			}
		}
	}
}

void ppu::resetAppoBuffers()
{
	for (int x = 0;x < 256;x++)
	{
		bgPriorityAppo[0][x] = 0; bgPriorityAppo[1][x] = 0; bgPriorityAppo[2][x] = 0; bgPriorityAppo[3][x] = 0;
		bgIsTransparentAppo[0][x] = true; bgIsTransparentAppo[1][x] = true; bgIsTransparentAppo[2][x] = true; bgIsTransparentAppo[3][x] = true;
		bgColorAppo[0][(x * 4) + 0] = 0; bgColorAppo[0][(x * 4) + 1] = 0; bgColorAppo[0][(x * 4) + 2] = 0; bgColorAppo[0][(x * 4) + 3] = 0;
		bgColorAppo[1][(x * 4) + 0] = 0; bgColorAppo[1][(x * 4) + 1] = 0; bgColorAppo[1][(x * 4) + 2] = 0; bgColorAppo[1][(x * 4) + 3] = 0;
		bgColorAppo[2][(x * 4) + 0] = 0; bgColorAppo[2][(x * 4) + 1] = 0; bgColorAppo[2][(x * 4) + 2] = 0; bgColorAppo[2][(x * 4) + 3] = 0;
		bgColorAppo[3][(x * 4) + 0] = 0; bgColorAppo[3][(x * 4) + 1] = 0; bgColorAppo[3][(x * 4) + 2] = 0; bgColorAppo[3][(x * 4) + 3] = 0;

		objColorAppo[(x * 4) + 0] = 0; objColorAppo[(x * 4) + 1] = 0; objColorAppo[(x * 4) + 2] = 0; objColorAppo[(x * 4) + 3] = 0;
		objPriorityAppo[x] = 0; 
		objIsTransparentAppo[x] = true; 
	}
}

/* this fantastic code comes from https://github.com/angelo-wf/LakeSnes. Probably would have taken ages to write it by myself */

void ppu::calculateMode7Starts(int y)
{
	// expand 13-bit values to signed values
	int hScroll = ((signed short int)(m7matrix[6] << 3)) >> 3;
	int vScroll = ((signed short int)(m7matrix[7] << 3)) >> 3;
	int xCenter = ((signed short int)(m7matrix[4] << 3)) >> 3;
	int yCenter = ((signed short int)(m7matrix[5] << 3)) >> 3;

	int clippedH = hScroll - xCenter;
	int clippedV = vScroll - yCenter;
	clippedH = (clippedH & 0x2000) ? (clippedH | ~1023) : (clippedH & 1023);
	clippedV = (clippedV & 0x2000) ? (clippedV | ~1023) : (clippedV & 1023);

	unsigned char ry = (unsigned char)(m7yFlip ? 255 - y : y);
	m7startX = (
		((m7matrix[0] * clippedH) & ~63) +
		((m7matrix[1] * ry) & ~63) +
		((m7matrix[1] * clippedV) & ~63) +
		(xCenter << 8)
		);
	m7startY = (
		((m7matrix[2] * clippedH) & ~63) +
		((m7matrix[3] * ry) & ~63) +
		((m7matrix[3] * clippedV) & ~63) +
		(yCenter << 8)
		);
}

unsigned char ppu::getPixelForMode7(int x, int layer, bool priority) 
{
	priority++;
	layer++;

	unsigned char rx = (unsigned char)(m7xFlip ? 255 - x : x);
	int xPos = (m7startX + m7matrix[0] * rx) >> 8;
	int yPos = (m7startY + m7matrix[2] * rx) >> 8;
	bool outsideMap = xPos < 0 || xPos >= 1024 || yPos < 0 || yPos >= 1024;
	xPos &= 0x3ff;
	yPos &= 0x3ff;
	if (!m7largeField) outsideMap = false;
	unsigned char tile = outsideMap ? 0 : vram[(yPos >> 3) * 128 + (xPos >> 3)] & 0xff;
	unsigned char pixel = outsideMap && !m7charFill ? 0 : vram[tile * 64 + (yPos & 7) * 8 + (xPos & 7)] >> 8;
	/*if (layer == 1) {
		if (((bool)(pixel & 0x80)) != priority) return 0;
		return pixel & 0x7f;
	}*/
	return pixel;
}

void ppu::renderMode7Scanline(int scanlinenum)
{
	unsigned char* palArr = new unsigned char[3 * 256];
	unsigned int colidx = 0;

	for (int col = 0;col < 256;col++)
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

	calculateMode7Starts(scanlinenum);

	unsigned char* pBuf = &screenFramebuffer[(scanlinenum * ppuResolutionX * 4)];
	for (int x = 0;x < 256;x++)
	{
		unsigned char colNum = getPixelForMode7(x, 0, false);
		*pBuf = palArr[(colNum * 3) + 0]; pBuf++;
		*pBuf = palArr[(colNum * 3) + 1]; pBuf++;
		*pBuf = palArr[(colNum * 3) + 2]; pBuf++;
		*pBuf = 0xff; pBuf++;
	}

	/*int tiley = scanlinenum / 8;

	for (int x = 0;x < 32;x++)
	{
		unsigned int tileaddr = x + (tiley * 128);
		unsigned int tileNum = vram[tileaddr] & 0xff;

		unsigned int tiledataAddr = tileNum * 64;
		tiledataAddr += 8 * (scanlinenum % 8);

		unsigned char* pBuf = &screenFramebuffer[(scanlinenum * ppuResolutionX * 4)+(x*8*4)];
		for (int bit = 0;bit < 8;bit++)
		{
			unsigned char colNum = vram[tiledataAddr + bit] >> 8;

			*pBuf = palArr[(colNum * 3) + 0]; pBuf++;
			*pBuf = palArr[(colNum * 3) + 1]; pBuf++;
			*pBuf = palArr[(colNum * 3) + 2]; pBuf++;
			*pBuf = 0xff; pBuf++;
		}
	}*/

	delete(palArr);
}

void ppu::renderScanline(int scanlinenum)
{
	if ((scanlinenum < 0) || (scanlinenum >= 224)) return;

	/*
		7-2  SC Base Address in VRAM (in 1K-word steps, aka 2K-byte steps)
		1-0  SC Size (0=One-Screen, 1=V-Mirror, 2=H-Mirror, 3=Four-Screen)
						(0=32x32, 1=64x32, 2=32x64, 3=64x64 tiles)
					(0: SC0 SC0    1: SC0 SC1  2: SC0 SC0  3: SC0 SC1   )
					(   SC0 SC0       SC0 SC1     SC1 SC1     SC2 SC3   )
		Specifies the BG Map addresses in VRAM. The "SCn" screens consists of 32x32 tiles each.
	*/

	if (screenDisabled)
	{
		// TODO
	}

	// rendering depends on screen mode
	int screenMode = (bgMode & 0x07);

	resetAppoBuffers();
	//screenMode = 7;

	if (screenMode == 0)
	{
		// 0      4-color     4-color     4-color     4-color   ;Normal   
		for (int bg = 3;bg >= 0;bg--)
		{
			//if (bg == 1)
			{
				if (((mainScreenDesignation & 0x1f) & (1 << bg)) > 0)
				{
					renderBGScanline(bg, 2, scanlinenum);
				}
			}
		}

		if (mainScreenDesignation & 0x10)
		{
			renderSpritesScanline(scanlinenum);
		}

		unsigned char* pfbuf = &screenFramebuffer[scanlinenum * ppuResolutionX * 4];
		for (int x = 0;x < 256;x++)
		{
			int finalCol = -1;

			if (objPriorityAppo[x] == 3 && (!objIsTransparentAppo[x])) finalCol = 4;
			else if ((bgPriorityAppo[0][x] == 1) && (!bgIsTransparentAppo[0][x])) finalCol = 0;
			else if ((bgPriorityAppo[1][x] == 1) && (!bgIsTransparentAppo[1][x])) finalCol = 1;
			else if (objPriorityAppo[x] == 2 && (!objIsTransparentAppo[x])) finalCol = 4;
			else if ((bgPriorityAppo[0][x] == 0) && (!bgIsTransparentAppo[0][x])) finalCol = 0;
			else if ((bgPriorityAppo[1][x] == 0) && (!bgIsTransparentAppo[1][x])) finalCol = 1;
			else if (objPriorityAppo[x] == 1 && (!objIsTransparentAppo[x])) finalCol = 4;
			else if ((bgPriorityAppo[2][x] == 1) && (!bgIsTransparentAppo[2][x])) finalCol = 2;
			else if ((bgPriorityAppo[3][x] == 1) && (!bgIsTransparentAppo[3][x])) finalCol = 3;
			else if (objPriorityAppo[x] == 0 && (!objIsTransparentAppo[x])) finalCol = 4;
			else if ((bgPriorityAppo[2][x] == 0) && (!bgIsTransparentAppo[2][x])) finalCol = 2;
			else if ((bgPriorityAppo[3][x] == 0) && (!bgIsTransparentAppo[3][x])) finalCol = 3;

			if (finalCol == -1)
			{
				unsigned int backdropColor = (((int)(cgram[1] & 0x7f)) << 8) | cgram[0];
				unsigned char red = backdropColor & 0x1f; red <<= 3;
				unsigned char green = (backdropColor >> 5) & 0x1f; green <<= 3;
				unsigned char blue = (backdropColor >> 10) & 0x1f; blue <<= 3;
				*pfbuf = red; pfbuf++;
				*pfbuf = green; pfbuf++;
				*pfbuf = blue; pfbuf++;
				*pfbuf = 0xff; pfbuf++;
			}
			else if (finalCol < 4)
			{
				*pfbuf = bgColorAppo[finalCol][(x * 4) + 0]; pfbuf++;
				*pfbuf = bgColorAppo[finalCol][(x * 4) + 1]; pfbuf++;
				*pfbuf = bgColorAppo[finalCol][(x * 4) + 2]; pfbuf++;
				*pfbuf = 0xff; pfbuf++;
			}
			else if (finalCol == 4)
			{
				*pfbuf = objColorAppo[(x * 4) + 0]; pfbuf++;
				*pfbuf = objColorAppo[(x * 4) + 1]; pfbuf++;
				*pfbuf = objColorAppo[(x * 4) + 2]; pfbuf++;
				*pfbuf = 0xff; pfbuf++;
			}
		}
	}
	else if (screenMode == 0x01)
	{
		// 1      16-color    16-color    4-color     -         ;Normal
		if ((((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) ) renderBGScanline(0, 4, scanlinenum);
		if ( (((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) || (((subScreenDesignation & 0x1f) & (1 << 1))) ) renderBGScanline(1, 4, scanlinenum);
		if ((((mainScreenDesignation & 0x1f) & (1 << 2)) > 0)) renderBGScanline(2, 2, scanlinenum);

		if (mainScreenDesignation & 0x10)
		{
			renderSpritesScanline(scanlinenum);
		}

		bool bg3_priority = (bgMode & 0x08) != 0;
		unsigned char* pfbuf = &screenFramebuffer[scanlinenum * ppuResolutionX * 4];
		for (int x = 0;x < 256;x++)
		{
			int finalCol=-1;

			if ((bgPriorityAppo[2][x] == 1) && bg3_priority && (!bgIsTransparentAppo[2][x])) finalCol = 2;
			else if (objPriorityAppo[x] == 3 && (!objIsTransparentAppo[x])) finalCol =4;
			else if ((bgPriorityAppo[0][x] == 1) && (!bgIsTransparentAppo[0][x])) finalCol=0;
			else if ((bgPriorityAppo[1][x] == 1) && (!bgIsTransparentAppo[1][x])) finalCol=1;
			else if (objPriorityAppo[x] == 2 && (!objIsTransparentAppo[x])) finalCol = 4;
			else if ((bgPriorityAppo[0][x] == 0) && (!bgIsTransparentAppo[0][x])) finalCol = 0;
			else if ((bgPriorityAppo[1][x] == 0) && (!bgIsTransparentAppo[1][x])) finalCol = 1;
			else if (objPriorityAppo[x] == 1 && (!objIsTransparentAppo[x])) finalCol = 4;
			else if ((bgPriorityAppo[2][x] == 1) && (!bg3_priority) && (!bgIsTransparentAppo[2][x])) finalCol = 2;
			else if (objPriorityAppo[x] == 0 && (!objIsTransparentAppo[x])) finalCol = 4;
			else if ((bgPriorityAppo[2][x] == 0) && (!bgIsTransparentAppo[2][x])) finalCol = 2;

			if (finalCol == -1)
			{
				unsigned int backdropColor;
				
				//if (((subScreenDesignation & 0x1f) & (1 << 1)))
				//{
				//	backdropColor = coldataColor;
				//}
				//else
				//{
					backdropColor= (((int)(cgram[1] & 0x7f)) << 8) | cgram[0];
				//}
				
				unsigned char red = backdropColor & 0x1f; red <<= 3;
				unsigned char green = (backdropColor >> 5) & 0x1f; green <<= 3;
				unsigned char blue = (backdropColor >> 10) & 0x1f; blue <<= 3;
				*pfbuf = red; pfbuf++;
				*pfbuf = green; pfbuf++;
				*pfbuf = blue; pfbuf++;
				*pfbuf = 0xff; pfbuf++;
			}
			else if (finalCol < 4)
			{
				*pfbuf = bgColorAppo[finalCol][(x * 4) + 0]; pfbuf++;
				*pfbuf = bgColorAppo[finalCol][(x * 4) + 1]; pfbuf++;
				*pfbuf = bgColorAppo[finalCol][(x * 4) + 2]; pfbuf++;
				*pfbuf = 0xff; pfbuf++;
			}
			else if (finalCol == 4)
			{
				*pfbuf = objColorAppo[(x * 4) + 0]; pfbuf++;
				*pfbuf = objColorAppo[(x * 4) + 1]; pfbuf++;
				*pfbuf = objColorAppo[(x * 4) + 2]; pfbuf++;
				*pfbuf = 0xff; pfbuf++;
			}
		}
	}
	else if (screenMode == 0x02)
	{
		// 1      16-color    16-color    opt     -         ;Normal
		if ((((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) || (((subScreenDesignation & 0x1f) & (1 << 1)) > 0)) renderBGScanline(1, 4, scanlinenum);
		if (((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) renderBGScanline(0, 4, scanlinenum);

		if (mainScreenDesignation & 0x10)
		{
			renderSpritesScanline(scanlinenum);
		}

		unsigned char* pfbuf = &screenFramebuffer[scanlinenum * ppuResolutionX * 4];
		for (int x = 0;x < 256;x++)
		{
			int finalCol = -1;

			if (objPriorityAppo[x] == 3 && (!objIsTransparentAppo[x])) finalCol = 4;
			else if ((bgPriorityAppo[0][x] == 1) && (!bgIsTransparentAppo[0][x])) finalCol = 0;
			else if (objPriorityAppo[x] == 2 && (!objIsTransparentAppo[x])) finalCol = 4;
			else if ((bgPriorityAppo[1][x] == 1) && (!bgIsTransparentAppo[1][x])) finalCol = 1;
			else if (objPriorityAppo[x] == 1 && (!objIsTransparentAppo[x])) finalCol = 4;
			else if ((bgPriorityAppo[0][x] == 0) && (!bgIsTransparentAppo[0][x])) finalCol = 0;
			else if (objPriorityAppo[x] == 0 && (!objIsTransparentAppo[x])) finalCol = 4;
			else if ((bgPriorityAppo[1][x] == 0) && (!bgIsTransparentAppo[1][x])) finalCol = 1;

			if (finalCol == -1)
			{
				//unsigned int backdropColor = (((int)(cgram[1] & 0x7f)) << 8) | cgram[0];
				unsigned int backdropColor = coldataColor;
				unsigned char red = backdropColor & 0x1f; red <<= 3;
				unsigned char green = (backdropColor >> 5) & 0x1f; green <<= 3;
				unsigned char blue = (backdropColor >> 10) & 0x1f; blue <<= 3;
				*pfbuf = red; pfbuf++;
				*pfbuf = green; pfbuf++;
				*pfbuf = blue; pfbuf++;
				*pfbuf = 0xff; pfbuf++;
			}
			else if (finalCol < 4)
			{
				*pfbuf = bgColorAppo[finalCol][(x * 4) + 0]; pfbuf++;
				*pfbuf = bgColorAppo[finalCol][(x * 4) + 1]; pfbuf++;
				*pfbuf = bgColorAppo[finalCol][(x * 4) + 2]; pfbuf++;
				*pfbuf = 0xff; pfbuf++;
			}
			else if (finalCol == 4)
			{
				*pfbuf = objColorAppo[(x * 4) + 0]; pfbuf++;
				*pfbuf = objColorAppo[(x * 4) + 1]; pfbuf++;
				*pfbuf = objColorAppo[(x * 4) + 2]; pfbuf++;
				*pfbuf = 0xff; pfbuf++;
			}
		}
	}
	else if (screenMode == 0x03)
	{
		// 3      256-color   16-color    -           -         ;Normal   
		if (((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) renderBGScanline(1, 4, scanlinenum);
		//if (((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) renderBGScanline(0, 8, scanlinenum);
		if ((((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) || (((subScreenDesignation & 0x1f) & (1 << 0)))) renderBGScanline(0, 8, scanlinenum);

		if (mainScreenDesignation & 0x10)
		{
			renderSpritesScanline(scanlinenum);
		}

		unsigned char* pfbuf = &screenFramebuffer[scanlinenum * ppuResolutionX * 4];
		for (int x = 0;x < 256;x++)
		{
			int finalCol = -1;

			if (objPriorityAppo[x] == 3 && (!objIsTransparentAppo[x])) finalCol = 4;
			else if ((bgPriorityAppo[0][x] == 1) && (!bgIsTransparentAppo[0][x])) finalCol = 0;
			else if (objPriorityAppo[x] == 2 && (!objIsTransparentAppo[x])) finalCol = 4;
			else if ((bgPriorityAppo[1][x] == 1) && (!bgIsTransparentAppo[1][x])) finalCol = 1;
			else if (objPriorityAppo[x] == 1 && (!objIsTransparentAppo[x])) finalCol = 4;
			else if ((bgPriorityAppo[0][x] == 0) && (!bgIsTransparentAppo[0][x])) finalCol = 0;
			else if (objPriorityAppo[x] == 0 && (!objIsTransparentAppo[x])) finalCol = 4;
			else if ((bgPriorityAppo[1][x] == 0) && (!bgIsTransparentAppo[1][x])) finalCol = 1;

			if (finalCol == -1)
			{
				unsigned int backdropColor = (((int)(cgram[1] & 0x7f)) << 8) | cgram[0];
				unsigned char red = backdropColor & 0x1f; red <<= 3;
				unsigned char green = (backdropColor >> 5) & 0x1f; green <<= 3;
				unsigned char blue = (backdropColor >> 10) & 0x1f; blue <<= 3;
				*pfbuf = red; pfbuf++;
				*pfbuf = green; pfbuf++;
				*pfbuf = blue; pfbuf++;
				*pfbuf = 0xff; pfbuf++;
			}
			else if (finalCol < 4)
			{
				*pfbuf = bgColorAppo[finalCol][(x * 4) + 0]; pfbuf++;
				*pfbuf = bgColorAppo[finalCol][(x * 4) + 1]; pfbuf++;
				*pfbuf = bgColorAppo[finalCol][(x * 4) + 2]; pfbuf++;
				*pfbuf = 0xff; pfbuf++;
			}
			else if (finalCol == 4)
			{
				*pfbuf = objColorAppo[(x * 4) + 0]; pfbuf++;
				*pfbuf = objColorAppo[(x * 4) + 1]; pfbuf++;
				*pfbuf = objColorAppo[(x * 4) + 2]; pfbuf++;
				*pfbuf = 0xff; pfbuf++;
			}
		}
	}
	else if (screenMode == 0x04)
	{
		if (((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) renderBGScanline(1, 2, scanlinenum);
		if (((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) renderBGScanline(0, 8, scanlinenum);

		if (mainScreenDesignation & 0x10)
		{
			renderSpritesScanline(scanlinenum);
		}

		unsigned char* pfbuf = &screenFramebuffer[scanlinenum * ppuResolutionX * 4];
		for (int x = 0;x < 256;x++)
		{
			int finalCol = -1;

			if (objPriorityAppo[x] == 3 && (!objIsTransparentAppo[x])) finalCol = 4;
			else if ((bgPriorityAppo[0][x] == 1) && (!bgIsTransparentAppo[0][x])) finalCol = 0;
			else if (objPriorityAppo[x] == 2 && (!objIsTransparentAppo[x])) finalCol = 4;
			else if ((bgPriorityAppo[1][x] == 1) && (!bgIsTransparentAppo[1][x])) finalCol = 1;
			else if (objPriorityAppo[x] == 1 && (!objIsTransparentAppo[x])) finalCol = 4;
			else if ((bgPriorityAppo[0][x] == 0) && (!bgIsTransparentAppo[0][x])) finalCol = 0;
			else if (objPriorityAppo[x] == 0 && (!objIsTransparentAppo[x])) finalCol = 4;
			else if ((bgPriorityAppo[1][x] == 0) && (!bgIsTransparentAppo[1][x])) finalCol = 1;

			if (finalCol == -1)
			{
				unsigned int backdropColor = (((int)(cgram[1] & 0x7f)) << 8) | cgram[0];
				unsigned char red = backdropColor & 0x1f; red <<= 3;
				unsigned char green = (backdropColor >> 5) & 0x1f; green <<= 3;
				unsigned char blue = (backdropColor >> 10) & 0x1f; blue <<= 3;
				*pfbuf = red; pfbuf++;
				*pfbuf = green; pfbuf++;
				*pfbuf = blue; pfbuf++;
				*pfbuf = 0xff; pfbuf++;
			}
			else if (finalCol < 4)
			{
				*pfbuf = bgColorAppo[finalCol][(x * 4) + 0]; pfbuf++;
				*pfbuf = bgColorAppo[finalCol][(x * 4) + 1]; pfbuf++;
				*pfbuf = bgColorAppo[finalCol][(x * 4) + 2]; pfbuf++;
				*pfbuf = 0xff; pfbuf++;
			}
			else if (finalCol == 4)
			{
				*pfbuf = objColorAppo[(x * 4) + 0]; pfbuf++;
				*pfbuf = objColorAppo[(x * 4) + 1]; pfbuf++;
				*pfbuf = objColorAppo[(x * 4) + 2]; pfbuf++;
				*pfbuf = 0xff; pfbuf++;
			}
		}
	}
	else if (screenMode == 0x07)
	{
		renderMode7Scanline(scanlinenum);

		if (mainScreenDesignation & 0x10)
		{
			renderSpritesScanline(scanlinenum);
		}

		unsigned char* pfbuf = &screenFramebuffer[scanlinenum * ppuResolutionX * 4];
		for (int x = 0;x < 256;x++)
		{
			if (!objIsTransparentAppo[x])
			{
				*pfbuf = objColorAppo[(x * 4) + 0]; pfbuf++;
				*pfbuf = objColorAppo[(x * 4) + 1]; pfbuf++;
				*pfbuf = objColorAppo[(x * 4) + 2]; pfbuf++;
				*pfbuf = 0xff; pfbuf++;
			}
			else pfbuf += 4;
		}
	}

	// final pass, master brightness!
	int brightness = iniDisp & 0x0f;
	if ((iniDisp & 0x80) == 0x80) brightness = 0;

	if (brightness != 15)
	{
		unsigned int pos = scanlinenum * ppuResolutionX * 4;
		for (unsigned int x=0;x<ppuResolutionX;x++)
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

			pos += 4;
		}
	}
}

void ppu::step(int numCycles, mmu& theMMU, cpu5a22& theCPU)
{
	internalCyclesCounter += numCycles;

	if ((internalCyclesCounter >= hdmaStartingPos)&&(hdmaStartedForThisLine==false))
	{
		hdmaStartedForThisLine = true;
		theMMU.executeHDMA();
	}

	if (internalCyclesCounter >= cyclesPerScanline)
	{
		if (scanline<ppuResolutionY) renderScanline(scanline);
		scanline += 1;
		hdmaStartedForThisLine = false;
		internalCyclesCounter %= cyclesPerScanline;

		// NMI
		if (scanline == vblankStartScanline)
		{
			//theMMU.resetHDMA();
			theMMU.setNMIFlag();
			// trigger nmi if not blocked
			if (theMMU.isVblankNMIEnabled())
			{
				theCPU.triggerNMI();
			}
			OAMAddr = OAMAddrSave;
		}

		// VIRQ
		if (theMMU.isVIRQEnabled() && (scanline==theMMU.getVIRQScanline()))
		{
			theCPU.triggerIRQ();
			theMMU.setIrqTriggered();
		}

		if (scanline >= totScanlines)
		{
			scanline = 0;
			theMMU.clearNMIFlag();
			theMMU.resetHDMA();
		}
	}
}

ppu::~ppu()
{
	delete(screenFramebuffer);
	delete(vramViewerBitmap);
}
