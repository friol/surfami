
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

	screenFramebufferHires = new unsigned char[ppuResolutionX*2 * ppuResolutionY*2 * 4];
	for (unsigned int pos = 0;pos < (ppuResolutionX*2 * ppuResolutionY*2 * 4);pos++)
	{
		screenFramebufferHires[pos] = 0;
	}

	vramViewerBitmap = new unsigned char[vramViewerXsize * vramViewerYsize * 4];
	for (int pos = 0;pos < (vramViewerXsize * vramViewerYsize * 4);pos++)
	{
		vramViewerBitmap[pos] = 0;
	}

	brightnessLookup = new unsigned char[256 * 16];
	for (unsigned int col = 0;col <256;col++)
	{
		for (unsigned int level = 0;level < 16;level++)
		{
			int val;
			val = col;
			val *= level;
			val >>= 4;
			brightnessLookup[(col * 16) + level] = (unsigned char)val;
		}
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
	/*unsigned char intensity = val & 0b11111;
	if (r) coldataColor = (coldataColor & 0b11111'11111'00000'1) | (intensity << 1);
	if (g) coldataColor = (coldataColor & 0b11111'00000'11111'1) | (intensity << 6);
	if (b) coldataColor = (coldataColor & 0b00000'11111'11111'1) | (intensity << 11);*/

	if (r) coldataColor[0] = (val & 0x1f)<<3;
	if (g) coldataColor[1] = (val & 0x1f)<<3;
	if (b) coldataColor[2] = (val & 0x1f)<<3;
}

void ppu::writeBgScrollX(int bgId,unsigned char val)
{
	bgScrollX[bgId] = (val << 8) | (BGSCROLL_L1 & ~7) | ((bgScrollX[bgId] >> 8) & 7);
	BGSCROLL_L1 = val;
}

void ppu::writeBgScrollY(int bgId, unsigned char val)
{
	bgScrollY[bgId] = (val << 8) | BGSCROLL_L1;
	BGSCROLL_L1 = val;
}

unsigned char ppu::cgramRead(unsigned short int offset)
{
	unsigned char res;
	offset &= 0x1ff;

	res = cgram[offset];

	if (offset & 0x01)
	{
		res &= 0x7f;
	}

	return res;
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
			return;
		}

		OAM[address] = val;
	}
	else if (latch_bit == 1)
	{
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

void ppu::setPalarrColor(int idx)
{
	unsigned int palcol = (((int)(cgram[idx + 1] & 0x7f)) << 8) | cgram[idx];
	int red = palcol & 0x1f; red <<= 3;
	int green = (palcol >> 5) & 0x1f; green <<= 3;
	int blue = (palcol >> 10) & 0x1f; blue <<= 3;

	idx >>= 1;
	palarrLookup[(idx * 3) + 0] = (unsigned char)red;
	palarrLookup[(idx * 3) + 1] = (unsigned char)green;
	palarrLookup[(idx * 3) + 2] = (unsigned char)blue;
}

void ppu::writeRegister(int reg, unsigned char val)
{
	if (reg == 0x2121)
	{
		// Register $2121 : Address for accessing CGRAM(1b / W)
		cgramIdx = ((int)val) * 2;
	}
	else if (reg == 0x2123)
	{
		w12sel = val;
	}
	else if (reg == 0x2124)
	{
		w34sel = val;
	}
	else if (reg == 0x2125)
	{
		wobjsel = val;
	}
	else if (reg == 0x2126)
	{
		windowxpos[0][0] = val;
	}
	else if (reg == 0x2127)
	{
		windowxpos[0][1] = val;
	}
	else if (reg == 0x2128)
	{
		windowxpos[1][0] = val;
	}
	else if (reg == 0x2129)
	{
		windowxpos[1][1] = val;
	}
	else if (reg == 0x212e)
	{
		windowTMW = val;
	}
	else if (reg == 0x212f)
	{
		windowTSW = val;
	}
	else if (reg == 0x2122)
	{
		// Register $2122: Data write to CGRAM (1b/W)
		cgramIdx &= 0x1ff;
		if (cgramIdx & 0x01) val &= 0x7f;
		cgram[cgramIdx] = val;

		setPalarrColor(cgramIdx&0x1fe);

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

		/*if (scanline <= vblankStartScanline)
		{
			bool noWrites = true;
			return;
		}*/

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

		if (vramAddr == 0x6060)
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

void ppu::tileViewerRenderTile4bpp(int px, int py, int tileAddr)
{
	unsigned char* pBuf;
	const int numCols = 16;
	unsigned char palArr[3 * numCols];

	int colidx = (0 * numCols) * 2;

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

	for (int y = 0;y < 8;y++)
	{
		pBuf = &vramViewerBitmap[(px * 4) + ((py + y) * vramViewerXsize * 4)];

		const unsigned char b_1 = vram[tileAddr] & 0xff;
		const unsigned char b_2 = vram[tileAddr] >> 8;
		const unsigned char b_3 = vram[tileAddr + 8] & 0xff;
		const unsigned char b_4 = vram[tileAddr + 8] >> 8;

		for (int x = 7;x >= 0;x--)
		{
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

void ppu::tileViewerRenderTile8bpp(int px, int py, int tileAddr)
{
	unsigned char* pBuf;
	const int numCols = 256;
	unsigned char palArr[3 * numCols];

	int colidx = (0 * numCols) * 2;

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

	//int tileAddr = tileNum * 32;
	//tileAddr += ((bgTileBaseAddress >> (4 * bgnum)) & 0x0f) * 1024 * 4;
	//tileAddr &= 0x7fff;

	int ybase = 0; int yend = 8; int yinc = 1; //int tileAddrInc = 1;

	for (int y = ybase;y != yend;y += yinc)
	{
		pBuf = &vramViewerBitmap[(px * 4) + ((py + y) * ppuResolutionX * 4)];

		const unsigned char b_1 = vram[tileAddr] & 0xff;
		const unsigned char b_2 = vram[tileAddr] >> 8;
		const unsigned char b_3 = vram[tileAddr + 8] & 0xff;
		const unsigned char b_4 = vram[tileAddr + 8] >> 8;
		const unsigned char b_5 = vram[tileAddr + 16] & 0xff;
		const unsigned char b_6 = vram[tileAddr + 16] >> 8;
		const unsigned char b_7 = vram[tileAddr + 24] & 0xff;
		const unsigned char b_8 = vram[tileAddr + 24] >> 8;

		int xbase = 7; int xend = -1; int xinc = -1;

		for (int x = xbase;x != xend;x += xinc)
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

void ppu::tileViewerRenderTiles()
{
	//int tileAddr = 0x0000;
	int tileAddr = 0x2000;
	for (int y = 0;y < 16;y++)
	{
		for (int x = 0;x < 16;x++)
		{
			//tileViewerRenderTile2bpp(x * 8, y * 8, tileAddr);
			//tileViewerRenderTile4bpp(x * 8, y * 8, tileAddr);
			tileViewerRenderTile8bpp(x * 8, y * 8, tileAddr);
			tileAddr += 32;
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

void ppu::renderTileScanline(int bpp, int px, int py, int tileNum, int palId, int bgnum, int xflip, int yflip,int scanlinenum,unsigned char bgpri)
{
	unsigned char* pBuf;
	int maxXvalue = 256;
	unsigned int maxPixel = (ppuResolutionX * ppuResolutionY * 4);
	if ((bgMode&0x07)==0x05)
	{
		maxXvalue = 512;
		maxPixel = (ppuResolutionX*2*ppuResolutionY*2*4);
	}

	// palette setup

	const int numCols = (bpp == 2) ? 4 : (bpp == 4) ? 16 : 256;
	//unsigned char* palArr = new unsigned char[3 * numCols];

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
	else if (bpp==4)
	{
		colidx = (palId * numCols) * 2;
	}
	else
	{
		colidx = 0;
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
	if ((ppos >= 0) && (ppos < maxPixel))
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
			//if ((ppos >= 0) && (ppos < maxPixel))
			{
				unsigned char curCol;

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

				if ((theX >= 0) && (theX < maxXvalue))
				{
					unsigned char* pBgColorAppo;
					unsigned char* pBgPriAppo;
					bool* pBgIsTranspAppo;

					// hi-res for mode 5
					if ((bgMode & 0x07) == 0x05)
					{
						pBgColorAppo = &bgColorAppoHires[bgnum][theX * 4];
						pBgPriAppo = &bgPriorityAppoHires[bgnum][theX];
						pBgIsTranspAppo = &bgIsTransparentAppoHires[bgnum][theX];
					}
					else
					{
						pBgColorAppo = &bgColorAppo[bgnum][theX * 4];
						pBgPriAppo = &bgPriorityAppo[bgnum][theX];
						pBgIsTranspAppo = &bgIsTransparentAppo[bgnum][theX];
					}

					if (bpp == 2)
					{
						if ((curCol % 4) != 0)
						{
							*pBgColorAppo = palarrLookup[(((colidx>>1)+curCol) * 3) + 0]; pBgColorAppo++;
							*pBgColorAppo = palarrLookup[(((colidx >> 1) + curCol) * 3) + 1]; pBgColorAppo++;
							*pBgColorAppo = palarrLookup[(((colidx >> 1) + curCol) * 3) + 2]; pBgColorAppo++;

							*pBgColorAppo = 0xff; 
							*pBgPriAppo = bgpri; 
							*pBgIsTranspAppo = false; 
						}
						else
						{
							//pBuf += 4;
							pBgColorAppo += 4;
							*pBgPriAppo = bgpri; 
							*pBgIsTranspAppo = true; 
						}
					}
					else if (bpp == 4)
					{
						if ((curCol % 16) != 0)
						{
							*pBgColorAppo = palarrLookup[(((palId * numCols)+curCol) * 3) + 0]; pBgColorAppo++;
							*pBgColorAppo = palarrLookup[(((palId * numCols)+ curCol) * 3) + 1]; pBgColorAppo++;
							*pBgColorAppo = palarrLookup[(((palId * numCols)+ curCol) * 3) + 2]; pBgColorAppo++;

							*pBgColorAppo = 0xff; 
							*pBgPriAppo = bgpri; 
							*pBgIsTranspAppo = false; 
						}
						else
						{
							//pBuf += 4;
							pBgColorAppo += 4;
							*pBgPriAppo = bgpri; 
							*pBgIsTranspAppo = true; 
						}
					}
					else
					{
						// direct color TODO

						*pBgColorAppo = palarrLookup[(curCol * 3) + 0]; pBgColorAppo++;
						*pBgColorAppo = palarrLookup[(curCol * 3) + 1]; pBgColorAppo++;
						*pBgColorAppo = palarrLookup[(curCol * 3) + 2]; pBgColorAppo++;
						*pBgColorAppo = 0xff; 
						
						*pBgPriAppo = bgpri; 

						if (curCol==0) *pBgIsTranspAppo = true;
						else *pBgIsTranspAppo = false; 
					}
				}
			}

			ppos += 4;
			theX += 1;
		}

		tileAddr += 1;
	}

	//delete(palArr);
}

void ppu::buildTilemapMap(unsigned short int tilemapMap[][64], int bgSize, int baseTileAddr, unsigned int rowtobuild)
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

	for (unsigned int y = 0;y < 32;y++)
	{
		if (y == rowtobuild)
		{
			for (int x = 0;x < 32;x++)
			{
				tilemapMap[y][x] = vram[tilemapPos & 0x7fff];
				tilemapPos++;
			}
		}
		else
		{
			tilemapPos += 32;
		}
	}

	if ((bgSize == 0) || (bgSize == 2)) tilemapPos -= 0x400;

	for (unsigned int y = 0;y < 32;y++)
	{
		if (y == rowtobuild)
		{
			for (int x = 32;x < 64;x++)
			{
				tilemapMap[y][x] = vram[tilemapPos & 0x7fff];
				tilemapPos++;
			}
		}
		else
		{
			tilemapPos += 32;
		}
	}

	if (bgSize == 0) tilemapPos -= 0x400;

	for (unsigned int y = 32;y < 64;y++)
	{
		if (y == rowtobuild)
		{
			for (int x = 0;x < 32;x++)
			{
				tilemapMap[y][x] = vram[tilemapPos & 0x7fff];
				tilemapPos++;
			}
		}
		else
		{
			tilemapPos += 32;
		}
	}

	if (bgSize == 0) tilemapPos -= 0x400;
	else if (bgSize == 1) tilemapPos -= 0x800;
	else if (bgSize == 2) tilemapPos -= 0x400;

	for (unsigned int y = 32;y < 64;y++)
	{
		if (y == rowtobuild)
		{
			for (int x = 32;x < 64;x++)
			{
				tilemapMap[y][x] = vram[tilemapPos & 0x7fff];
				tilemapPos++;
			}
		}
		else
		{
			tilemapPos += 32;
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

	/* 8x8 or 16x16 tiles for modes 0-4 */
	int bgTileSize = (bgMode & (0x10 << bgnum));

	int tileDim = 8;
	int tileDimX = 33;
	int tileDimY = 29;

	if (bgTileSize > 0)
	{
		if ((bgMode & 0x07) != 0x05)
		{
			tileDim = 16;
			tileDimX = 17;
			tileDimY = 15;
		}
		else
		{
			tileDim = 16;
		}
	}

	const int y = (scanlinenum - ((scanlinenum) % tileDim)) / tileDim;
	int realy = y + (yscroll / tileDim);
	
	int andery = 0x3f;
	if ((bgSize == 0) || (bgSize == 1)) andery = 0x1f;

	if (((yscroll % tileDim) + (scanlinenum % tileDim)) > (tileDim - 1))
	{
		realy += 1;
	}

	unsigned short int tilemapMap[64][64];
	buildTilemapMap(tilemapMap, bgSize, baseTileAddr, realy & andery);

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
				realy = y + (yscroll / tileDim);
				if (((yscroll % tileDim) + (scanlinenum % tileDim)) > (tileDim - 1))
				{
					realy += 1;
				}

				buildTilemapMap(tilemapMap, bgSize, baseTileAddr, realy & andery);
			}
		}

		int realx = x + (xscroll / tileDim);

		int anderx = 0x3f; 
		if ((bgSize == 0) || (bgSize == 2)) anderx = 0x1f;

		unsigned short int vramWord = tilemapMap[realy & andery][realx & anderx];
		const int tileNum = vramWord & 0x3ff;
		const int palId = (vramWord >> 10) & 0x7;
		const unsigned char bgPri = (vramWord >> 13) & 0x01;
		const int xflip = (vramWord >> 14) & 0x01;
		const int yflip = (vramWord >> 15) & 0x01;

		if (tileDim == 8)
		{
			if ((bgMode & 0x07) == 0x05)
			{
				renderTileScanline(bpp, (x * 16) - (xscroll % tileDim), yscroll, tileNum, palId, bgnum, xflip, yflip, scanlinenum, bgPri);
				renderTileScanline(bpp, ((x * 16) + 8) - (xscroll % tileDim), yscroll, (tileNum + 1) & 0x3ff, palId, bgnum, xflip, yflip, scanlinenum, bgPri);
			}
			else
			{
				renderTileScanline(bpp, (x * 8) - (xscroll % tileDim), yscroll, tileNum, palId, bgnum, xflip, yflip, scanlinenum, bgPri);
			}
		}
		else
		{
			if (((yscroll+scanlinenum) % tileDim) < 8)
			{
				if (!xflip)
				{
					renderTileScanline(bpp, (x * 16) - (xscroll % tileDim), yscroll, tileNum, palId, bgnum, xflip, yflip, scanlinenum, bgPri);
					renderTileScanline(bpp, ((x * 16) + 8) - (xscroll % tileDim), yscroll, (tileNum + 1) & 0x3ff, palId, bgnum, xflip, yflip, scanlinenum, bgPri);
				}
				else
				{
					renderTileScanline(bpp, ((x * 16) + 8) - (xscroll % tileDim), yscroll, tileNum, palId, bgnum, xflip, yflip, scanlinenum, bgPri);
					renderTileScanline(bpp, (x * 16) - (xscroll % tileDim), yscroll, (tileNum + 1) & 0x3ff, palId, bgnum, xflip, yflip, scanlinenum, bgPri);
				}
			}
			else
			{
				if (!xflip)
				{
					renderTileScanline(bpp, (x * 16) - (xscroll % tileDim), yscroll, (tileNum + 16) & 0x3ff, palId, bgnum, xflip, yflip, scanlinenum, bgPri);
					renderTileScanline(bpp, ((x * 16) + 8) - (xscroll % tileDim), yscroll, (tileNum + 17) & 0x3ff, palId, bgnum, xflip, yflip, scanlinenum, bgPri);
				}
				else
				{
					renderTileScanline(bpp, ((x * 16) + 8) - (xscroll % tileDim), yscroll, (tileNum + 16) & 0x3ff, palId, bgnum, xflip, yflip, scanlinenum, bgPri);
					renderTileScanline(bpp, (x * 16) - (xscroll % tileDim), yscroll, (tileNum + 17) & 0x3ff, palId, bgnum, xflip, yflip, scanlinenum, bgPri);
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

	for (unsigned int sprNum = 0; sprNum <128; sprNum++)
	{
		unsigned int spriteNumber = 127-sprNum;
		if (oamPriRot) spriteNumber = (((OAMAddr >> 2) & 0x7f) + 127-sprNum) & 0x7f;

		const unsigned char byte1 = OAM[(spriteNumber * 4)];
		const unsigned char byte2 = OAM[(spriteNumber * 4) + 1];
		const unsigned char byte3 = OAM[(spriteNumber * 4) + 2];
		const unsigned char byte4 = OAM[(spriteNumber * 4) + 3];
		const unsigned char attr = (OAM[512 + (spriteNumber / 4)] >> ((spriteNumber % 4) * 2)) & 0b11;

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

		const unsigned int OAMBase = (obSel & 0x07)<<13;

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
					unsigned int tile_address = OAMBase + ((((tile_nr) + x / 8) * 0x10 + (y % 8) + (y / 8 * 0x100)));
					if (tile_nr&0x100)
					{
						tile_address += (((obSel & 0x18)) << 9);
					}

					const unsigned char b_1 = vram[tile_address&0x7fff] & 0xff;
					const unsigned char b_2 = vram[tile_address & 0x7fff] >> 8;
					const unsigned char b_3 = vram[(tile_address + 8) & 0x7fff] & 0xff;
					const unsigned char b_4 = vram[(tile_address + 8) & 0x7fff] >> 8;

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
							unsigned char* pObjPaletteAppo = &objPaletteAppo[theEx];
							bool* pObjTranspAppo = &objIsTransparentAppo[theEx];

							unsigned int realColIdx = 128 + (paletteNum * 16) + pixPalIdx;
							*pObjColorAppo = palarrLookup[(realColIdx * 3) + 0]; pObjColorAppo++;
							*pObjColorAppo = palarrLookup[(realColIdx * 3) + 1]; pObjColorAppo++;
							*pObjColorAppo = palarrLookup[(realColIdx * 3) + 2]; pObjColorAppo++;
							
							*pObjColorAppo = 0xff; pObjColorAppo++;
							*pObjPriAppo = priority; 
							*pObjTranspAppo = false; 
							*pObjPaletteAppo = (unsigned char)paletteNum;
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
	for (unsigned int x = 0;x < 256;x++)
	{
		bgPriorityAppo[0][x] = 2; 
		bgPriorityAppo[1][x] = 2; 
		bgPriorityAppo[2][x] = 2; 
		bgPriorityAppo[3][x] = 2;

		bgIsTransparentAppo[0][x] = true; bgIsTransparentAppo[1][x] = true; bgIsTransparentAppo[2][x] = true; bgIsTransparentAppo[3][x] = true;

		bgColorAppo[0][(x * 4) + 0] = 0; bgColorAppo[0][(x * 4) + 1] = 0; bgColorAppo[0][(x * 4) + 2] = 0; bgColorAppo[0][(x * 4) + 3] = 0;
		bgColorAppo[1][(x * 4) + 0] = 0; bgColorAppo[1][(x * 4) + 1] = 0; bgColorAppo[1][(x * 4) + 2] = 0; bgColorAppo[1][(x * 4) + 3] = 0;
		bgColorAppo[2][(x * 4) + 0] = 0; bgColorAppo[2][(x * 4) + 1] = 0; bgColorAppo[2][(x * 4) + 2] = 0; bgColorAppo[2][(x * 4) + 3] = 0;
		bgColorAppo[3][(x * 4) + 0] = 0; bgColorAppo[3][(x * 4) + 1] = 0; bgColorAppo[3][(x * 4) + 2] = 0; bgColorAppo[3][(x * 4) + 3] = 0;

		objColorAppo[(x * 4) + 0] = 0; objColorAppo[(x * 4) + 1] = 0; objColorAppo[(x * 4) + 2] = 0; objColorAppo[(x * 4) + 3] = 0;
		objPriorityAppo[x] = 0; 
		objIsTransparentAppo[x] = true; 
		objPaletteAppo[x] = 0;
	}
}

void ppu::resetAppoBuffersHires()
{
	for (unsigned int x = 0;x < 512;x++)
	{
		bgPriorityAppoHires[0][x] = 2;
		bgPriorityAppoHires[1][x] = 2;
		bgPriorityAppoHires[2][x] = 2;
		bgPriorityAppoHires[3][x] = 2;

		bgIsTransparentAppoHires[0][x] = true; bgIsTransparentAppoHires[1][x] = true; bgIsTransparentAppoHires[2][x] = true; bgIsTransparentAppoHires[3][x] = true;

		bgColorAppoHires[0][(x * 4) + 0] = 0; bgColorAppoHires[0][(x * 4) + 1] = 0; bgColorAppoHires[0][(x * 4) + 2] = 0; bgColorAppoHires[0][(x * 4) + 3] = 0;
		bgColorAppoHires[1][(x * 4) + 0] = 0; bgColorAppoHires[1][(x * 4) + 1] = 0; bgColorAppoHires[1][(x * 4) + 2] = 0; bgColorAppoHires[1][(x * 4) + 3] = 0;
		bgColorAppoHires[2][(x * 4) + 0] = 0; bgColorAppoHires[2][(x * 4) + 1] = 0; bgColorAppoHires[2][(x * 4) + 2] = 0; bgColorAppoHires[2][(x * 4) + 3] = 0;
		bgColorAppoHires[3][(x * 4) + 0] = 0; bgColorAppoHires[3][(x * 4) + 1] = 0; bgColorAppoHires[3][(x * 4) + 2] = 0; bgColorAppoHires[3][(x * 4) + 3] = 0;

		objColorAppo[((x>>1) * 4) + 0] = 0; objColorAppo[((x >> 1) * 4) + 1] = 0; objColorAppo[((x >> 1) * 4) + 2] = 0; objColorAppo[((x >> 1) * 4) + 3] = 0;
		objPriorityAppo[x>>1] = 0;
		objIsTransparentAppo[x>>1] = true;
	}
}

/* this fantastic code comes from https://github.com/angelo-wf/LakeSnes */

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
	priority = priority; layer = layer;
	//priority++;
	//layer++;

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
	calculateMode7Starts(scanlinenum);

	unsigned char* pBuf = &screenFramebuffer[(scanlinenum * ppuResolutionX * 4)];
	for (int x = 0;x < 256;x++)
	{
		int colNum = getPixelForMode7(x, 0, false);

		if ((cgwSel & 0x01) == 0)
		{
			*pBuf = palarrLookup[(colNum * 3) + 0]; pBuf++;
			*pBuf = palarrLookup[(colNum * 3) + 1]; pBuf++;
			*pBuf = palarrLookup[(colNum * 3) + 2]; pBuf++;
			*pBuf = 0xff; pBuf++;
		}
		else
		{
			// direct color
			unsigned char red = (unsigned char)((((colNum & 0x7) << 2) | ((colNum & 0x100) >> 7))<<3);
			unsigned char green = (unsigned char)((((colNum & 0x38) >> 1) | ((colNum & 0x200) >> 8))<<3);
			unsigned char blue = (unsigned char)((((colNum & 0xc0) >> 3) | ((colNum & 0x400) >> 8))<<3);
			
			*pBuf = red; pBuf++;
			*pBuf = green; pBuf++;
			*pBuf = blue; pBuf++;
			*pBuf = 0xff; pBuf++;
		}
	}
}

bool ppu::applyWindow(int x)
{
	bool isMathWindowEnabled = false;
	if ( (!(windowTMW & 0x0f)) && (!(windowTSW & 0x0f)) ) return isMathWindowEnabled;

	bool invert1[4] = { false,false,false,false };
	bool invertObj = false;

	// window 1
	if ((w12sel & 0x03)==0x03) invert1[0] = true;
	if ((w12sel & 0x30)==0x30) invert1[1] = true;
	if ((w34sel & 0x03)==0x03) invert1[2] = true;
	if ((w34sel & 0x30)==0x30) invert1[3] = true;
	if ((wobjsel & 0x01)==0x01) invertObj = true;

	if (invert1[0] && ((x < windowxpos[0][0]) || (x > windowxpos[0][1])))
	{
		if (w12sel & 0x02)
		{
			bgIsTransparentAppo[0][x] = true;
			isMathWindowEnabled = true;
		}
	}
	if ((!invert1[0]) && ((x >= windowxpos[0][0]) && (x <= windowxpos[0][1])))
	{
		if (w12sel & 0x02)
		{
			bgIsTransparentAppo[0][x] = true;
			isMathWindowEnabled = true;
		}
	}

	if (invert1[1] && ((x < windowxpos[0][0]) || (x > windowxpos[0][1])))
	{
		if (w12sel & 0x20)
		{
			bgIsTransparentAppo[1][x] = true;
			isMathWindowEnabled = true;
		}
	}
	if ((!invert1[1]) && ((x >= windowxpos[0][0]) && (x <= windowxpos[0][1])))
	{
		if (w12sel & 0x20)
		{
			bgIsTransparentAppo[1][x] = true;
			isMathWindowEnabled = true;
		}
	}

	if (invert1[2] && ((x < windowxpos[0][0]) || (x > windowxpos[0][1])))
	{
		if (w34sel & 0x02)
		{
			bgIsTransparentAppo[2][x] = true;
			isMathWindowEnabled = true;
		}
	}
	if ((!invert1[2]) && ((x >= windowxpos[0][0]) && (x <= windowxpos[0][1])))
	{
		if (w34sel & 0x02)
		{
			bgIsTransparentAppo[2][x] = true;
			isMathWindowEnabled = true;
		}
	}

	if (invert1[3] && ((x < windowxpos[0][0]) || (x > windowxpos[0][1])))
	{
		if (w34sel & 0x20)
		{
			bgIsTransparentAppo[3][x] = true;
			isMathWindowEnabled = true;
		}
	}
	if ((!invert1[3]) && ((x >= windowxpos[0][0]) && (x <= windowxpos[0][1])))
	{
		if (w34sel & 0x20)
		{
			bgIsTransparentAppo[3][x] = true;
			isMathWindowEnabled = true;
		}
	}

	if (invertObj && ((x < windowxpos[0][0]) || (x > windowxpos[0][1])))
	{
		if (wobjsel & 0x02)
		{
			objIsTransparentAppo[x] = true;
			isMathWindowEnabled = true;
		}
	}
	if ((!invertObj) && ((x >= windowxpos[0][0]) && (x <= windowxpos[0][1])))
	{
		if (wobjsel & 0x02)
		{
			objIsTransparentAppo[x] = true;
			isMathWindowEnabled = true;
		}
	}

	return isMathWindowEnabled;
}

void ppu::upskaleToHires(unsigned int scanlinenn)
{
	unsigned char* pfbuf = &screenFramebuffer[scanlinenn * ppuResolutionX * 4];
	unsigned char* pfbufhi = &screenFramebufferHires[(scanlinenn *2) * ppuResolutionX*2 * 4];
	for (unsigned int x = 0;x < 256;x++)
	{
		*pfbufhi = pfbuf[0]; *(pfbufhi+ (ppuResolutionX * 2 * 4)) = pfbuf[0]; pfbufhi++;
		*pfbufhi = pfbuf[1]; *(pfbufhi + (ppuResolutionX * 2 * 4)) = pfbuf[1]; pfbufhi++;
		*pfbufhi = pfbuf[2]; *(pfbufhi + (ppuResolutionX * 2 * 4)) = pfbuf[2]; pfbufhi++;
		*pfbufhi = pfbuf[3]; *(pfbufhi + (ppuResolutionX * 2 * 4)) = pfbuf[3]; pfbufhi++;
		*pfbufhi = pfbuf[0]; *(pfbufhi + (ppuResolutionX * 2 * 4)) = pfbuf[0]; pfbufhi++;
		*pfbufhi = pfbuf[1]; *(pfbufhi + (ppuResolutionX * 2 * 4)) = pfbuf[1]; pfbufhi++;
		*pfbufhi = pfbuf[2]; *(pfbufhi + (ppuResolutionX * 2 * 4)) = pfbuf[2]; pfbufhi++;
		*pfbufhi = pfbuf[3]; *(pfbufhi + (ppuResolutionX * 2 * 4)) = pfbuf[3]; pfbufhi++;
		pfbuf += 4;
	}
}

unsigned char ppu::clampAdd(unsigned char a, unsigned char b,bool div2)
{
	int ai = a; int bi = b;
	int sum = ai + bi;
	if (div2) sum /= 2;
	if (sum > 255) return 255;
	return (unsigned char)sum;
}

unsigned char ppu::clampSub(unsigned char a, unsigned char b,bool div2)
{
	int ai = a; int bi = b;
	int diff = ai - bi;
	if (div2) diff /= 2;
	if (diff<0) return 0;
	return (unsigned char)diff;
}

void ppu::doColorMath(int maincol, int subcol, unsigned char redm, unsigned char greenm, unsigned char bluem,
	unsigned char reds, unsigned char greens, unsigned char blues,
	unsigned char& red, unsigned char& green, unsigned char& blue,
	unsigned char objPalette)
{
	subcol = subcol;

	/*
	  2131h - CGADSUB - Color Math Control Register B (W)
	  7    Color Math Add/Subtract        (0=Add; Main+Sub, 1=Subtract; Main-Sub)
	  6    Color Math "Div2" Half Result  (0=No divide, 1=Divide result by 2)
	  5    Color Math when Main Screen = Backdrop        (0=Off, 1=On) ;\
	  4    Color Math when Main Screen = OBJ/Palette4..7 (0=Off, 1=On) ; OFF: Show Raw Main,
	  3    Color Math when Main Screen = BG4             (0=Off, 1=On) ;   or
	  2    Color Math when Main Screen = BG3             (0=Off, 1=On) ; ON: Show
	  1    Color Math when Main Screen = BG2             (0=Off, 1=On) ; Main+/-Sub
	  0    Color Math when Main Screen = BG1             (0=Off, 1=On) ;/
	*/

	bool colorMathAdd = ((cgaddsub & 0x80) == 0);
	bool colorMathDiv2= ((cgaddsub & 0x40) == 0x40);

	if (
				(((cgaddsub & 0x20) == 0x20) && (maincol == -1)) ||
				(((cgaddsub & 0x10) == 0x10) && (maincol == 4) && ((objPalette >= 4) && (objPalette <= 7))) ||
				(((cgaddsub & 0x01) == 0x01) && (maincol == 0)) ||
				(((cgaddsub & 0x02) == 0x02) && (maincol == 1)) ||
				(((cgaddsub & 0x04) == 0x04) && (maincol == 2)) ||
				(((cgaddsub & 0x08) == 0x08) && (maincol == 3))
		)
	{
		if (colorMathAdd)
		{
			red = clampAdd(redm, reds, colorMathDiv2);
			green = clampAdd(greenm, greens, colorMathDiv2);
			blue = clampAdd(bluem, blues, colorMathDiv2);
		}
		else
		{
			red = clampSub(redm, reds, colorMathDiv2);
			green = clampSub(greenm, greens, colorMathDiv2);
			blue = clampSub(bluem, blues, colorMathDiv2);
		}
	}
	else
	{
		red = redm; green = greenm; blue = bluem;
	}
}

void ppu::mixLayers(int scanlinenum, int screenMode)
{
	bool bg3_priority = (bgMode & 0x08) != 0;
	unsigned char* pfbuf = &screenFramebuffer[scanlinenum * ppuResolutionX * 4];
	for (int x = 0;x < 256;x++)
	{
		int finalColMain = -1;
		int finalColSub = -1;

		bool isWindowEnabled=applyWindow(x);

		if (screenMode == 0x0)
		{
			if (objPriorityAppo[x] == 3 && (!objIsTransparentAppo[x])) finalColMain = 4;
			else if ((bgPriorityAppo[0][x] == 1) && (((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) && (!bgIsTransparentAppo[0][x])) finalColMain = 0;
			else if ((bgPriorityAppo[1][x] == 1) && (((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) && (!bgIsTransparentAppo[1][x])) finalColMain = 1;
			else if (objPriorityAppo[x] == 2 && (!objIsTransparentAppo[x])) finalColMain = 4;
			else if ((bgPriorityAppo[0][x] == 0) && (((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) && (!bgIsTransparentAppo[0][x])) finalColMain = 0;
			else if ((bgPriorityAppo[1][x] == 0) && (((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) && (!bgIsTransparentAppo[1][x])) finalColMain = 1;
			else if (objPriorityAppo[x] == 1 && (!objIsTransparentAppo[x])) finalColMain = 4;
			else if ((bgPriorityAppo[2][x] == 1) && (((mainScreenDesignation & 0x1f) & (1 << 2)) > 0) && (!bgIsTransparentAppo[2][x])) finalColMain = 2;
			else if ((bgPriorityAppo[3][x] == 1) && (((mainScreenDesignation & 0x1f) & (1 << 3)) > 0) && (!bgIsTransparentAppo[3][x])) finalColMain = 3;
			else if (objPriorityAppo[x] == 0 && (!objIsTransparentAppo[x])) finalColMain = 4;
			else if ((bgPriorityAppo[2][x] == 0) && (((mainScreenDesignation & 0x1f) & (1 << 2)) > 0) && (!bgIsTransparentAppo[2][x])) finalColMain = 2;
			else if ((bgPriorityAppo[3][x] == 0) && (((mainScreenDesignation & 0x1f) & (1 << 3)) > 0) && (!bgIsTransparentAppo[3][x])) finalColMain = 3;

			if ((subScreenDesignation & 0x1f) > 0)
			{
				if (objPriorityAppo[x] == 3 && (!objIsTransparentAppo[x])) finalColSub = 4;
				else if ((bgPriorityAppo[0][x] == 1) && (((subScreenDesignation & 0x1f) & (1 << 0)) > 0) && (!bgIsTransparentAppo[0][x])) finalColSub = 0;
				else if ((bgPriorityAppo[1][x] == 1) && (((subScreenDesignation & 0x1f) & (1 << 1)) > 0) && (!bgIsTransparentAppo[1][x])) finalColSub = 1;
				else if (objPriorityAppo[x] == 2 && (!objIsTransparentAppo[x])) finalColSub = 4;
				else if ((bgPriorityAppo[0][x] == 0) && (((subScreenDesignation & 0x1f) & (1 << 0)) > 0) && (!bgIsTransparentAppo[0][x])) finalColSub = 0;
				else if ((bgPriorityAppo[1][x] == 0) && (((subScreenDesignation & 0x1f) & (1 << 1)) > 0) && (!bgIsTransparentAppo[1][x])) finalColSub = 1;
				else if (objPriorityAppo[x] == 1 && (!objIsTransparentAppo[x])) finalColSub = 4;
				else if ((bgPriorityAppo[2][x] == 1) && (((subScreenDesignation & 0x1f) & (1 << 2)) > 0) && (!bgIsTransparentAppo[2][x])) finalColSub = 2;
				else if ((bgPriorityAppo[3][x] == 1) && (((subScreenDesignation & 0x1f) & (1 << 3)) > 0) && (!bgIsTransparentAppo[3][x])) finalColSub = 3;
				else if (objPriorityAppo[x] == 0 && (!objIsTransparentAppo[x])) finalColSub = 4;
				else if ((bgPriorityAppo[2][x] == 0) && (((subScreenDesignation & 0x1f) & (1 << 2)) > 0) && (!bgIsTransparentAppo[2][x])) finalColSub = 2;
				else if ((bgPriorityAppo[3][x] == 0) && (((subScreenDesignation & 0x1f) & (1 << 3)) > 0) && (!bgIsTransparentAppo[3][x])) finalColSub = 3;
			}
		}
		else if (screenMode == 0x01)
		{
			if ((bgPriorityAppo[2][x] == 1) && bg3_priority && (((mainScreenDesignation & 0x1f) & (1 << 2)) > 0) && (!bgIsTransparentAppo[2][x])) finalColMain = 2;
			else if ((objPriorityAppo[x] == 3) && (!objIsTransparentAppo[x]) && (((mainScreenDesignation & 0x1f) & (1 << 4)) > 0) ) finalColMain = 4;
			else if ((bgPriorityAppo[0][x] == 1) && (((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) && (!bgIsTransparentAppo[0][x])) finalColMain = 0;
			else if ((bgPriorityAppo[1][x] == 1) && (((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) && (!bgIsTransparentAppo[1][x])) finalColMain = 1;
			else if ((objPriorityAppo[x] == 2) && (!objIsTransparentAppo[x]) && (((mainScreenDesignation & 0x1f) & (1 << 4)) > 0)) finalColMain = 4;
			else if ((bgPriorityAppo[0][x] == 0) && (((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) && (!bgIsTransparentAppo[0][x])) finalColMain = 0;
			else if ((bgPriorityAppo[1][x] == 0) && (((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) && (!bgIsTransparentAppo[1][x])) finalColMain = 1;
			else if ((objPriorityAppo[x] == 1) && (!objIsTransparentAppo[x]) && (((mainScreenDesignation & 0x1f) & (1 << 4)) > 0)) finalColMain = 4;
			else if ((bgPriorityAppo[2][x] == 1) && (!bg3_priority) && (((mainScreenDesignation & 0x1f) & (1 << 2)) > 0) && (!bgIsTransparentAppo[2][x])) finalColMain = 2;
			else if ((objPriorityAppo[x] == 0) && (!objIsTransparentAppo[x]) && (((mainScreenDesignation & 0x1f) & (1 << 4)) > 0)) finalColMain = 4;
			else if ((bgPriorityAppo[2][x] == 0) && bg3_priority && (((mainScreenDesignation & 0x1f) & (1 << 2)) > 0) && (!bgIsTransparentAppo[2][x])) finalColMain = 2;
			else if ((bgPriorityAppo[2][x] == 0) && (!bg3_priority) && (((mainScreenDesignation & 0x1f) & (1 << 2)) > 0) && (!bgIsTransparentAppo[2][x])) finalColMain = 2;

			if ((subScreenDesignation & 0x1f) > 0)
			{
				if ((bgPriorityAppo[2][x] == 1) && bg3_priority && (((subScreenDesignation & 0x1f) & (1 << 2)) > 0) && (!bgIsTransparentAppo[2][x])) finalColSub = 2;
				else if ((objPriorityAppo[x] == 3) && (!objIsTransparentAppo[x]) && (((subScreenDesignation & 0x1f) & (1 << 4)) > 0)) finalColSub = 4;
				else if ((bgPriorityAppo[0][x] == 1) && (((subScreenDesignation & 0x1f) & (1 << 0)) > 0) && (!bgIsTransparentAppo[0][x])) finalColSub = 0;
				else if ((bgPriorityAppo[1][x] == 1) && (((subScreenDesignation & 0x1f) & (1 << 1)) > 0) && (!bgIsTransparentAppo[1][x])) finalColSub = 1;
				else if ((objPriorityAppo[x] == 2) && (!objIsTransparentAppo[x]) && (((subScreenDesignation & 0x1f) & (1 << 4)) > 0)) finalColSub = 4;
				else if ((bgPriorityAppo[0][x] == 0) && (((subScreenDesignation & 0x1f) & (1 << 0)) > 0) && (!bgIsTransparentAppo[0][x])) finalColSub = 0;
				else if ((bgPriorityAppo[1][x] == 0) && (((subScreenDesignation & 0x1f) & (1 << 1)) > 0) && (!bgIsTransparentAppo[1][x])) finalColSub = 1;
				else if ((objPriorityAppo[x] == 1) && (!objIsTransparentAppo[x]) && (((subScreenDesignation & 0x1f) & (1 << 4)) > 0)) finalColSub = 4;
				else if ((bgPriorityAppo[2][x] == 1) && (!bg3_priority) && (((subScreenDesignation & 0x1f) & (1 << 2)) > 0) && (!bgIsTransparentAppo[2][x])) finalColSub = 2;
				else if ((objPriorityAppo[x] == 0) && (!objIsTransparentAppo[x]) && (((subScreenDesignation & 0x1f) & (1 << 4)) > 0)) finalColSub = 4;
				else if ((bgPriorityAppo[2][x] == 0) && bg3_priority && (((subScreenDesignation & 0x1f) & (1 << 2)) > 0) && (!bgIsTransparentAppo[2][x])) finalColSub = 2;
				else if ((bgPriorityAppo[2][x] == 0) && (!bg3_priority) && (((subScreenDesignation & 0x1f) & (1 << 2)) > 0) && (!bgIsTransparentAppo[2][x])) finalColSub = 2;
			}
		}
		else if (screenMode == 0x02)
		{
			if ((objPriorityAppo[x] == 3) && (!objIsTransparentAppo[x])) finalColMain = 4;
			else if ((bgPriorityAppo[0][x] == 1) && (((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) && (!bgIsTransparentAppo[0][x])) finalColMain = 0;
			else if ((objPriorityAppo[x] == 2) && (!objIsTransparentAppo[x])) finalColMain = 4;
			else if ((bgPriorityAppo[1][x] == 1) && (((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) && (!bgIsTransparentAppo[1][x])) finalColMain = 1;
			else if ((objPriorityAppo[x] == 1) && (!objIsTransparentAppo[x])) finalColMain = 4;
			else if ((bgPriorityAppo[0][x] == 0) && (((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) && (!bgIsTransparentAppo[0][x])) finalColMain = 0;
			else if ((objPriorityAppo[x] == 0) && (!objIsTransparentAppo[x])) finalColMain = 4;
			else if ((bgPriorityAppo[1][x] == 0) && (((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) && (!bgIsTransparentAppo[1][x])) finalColMain = 1;

			if ((subScreenDesignation & 0x1f) > 0)
			{
				if ((objPriorityAppo[x] == 3) && (!objIsTransparentAppo[x])) finalColSub = 4;
				else if ((bgPriorityAppo[0][x] == 1) && (((subScreenDesignation & 0x1f) & (1 << 0)) > 0) && (!bgIsTransparentAppo[0][x])) finalColSub = 0;
				else if ((objPriorityAppo[x] == 2) && (!objIsTransparentAppo[x])) finalColSub = 4;
				else if ((bgPriorityAppo[1][x] == 1) && (((subScreenDesignation & 0x1f) & (1 << 1)) > 0) && (!bgIsTransparentAppo[1][x])) finalColSub = 1;
				else if ((objPriorityAppo[x] == 1) && (!objIsTransparentAppo[x])) finalColSub = 4;
				else if ((bgPriorityAppo[0][x] == 0) && (((subScreenDesignation & 0x1f) & (1 << 0)) > 0) && (!bgIsTransparentAppo[0][x])) finalColSub = 0;
				else if ((objPriorityAppo[x] == 0) && (!objIsTransparentAppo[x])) finalColSub = 4;
				else if ((bgPriorityAppo[1][x] == 0) && (((subScreenDesignation & 0x1f) & (1 << 1)) > 0) && (!bgIsTransparentAppo[1][x])) finalColSub = 1;
			}
		}
		else if (screenMode == 0x03)
		{
			if (objPriorityAppo[x] == 3 && (!objIsTransparentAppo[x]) && (((mainScreenDesignation & 0x1f) & (1 << 4)) > 0)) finalColMain = 4;
			else if ((bgPriorityAppo[0][x] == 1) && (!bgIsTransparentAppo[0][x]) && (((mainScreenDesignation & 0x1f) & (1 << 0)) > 0)) finalColMain = 0;
			else if (objPriorityAppo[x] == 2 && (!objIsTransparentAppo[x]) && (((mainScreenDesignation & 0x1f) & (1 << 4)) > 0)) finalColMain = 4;
			else if ((bgPriorityAppo[1][x] == 1) && (!bgIsTransparentAppo[1][x]) && (((mainScreenDesignation & 0x1f) & (1 << 1)) > 0)) finalColMain = 1;
			else if (objPriorityAppo[x] == 1 && (!objIsTransparentAppo[x]) && (((mainScreenDesignation & 0x1f) & (1 << 4)) > 0)) finalColMain = 4;
			else if ((bgPriorityAppo[0][x] == 0) && (!bgIsTransparentAppo[0][x]) && (((mainScreenDesignation & 0x1f) & (1 << 0)) > 0)) finalColMain = 0;
			else if (objPriorityAppo[x] == 0 && (!objIsTransparentAppo[x]) && (((mainScreenDesignation & 0x1f) & (1 << 4)) > 0)) finalColMain = 4;
			else if ((bgPriorityAppo[1][x] == 0) && (!bgIsTransparentAppo[1][x]) && (((mainScreenDesignation & 0x1f) & (1 << 1)) > 0)) finalColMain = 1;

			if ((subScreenDesignation & 0x1f) > 0)
			{
				if (objPriorityAppo[x] == 3 && (!objIsTransparentAppo[x]) && (((subScreenDesignation & 0x1f) & (1 << 4)) > 0)) finalColSub = 4;
				else if ((bgPriorityAppo[0][x] == 1) && (!bgIsTransparentAppo[0][x]) && (((subScreenDesignation & 0x1f) & (1 << 0)) > 0)) finalColSub = 0;
				else if (objPriorityAppo[x] == 2 && (!objIsTransparentAppo[x]) && (((subScreenDesignation & 0x1f) & (1 << 4)) > 0)) finalColSub = 4;
				else if ((bgPriorityAppo[1][x] == 1) && (!bgIsTransparentAppo[1][x]) && (((subScreenDesignation & 0x1f) & (1 << 1)) > 0)) finalColSub = 1;
				else if (objPriorityAppo[x] == 1 && (!objIsTransparentAppo[x]) && (((subScreenDesignation & 0x1f) & (1 << 4)) > 0)) finalColSub = 4;
				else if ((bgPriorityAppo[0][x] == 0) && (!bgIsTransparentAppo[0][x]) && (((subScreenDesignation & 0x1f) & (1 << 0)) > 0)) finalColSub = 0;
				else if (objPriorityAppo[x] == 0 && (!objIsTransparentAppo[x]) && (((subScreenDesignation & 0x1f) & (1 << 4)) > 0)) finalColSub = 4;
				else if ((bgPriorityAppo[1][x] == 0) && (!bgIsTransparentAppo[1][x]) && (((subScreenDesignation & 0x1f) & (1 << 1)) > 0)) finalColSub = 1;
			}
		}
		else if (screenMode == 0x04)
		{
			if (objPriorityAppo[x] == 3 && (!objIsTransparentAppo[x])) finalColMain = 4;
			else if ((bgPriorityAppo[0][x] == 1) && (((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) && (!bgIsTransparentAppo[0][x])) finalColMain = 0;
			else if (objPriorityAppo[x] == 2 && (!objIsTransparentAppo[x])) finalColMain = 4;
			else if ((bgPriorityAppo[1][x] == 1) && (((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) && (!bgIsTransparentAppo[1][x])) finalColMain = 1;
			else if (objPriorityAppo[x] == 1 && (!objIsTransparentAppo[x])) finalColMain = 4;
			else if ((bgPriorityAppo[0][x] == 0) && (((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) && (!bgIsTransparentAppo[0][x])) finalColMain = 0;
			else if (objPriorityAppo[x] == 0 && (!objIsTransparentAppo[x])) finalColMain = 4;
			else if ((bgPriorityAppo[1][x] == 0) && (((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) && (!bgIsTransparentAppo[1][x])) finalColMain = 1;

			if ((subScreenDesignation & 0x1f) > 0)
			{
				if (objPriorityAppo[x] == 3 && (!objIsTransparentAppo[x])) finalColSub = 4;
				else if ((bgPriorityAppo[0][x] == 1) && (((subScreenDesignation & 0x1f) & (1 << 0)) > 0) && (!bgIsTransparentAppo[0][x])) finalColSub = 0;
				else if (objPriorityAppo[x] == 2 && (!objIsTransparentAppo[x])) finalColSub = 4;
				else if ((bgPriorityAppo[1][x] == 1) && (((subScreenDesignation & 0x1f) & (1 << 1)) > 0) && (!bgIsTransparentAppo[1][x])) finalColSub = 1;
				else if (objPriorityAppo[x] == 1 && (!objIsTransparentAppo[x])) finalColSub = 4;
				else if ((bgPriorityAppo[0][x] == 0) && (((subScreenDesignation & 0x1f) & (1 << 0)) > 0) && (!bgIsTransparentAppo[0][x])) finalColSub = 0;
				else if (objPriorityAppo[x] == 0 && (!objIsTransparentAppo[x])) finalColSub = 4;
				else if ((bgPriorityAppo[1][x] == 0) && (((subScreenDesignation & 0x1f) & (1 << 1)) > 0) && (!bgIsTransparentAppo[1][x])) finalColSub = 1;
			}
		}

		unsigned char redm=0, greenm=0, bluem=0;
		unsigned char reds=0, greens=0, blues=0;

		//2130h - CGWSEL - Color Math Control Register A(W)
		//	7 - 6  Force Main Screen Black(3 = Always, 2 = MathWindow, 1 = NotMathWin, 0 = Never)

		if ((cgwSel & 0xc0) == 0xc0)
		{
		}
		else if (finalColMain == -1)
		{
			redm = palarrLookup[0];
			greenm = palarrLookup[1];
			bluem = palarrLookup[2];
		}
		else if (finalColMain < 4)
		{
			redm = bgColorAppo[finalColMain][(x * 4) + 0];
			greenm = bgColorAppo[finalColMain][(x * 4) + 1];
			bluem = bgColorAppo[finalColMain][(x * 4) + 2];
		}
		else if (finalColMain == 4)
		{
			redm = objColorAppo[(x * 4) + 0];
			greenm = objColorAppo[(x * 4) + 1];
			bluem = objColorAppo[(x * 4) + 2];
		}

		bool subScreenEnabled = (cgwSel & 0x02) == 0x02;

		if (
			(((cgwSel & 0x30) != 0x30)) && 
				(
					(((cgwSel & 0x30) == 0) ) || 
					( (!isWindowEnabled) && ((cgwSel&0x10)) )  ||
					( (!isWindowEnabled) && ((cgwSel&0x20)) )
				)
			)
		{
			// we have color math enabled, let's mix the colors
			if ((finalColSub == -1)||(!subScreenEnabled))
			{
				reds = coldataColor[0];
				greens = coldataColor[1];
				blues = coldataColor[2];
			}
			else if (finalColSub < 4)
			{
				reds = bgColorAppo[finalColSub][(x * 4) + 0];
				greens = bgColorAppo[finalColSub][(x * 4) + 1];
				blues = bgColorAppo[finalColSub][(x * 4) + 2];
			}
			else if (finalColSub == 4)
			{
				reds = objColorAppo[(x * 4) + 0];
				greens = objColorAppo[(x * 4) + 1];
				blues = objColorAppo[(x * 4) + 2];
			}

			unsigned char red, green, blue;
			doColorMath(finalColMain,finalColSub, redm, greenm, bluem, reds, greens, blues, red, green, blue, objPaletteAppo[x]);

			*pfbuf = red; pfbuf++;
			*pfbuf = green; pfbuf++;
			*pfbuf = blue; pfbuf++;
			*pfbuf = 0xff; pfbuf++;
		}
		else
		{
			// no color math
			*pfbuf = redm; pfbuf++;
			*pfbuf = greenm; pfbuf++;
			*pfbuf = bluem; pfbuf++;
			*pfbuf = 0xff; pfbuf++;
		}

	}
}

void ppu::renderScanline(int scanlinenum)
{
	if ((scanlinenum < 0) || (scanlinenum >= (ppuResolutionY-1))) return;

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

	resetAppoBuffers();

	// rendering depends on screen mode
	int screenMode = (bgMode & 0x07);

	if (screenMode == 0)
	{
		// 0      4-color     4-color     4-color     4-color   ;Normal   
		for (int bg = 0;bg < 4;bg++)
		{
			if ( (((mainScreenDesignation & 0x1f) & (1 << bg)) > 0) || (((subScreenDesignation & 0x1f) & (1 << bg)) > 0) )
			{
				renderBGScanline(bg, 2, scanlinenum+1);
			}
		}

		if ((mainScreenDesignation & 0x10) || (subScreenDesignation & 0x10))
		{
			renderSpritesScanline(scanlinenum);
		}

		mixLayers(scanlinenum, screenMode);
		upskaleToHires(scanlinenum);
	}
	else if (screenMode == 0x01)
	{
		// 1      16-color    16-color    4-color     -         ;Normal

		if (((((mainScreenDesignation & 0x1f) & (1 << 2)) > 0) || (((subScreenDesignation & 0x1f) & (1 << 2)))) && isBgActive[2] ) renderBGScanline(2, 2, scanlinenum + 1);
		if (((((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) || (((subScreenDesignation & 0x1f) & (1 << 1)))) && isBgActive[1] ) renderBGScanline(1, 4, scanlinenum+1);
		if (((((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) || (((subScreenDesignation & 0x1f) & (1 << 0)))) && isBgActive[0]) renderBGScanline(0, 4, scanlinenum + 1);
		
		if ((mainScreenDesignation & 0x10)|| (subScreenDesignation & 0x10))
		{
			renderSpritesScanline(scanlinenum);
		}

		mixLayers(scanlinenum, screenMode);
		upskaleToHires(scanlinenum);
	}
	else if (screenMode == 0x02)
	{
		// 1      16-color    16-color    opt     -         ;Normal
		if ((((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) || (((subScreenDesignation & 0x1f) & (1 << 1)) > 0)) renderBGScanline(1, 4, scanlinenum+1);
		if ((((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) || (((subScreenDesignation & 0x1f) & (1 << 0)) > 0)) renderBGScanline(0, 4, scanlinenum+1);

		if ((mainScreenDesignation & 0x10) || (subScreenDesignation & 0x10))
		{
			renderSpritesScanline(scanlinenum);
		}

		mixLayers(scanlinenum, screenMode);
		upskaleToHires(scanlinenum);
	}
	else if (screenMode == 0x03)
	{
		// 3      256-color   16-color    -           -         ;Normal   
		if ( ( (((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) || (((subScreenDesignation & 0x1f) & (1 << 1))) ) && isBgActive[1]) renderBGScanline(1, 4, scanlinenum+1);
		if ( ((((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) || (((subScreenDesignation & 0x1f) & (1 << 0)))) && isBgActive[0]) renderBGScanline(0, 8, scanlinenum+1);

		if ((mainScreenDesignation & 0x10) || (subScreenDesignation & 0x10))
		{
			renderSpritesScanline(scanlinenum);
		}

		mixLayers(scanlinenum, screenMode);
		upskaleToHires(scanlinenum);
	}
	else if (screenMode == 0x04)
	{
		if ( (((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) || (((subScreenDesignation & 0x1f) & (1 << 1)) > 0) ) renderBGScanline(1, 2, scanlinenum+1);
		if ( (((mainScreenDesignation & 0x1f) & (1 << 0)) > 0) || (((subScreenDesignation & 0x1f) & (1 << 0)) > 0) ) renderBGScanline(0, 8, scanlinenum+1);

		if ((mainScreenDesignation & 0x10) || (subScreenDesignation & 0x10))
		{
			renderSpritesScanline(scanlinenum);
		}

		mixLayers(scanlinenum, screenMode);
		upskaleToHires(scanlinenum);
	}
	else if (screenMode == 0x05)
	{
		resetAppoBuffersHires();

		if ((mainScreenDesignation & 0x10) || (subScreenDesignation & 0x10))
		{
			renderSpritesScanline(scanlinenum);
		}

		if (((mainScreenDesignation & 0x1f) & (1 << 0)) > 0)
		{
			if (setini & 0x01)
			{
				renderBGScanline(0, 4, (scanlinenum + 1) * 2);
				if (((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) renderBGScanline(1, 2, (scanlinenum + 1) * 2);
				unsigned char* pfbuf = &screenFramebufferHires[(scanlinenum * 2) * ppuResolutionX * 2 * 4];
				for (int x = 0;x < 512;x++)
				{
					int finalCol = -1;

					if (objPriorityAppo[x >> 1] == 3 && (!objIsTransparentAppo[x >> 1])) finalCol = 4;
					else if ((bgPriorityAppoHires[0][x] == 1) && (!bgIsTransparentAppoHires[0][x])) finalCol = 0;
					else if (objPriorityAppo[x >> 1] == 2 && (!objIsTransparentAppo[x >> 1])) finalCol = 4;
					else if ((bgPriorityAppoHires[1][x] == 1) && (!bgIsTransparentAppoHires[1][x])) finalCol = 1;
					else if (objPriorityAppo[x >> 1] == 1 && (!objIsTransparentAppo[x >> 1])) finalCol = 4;
					else if ((bgPriorityAppoHires[0][x] == 0) && (!bgIsTransparentAppoHires[0][x])) finalCol = 0;
					else if (objPriorityAppo[x >> 1] == 0 && (!objIsTransparentAppo[x >> 1])) finalCol = 4;
					else if ((bgPriorityAppoHires[1][x] == 0) && (!bgIsTransparentAppoHires[1][x])) finalCol = 1;

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
						*pfbuf = bgColorAppoHires[finalCol][(x * 4) + 0]; pfbuf++;
						*pfbuf = bgColorAppoHires[finalCol][(x * 4) + 1]; pfbuf++;
						*pfbuf = bgColorAppoHires[finalCol][(x * 4) + 2]; pfbuf++;
						*pfbuf = 0xff; pfbuf++;
					}
					else if (finalCol == 4)
					{
						*pfbuf = objColorAppo[((x >> 1) * 4) + 0]; pfbuf++;
						*pfbuf = objColorAppo[((x >> 1) * 4) + 1]; pfbuf++;
						*pfbuf = objColorAppo[((x >> 1) * 4) + 2]; pfbuf++;
						*pfbuf = 0xff; pfbuf++;
					}
				}

				renderBGScanline(0, 4, ((scanlinenum + 1) * 2) + 1);
				if (((mainScreenDesignation & 0x1f) & (1 << 1)) > 0) renderBGScanline(1, 2, ((scanlinenum + 1) * 2) + 1);
				for (int x = 0;x < 512;x++)
				{
					int finalCol = -1;

					if (objPriorityAppo[x >> 1] == 3 && (!objIsTransparentAppo[x >> 1])) finalCol = 4;
					else if ((bgPriorityAppoHires[0][x] == 1) && (!bgIsTransparentAppoHires[0][x])) finalCol = 0;
					else if (objPriorityAppo[x >> 1] == 2 && (!objIsTransparentAppo[x >> 1])) finalCol = 4;
					else if ((bgPriorityAppoHires[1][x] == 1) && (!bgIsTransparentAppoHires[1][x])) finalCol = 1;
					else if (objPriorityAppo[x >> 1] == 1 && (!objIsTransparentAppo[x >> 1])) finalCol = 4;
					else if ((bgPriorityAppoHires[0][x] == 0) && (!bgIsTransparentAppoHires[0][x])) finalCol = 0;
					else if (objPriorityAppo[x >> 1] == 0 && (!objIsTransparentAppo[x >> 1])) finalCol = 4;
					else if ((bgPriorityAppoHires[1][x] == 0) && (!bgIsTransparentAppoHires[1][x])) finalCol = 1;

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
						*pfbuf = bgColorAppoHires[finalCol][(x * 4) + 0]; pfbuf++;
						*pfbuf = bgColorAppoHires[finalCol][(x * 4) + 1]; pfbuf++;
						*pfbuf = bgColorAppoHires[finalCol][(x * 4) + 2]; pfbuf++;
						*pfbuf = 0xff; pfbuf++;
					}
					else if (finalCol == 4)
					{
						*pfbuf = objColorAppo[((x >> 1) * 4) + 0]; pfbuf++;
						*pfbuf = objColorAppo[((x >> 1) * 4) + 1]; pfbuf++;
						*pfbuf = objColorAppo[((x >> 1) * 4) + 2]; pfbuf++;
						*pfbuf = 0xff; pfbuf++;
					}
				}
			}
			else
			{
				renderBGScanline(0, 4, (scanlinenum + 1));
				unsigned char* pfbuf = &screenFramebufferHires[(scanlinenum*2) * ppuResolutionX * 2 * 4];
				for (int x = 0;x < 512;x++)
				{
					int finalCol = -1;

					if (objPriorityAppo[x>>1] == 3 && (!objIsTransparentAppo[x>>1])) finalCol = 4;
					else if ((bgPriorityAppoHires[0][x] == 1) && (!bgIsTransparentAppoHires[0][x])) finalCol = 0;
					else if (objPriorityAppo[x>>1] == 2 && (!objIsTransparentAppo[x>>1])) finalCol = 4;
					else if ((bgPriorityAppoHires[1][x] == 1) && (!bgIsTransparentAppoHires[1][x])) finalCol = 1;
					else if (objPriorityAppo[x>>1] == 1 && (!objIsTransparentAppo[x>>1])) finalCol = 4;
					else if ((bgPriorityAppoHires[0][x] == 0) && (!bgIsTransparentAppoHires[0][x])) finalCol = 0;
					else if (objPriorityAppo[x>>1] == 0 && (!objIsTransparentAppo[x>>1])) finalCol = 4;
					else if ((bgPriorityAppoHires[1][x] == 0) && (!bgIsTransparentAppoHires[1][x])) finalCol = 1;

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
						*pfbuf = bgColorAppoHires[finalCol][(x * 4) + 0]; *(pfbuf + (ppuResolutionX * 2 * 4)) = bgColorAppoHires[finalCol][(x * 4) + 0]; pfbuf++;
						*pfbuf = bgColorAppoHires[finalCol][(x * 4) + 1]; *(pfbuf + (ppuResolutionX * 2 * 4)) = bgColorAppoHires[finalCol][(x * 4) + 1]; pfbuf++;
						*pfbuf = bgColorAppoHires[finalCol][(x * 4) + 2]; *(pfbuf + (ppuResolutionX * 2 * 4)) = bgColorAppoHires[finalCol][(x * 4) + 2]; pfbuf++;
						*pfbuf = 0xff; *(pfbuf + (ppuResolutionX * 2 * 4)) = 0xff; pfbuf++;
					}
					else if (finalCol == 4)
					{
						*pfbuf = objColorAppo[((x>>1) * 4) + 0]; pfbuf++;
						*pfbuf = objColorAppo[((x>>1) * 4) + 1]; pfbuf++;
						*pfbuf = objColorAppo[((x>>1) * 4) + 2]; pfbuf++;
						*pfbuf = 0xff; pfbuf++;
					}
				}
			}
		}
	}
	else if (screenMode == 0x07)
	{
		renderMode7Scanline(scanlinenum);

		if ((mainScreenDesignation & 0x10) || (subScreenDesignation & 0x10))
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

		upskaleToHires(scanlinenum);
	}

	// final pass, master brightness!
	int brightness = iniDisp & 0x0f;
	if ((iniDisp & 0x80) == 0x80) brightness = 0;

	if (brightness != 15)
	{
		const unsigned int pos = (scanlinenum*2) * ppuResolutionX*2 * 4;
		unsigned char* pFrameBuf = &screenFramebufferHires[pos];
		for (unsigned int x = 0;x < ppuResolutionX*2*2;x++)
		{
			*pFrameBuf = brightnessLookup[((*pFrameBuf) * 16) + brightness];
			pFrameBuf++;
			*pFrameBuf = brightnessLookup[((*pFrameBuf) * 16) + brightness];
			pFrameBuf++;
			*pFrameBuf = brightnessLookup[((*pFrameBuf) * 16) + brightness];
			pFrameBuf += 2;
		}
	}
}

void ppu::step(int numCycles, mmu& theMMU, cpu5a22& theCPU)
{
	int hPos = internalCyclesCounter / 4;

	// HIRQ
	if (theMMU.isHIRQEnabled() && (hPos == theMMU.getHIRQPos()))
	{
		theCPU.triggerIRQ();
		theMMU.setIrqTriggered();
	}

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
			theMMU.resetHDMA(false);
		}
	}
}

ppu::~ppu()
{
	delete(screenFramebuffer);
	delete(screenFramebufferHires);
	delete(vramViewerBitmap);
	delete(brightnessLookup);
}
