
/* surFami emu - BUS, DMAs, etc. */

#include "mmu.h"
#include "logger.h"

extern logger glbTheLogger;

//

mmu::mmu(ppu& thePPU,apu& theAPU)
{
	pPPU = &thePPU;
	pAPU = &theAPU;
	snesRAM = new unsigned char[0x1000000];
	for (int pos = 0;pos < 0x1000000;pos++)
	{
		snesRAM[pos] = 0;
	}
}

void mmu::DMAstart(unsigned char val)
{
	// DMA start - bit set indicates which of the 8 DMA channels to start
	unsigned char dmaMask = val;

	std::stringstream strr;
	strr << std::hex << std::setw(2) << std::setfill('0') << (int)dmaMask;
	std::string sDMAmask = strr.str();

	glbTheLogger.logMsg("DMA start called. DMA channels:"+sDMAmask);

	if (dmaMask == 0) return; // nothing to do, I hope

	for (int dmaChannel = 0;dmaChannel < 8;dmaChannel++)
	{
		if ((dmaMask & (1 << dmaChannel)) == (1 << dmaChannel))
		{
			unsigned char BBusAddr = snesRAM[0x4301 + (dmaChannel * 0x10)];
			unsigned char DMAConfig = snesRAM[0x4300 + (dmaChannel * 0x10)];
			unsigned char dma_dir = DMAConfig >> 7;             //  0 - write to BBus, 1 - read from BBus
			unsigned char dma_step = (DMAConfig >> 3) & 0b11;       //  0 - increment, 2 - decrement, 1/3 = none
			unsigned char dma_mode = (DMAConfig & 0b111);

			unsigned int targetAddr = (snesRAM[0x4304 + (dmaChannel * 0x10)] << 16) | (snesRAM[0x4303 + (dmaChannel * 0x10)] << 8) | snesRAM[0x4302 + (dmaChannel * 0x10)];
			unsigned short int byteCount = (snesRAM[0x4306 + (dmaChannel * 0x10)] << 8) | snesRAM[0x4305 + (dmaChannel * 0x10)];

			std::stringstream strstrdgb;
			strstrdgb << std::hex << std::setw(4) << std::setfill('0') << (int)targetAddr;
			std::string sDMATargetAddr = strstrdgb.str();
			std::stringstream strstrdgb2;
			strstrdgb2 << std::hex << std::setw(4) << std::setfill('0') << (int)(0x2100 + BBusAddr);
			std::string sDMATargetAddr2 = strstrdgb2.str();

			glbTheLogger.logMsg("DMA chan:" + std::to_string(dmaChannel)+ " DMA mode:"+std::to_string(dma_mode)+" DMA direction:"+std::to_string(dma_dir));

			if (dma_dir == 0)
			{
				glbTheLogger.logMsg("Data will be written to:" + sDMATargetAddr2 + " DMA bytes to move:" + std::to_string(byteCount));
			}
			else
			{
				glbTheLogger.logMsg("DMA will be written to:" + sDMATargetAddr + " DMA bytes to move:" + std::to_string(byteCount));
			}

			if (dma_mode == 0)
			{
				// 000 -> transfer 1 byte
				while (((snesRAM[0x4306 + (dmaChannel * 0x10)] << 8) | snesRAM[0x4305 + (dmaChannel * 0x10)]) > 0)
				{
					if (!dma_dir)
					{
						write8(0x2100 + BBusAddr, snesRAM[targetAddr]);
					}
					else
					{
						write8(targetAddr,read8(0x2100 + BBusAddr));
					}

					byteCount--;
					snesRAM[0x4306 + (dmaChannel * 0x10)] = byteCount >> 8;
					snesRAM[0x4305 + (dmaChannel * 0x10)] = byteCount & 0xff;
					targetAddr += (dma_step == 0) ? 1 : ((dma_step == 2) ? -1 : 0);
				}
			}
			else if (dma_mode == 1)
			{
				//  001 -> transfer 2 bytes (xx, xx + 1) (e.g. VRAM)
				while (((snesRAM[0x4306 + (dmaChannel * 0x10)] << 8) | snesRAM[0x4305 + (dmaChannel * 0x10)]) > 0)
				{
					if (!dma_dir) 
					{
						write8(0x2100 + BBusAddr, snesRAM[targetAddr]);
						if (!--byteCount) return;
						write8(0x2100 + BBusAddr + 1, snesRAM[targetAddr + 1]);
						if (!--byteCount) return;
					}
					else 
					{
						write8(targetAddr, read8(0x2100 + BBusAddr));
						if (!--byteCount) return;
						write8(targetAddr + 1, read8(0x2100 + BBusAddr + 1));
						if (!--byteCount) return;
					}
					snesRAM[0x4306 + (dmaChannel * 0x10)] = byteCount >> 8;
					snesRAM[0x4305 + (dmaChannel * 0x10)] = byteCount & 0xff;
					targetAddr += (dma_step == 0) ? 2 : ((dma_step == 2) ? -2 : 0);
				}
			}
			else if (dma_mode == 2)
			{
				//	002 -> transfer 2 bytes (xx, xx) (e.g. OAM / CGRAM)
				while (((snesRAM[0x4306 + (dmaChannel * 0x10)] << 8) | snesRAM[0x4305 + (dmaChannel * 0x10)]) > 0)
				{
					if (!dma_dir)
					{
						write8(0x2100 + BBusAddr, snesRAM[targetAddr]);
						if (!--byteCount) return;
						write8(0x2100 + BBusAddr, snesRAM[targetAddr + 1]);
						if (!--byteCount) return;
					}
					else
					{
						write8(targetAddr, read8(0x2100 + BBusAddr));
						if (!--byteCount) return;
						write8(targetAddr, read8(0x2100 + BBusAddr + 1));
						if (!--byteCount) return;
					}
					snesRAM[0x4306 + (dmaChannel * 0x10)] = byteCount >> 8;
					snesRAM[0x4305 + (dmaChannel * 0x10)] = byteCount & 0xff;
					targetAddr += (dma_step == 0) ? 2 : ((dma_step == 2) ? -2 : 0);
				}
			}
			else
			{
				glbTheLogger.logMsg("Error: unsupported dma mode " + std::to_string(dma_mode));
			}
		}
	}
}

void mmu::write8(unsigned int address, unsigned char val)
{
	std::stringstream strr;
	strr << std::hex << std::setw(2) << std::setfill('0') << (int)val;

	unsigned char bank_nr = address >> 16;
	unsigned short int adr = address & 0xffff;

	if (adr < 0x8000 && ((bank_nr < 0x40) || (bank_nr >= 0x80 && bank_nr < 0xc0)))
	{
		//	WRAM
		if (adr < 0x2000)
		{
			snesRAM[0x7e0000 + adr] = val;
		}
		else if (adr == 0x2100)
		{
			// 2100h - INIDISP - Display Control 1
			glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2100 (INIDISP - Display Control 1)");
			pPPU->setINIDISP(val);
		}
		else if (adr == 0x2101)
		{
			// 2101h - OBSEL - Object Size and Object Base
			glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2101 (OBSEL - Object Size and Object Base)");
			pPPU->setOBSEL(val);
		}
		else if (adr == 0x2102)
		{
			// 2102h/2103h - OAMADDL/OAMADDH - OAM Address and Priority Rotation (W)
			pPPU->writeOAMAddressLow(val);
		}
		else if (adr == 0x2103)
		{
			// 2102h/2103h - OAMADDL/OAMADDH - OAM Address and Priority Rotation (W)
			pPPU->writeOAMAddressHigh(val);
		}
		else if (adr == 0x2104)
		{
			// 2104h - OAMDATA - OAM Data Write(W)
			pPPU->writeOAM(val);
		}
		else if (adr == 0x4200)
		{
			// NMITIMEN - Interrupt Enable and Joypad Request (W)
			glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x4200 (NMITIMEN - Interrupt Enable and Joypad Request)");
			nmiTimen = val;
		}
		else if (adr == 0x2180)
		{
			// WMDATA - WRAM Data Read/Write (R/W)
			unsigned int waddr = wram281x[0] | (wram281x[1] << 8) | (wram281x[2] << 16) & 0x1ffff;
			snesRAM[0x7e0000 + waddr] = val;
			waddr += 1; waddr &= 0x1ffff;
			wram281x[0] = waddr & 0xff;
			wram281x[1] = (waddr>>8) & 0xff;
			wram281x[2] = (waddr>>16) & 0x01;
		}
		else if ((adr == 0x2181) || (adr == 0x2182) || (adr == 0x2183))
		{
			unsigned int byte = adr - 0x2181;
			wram281x[byte] = val;
		}
		else if (adr == 0x420B) // DMA start reg
		{
			DMAstart(val);
		}
		else if (adr == 0x2105)
		{
			glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2105 (BG Mode and Character Size Register)");
			pPPU->writeRegister(0x2105, val);
		}
		else if ((adr == 0x2107) || (adr == 0x2108) || (adr == 0x2109) || (adr == 0x210A))
		{
			glbTheLogger.logMsg("Writing [" + strr.str() + "] to 0x2107/8/9/A (BGx Screen Base and Screen Size)");
			pPPU->writeRegister(adr, val);
		}
		else if (adr == 0x2121)
		{
			glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2121 (CGRAM index)");
			pPPU->writeRegister(0x2121, val);
		}
		else if (adr == 0x2122)
		{
			// write to cgram
			//glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2122 (CGRAM write reg)");
			pPPU->writeRegister(0x2122, val);
		}
		else if (adr == 0x2115)
		{
			glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2115 (VRAM Address Increment Mode)");
			pPPU->writeRegister(0x2115, val);
		}
		else if (adr == 0x2116)
		{
			glbTheLogger.logMsg("Writing [" + strr.str() + "] to 0x2116 (VRAM Address Lower)");
			pPPU->writeRegister(0x2116, val);
		}
		else if (adr == 0x2117)
		{
			glbTheLogger.logMsg("Writing [" + strr.str() + "] to 0x2117 (VRAM Address Upper)");
			pPPU->writeRegister(0x2117, val);
		}
		else if (adr == 0x2118)
		{
			//glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2118 (VRAM data write lower 8-bit)");
			pPPU->writeRegister(0x2118, val);
		}
		else if (adr == 0x2119)
		{
			//glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2118 (VRAM data write upper 8-bit)");
			pPPU->writeRegister(0x2119, val);
		}
		else if (adr == 0x210B)
		{
			glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x210B (BG tile base address lower)");
			pPPU->writeRegister(0x210B, val);
		}
		else if (adr == 0x210C)
		{
			glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x210C (BG tile base address upper)");
			pPPU->writeRegister(0x210C, val);
		}
		else if (adr == 0x212c)
		{
			glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x212C (TM - Main Screen Designation)");
			pPPU->writeRegister(adr, val);
		}
		else if ((adr == 0x210D) || (adr == 0x210F) || (adr == 0x2111) || (adr == 0x2113))
		{
			int bgid = (adr - 0x210D)>>1;
			if (bgid==0) glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to BG "+std::to_string(bgid) + " scroll X");
			pPPU->writeBgScrollX(bgid, val);
		}
		else if ((adr == 0x210E) || (adr == 0x2110) || (adr == 0x2112) || (adr == 0x2114))
		{
			int bgid = (adr - 0x210E)>>1;
			if (bgid == 0) glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to BG " + std::to_string(bgid) + " scroll Y");
			pPPU->writeBgScrollY(bgid, val);
		}

		snesRAM[adr] = val;
	}
	else
	{
		snesRAM[address] = val;
	}
}

unsigned char mmu::read8(unsigned int address)
{
	unsigned char bank_nr = address >> 16;
	unsigned short int adr = address & 0xffff;

	if (adr < 0x8000 && ((bank_nr < 0x40) || (bank_nr >= 0x80 && bank_nr < 0xc0)))
	{
		//	WRAM
		if (adr < 0x2000)
		{
			return snesRAM[0x7e0000 + adr];
		}
		else if (adr == 0x2180)
		{
			// WMDATA - WRAM Data Read/Write (R/W)
			unsigned int waddr = wram281x[0] | (wram281x[1] << 8) | (wram281x[2] << 16) & 0x1ffff;
			return snesRAM[0x7e0000 + waddr];
			// TODO: increment address on read?
		}
		else if (adr == 0x2138)
		{
			//	PPU - RDOAM - Read OAM Data (R)
			//PPU_readOAM();
			//glbTheLogger.logMsg("Error: reading from 2138");
			return pPPU->readOAM();
		}
		else if ((adr == 0x2139) || (adr == 0x213a))
		{
			//	PPU - VMDATAL - VRAM Write - Low (R)
			//	PPU - VMDATAH - VRAM Write - High (R)
			glbTheLogger.logMsg("Error: reading from 2139-213a");
		}
		else if (adr == 0x213b)
		{
			//	PPU - CGDATA - Palette CGRAM Data Read (R)
			//return PPU_readCGRAM(memory[0x2121]);
			glbTheLogger.logMsg("Error: reading from 213b (CGRAM)");
		}
		else if (adr == 0x213f)
		{
			// TODO PAL/NTSC flag
			//return 0x03;
			return 0x13;
		}
		else if ((adr == 0x2140) || (adr == 0x2141))
		{
			return pAPU->read8(adr);
		}
		else if (adr == 0x4016)
		{
			// 4016h / Read - JOYA - Joypad Input Register A(R)
			// manual reading

			return 0;
		}
		else if (adr==0x4200) 
		{
			return (nmiFlag << 7);
		}
		else if (adr == 0x4210)
		{
			//bool res = pCPU->getNMIFlag();
			bool res = nmiFlag;
			if (res)
			{
				nmiFlag = false;
			}
			return (res << 7) | 0x02;
		}
		else if (adr == 0x4211)
		{
			//	PPU Interrupts - H/V-Timer IRQ Flag (R) [Read/Ack] - TODO
			return 0;
		}
		else if (adr == 0x4212)
		{
			//	PPU Interrupts - H/V-Blank Flag and Joypad Busy Flag (R) - TODO
			if (isKeySelectPressed || isKeyStartPressed)
			{
				return 0x01;
			}
			return 0;
		}
		else if ((adr == 0x4218)|| (adr == 0x421a))
		{
			unsigned char res = 0;
			if (isKeyAPressed)
			{
				isKeyAPressed = false;
				res |= 0x80;
			}
			if (isKeyXPressed)
			{
				isKeyXPressed = false;
				res |= 0x40;
			}

			return res;
		}
		else if ((adr == 0x4219)|| (adr == 0x421b))
		{
			unsigned char res = 0;
			if (isKeySelectPressed)
			{
				isKeySelectPressed = false;
				res |= 0x20;
			}
			if (isKeyStartPressed)
			{
				isKeyStartPressed = false;
				res |= 0x10;
			}
			if (isKeyRightPressed)
			{
				isKeyRightPressed = false;
				res |= 0x01;
			}
			if (isKeyLeftPressed)
			{
				isKeyLeftPressed = false;
				res |= 0x02;
			}
			if (isKeyDownPressed)
			{
				isKeyDownPressed = false;
				res |= 0x04;
			}
			if (isKeyUpPressed)
			{
				isKeyUpPressed = false;
				res |= 0x08;
			}

			return res;
		}
		else
		{
			return snesRAM[adr];
		}
	}
	else
	{
		// sram - some games (like tetris&dr.mario) complain if they find SRAM at this address, 
		// and refuse to run, thinking you are using a copier 
		if ((bank_nr >= 0x70) && (bank_nr <= 0x73))
		{
			return 0;
		}

		return snesRAM[address];
	}

	return snesRAM[address];
}

mmu::~mmu()
{
	delete(snesRAM);
}
