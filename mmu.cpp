
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

	for (int i = 0; i < 8; i++)
	{
		HDMAS[i].hdmaEnable(0);
	}
}

void mmu::DMAstart(unsigned char val)
{
	// DMA start - bit set indicates which of the 8 DMA channels to start
	unsigned char dmaMask = val;

	std::stringstream strr;
	strr << std::hex << std::setw(2) << std::setfill('0') << (int)dmaMask;
	std::string sDMAmask = strr.str();

	//glbTheLogger.logMsg("DMA start called. DMA channels:"+sDMAmask);

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

			unsigned char hiBank = snesRAM[0x4304 + (dmaChannel * 0x10)];
			unsigned char hiAddr = snesRAM[0x4303 + (dmaChannel * 0x10)];
			unsigned char loAddr = snesRAM[0x4302 + (dmaChannel * 0x10)];

			unsigned int targetAddr = (hiBank << 16) | (hiAddr << 8) | loAddr;
			unsigned int byteCount = (snesRAM[0x4306 + (dmaChannel * 0x10)] << 8) | snesRAM[0x4305 + (dmaChannel * 0x10)];

			if (byteCount == 0)
			{
				byteCount = 0x10000;
			}

			std::stringstream strstrdgb;
			strstrdgb << std::hex << std::setw(4) << std::setfill('0') << (int)targetAddr;
			std::string sDMATargetAddr = strstrdgb.str();
			std::stringstream strstrdgb2;
			strstrdgb2 << std::hex << std::setw(4) << std::setfill('0') << (int)(0x2100 + BBusAddr);
			std::string sDMATargetAddr2 = strstrdgb2.str();

			//glbTheLogger.logMsg("DMA chan:" + std::to_string(dmaChannel)+ " DMA mode:"+std::to_string(dma_mode)+" DMA direction:"+std::to_string(dma_dir));

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
				//while (((snesRAM[0x4306 + (dmaChannel * 0x10)] << 8) | snesRAM[0x4305 + (dmaChannel * 0x10)]) > 0)
				while (byteCount > 0)
				{
					if (!dma_dir)
					{
						//write8(0x2100 + BBusAddr, snesRAM[targetAddr]);
						write8(0x2100 + BBusAddr, read8(targetAddr));
					}
					else
					{
						write8(targetAddr,read8(0x2100 + BBusAddr));
					}

					byteCount--;
					snesRAM[0x4306 + (dmaChannel * 0x10)] = (unsigned char)(byteCount >> 8);
					snesRAM[0x4305 + (dmaChannel * 0x10)] = (unsigned char)(byteCount & 0xff);
					targetAddr += (dma_step == 0) ? 1 : ((dma_step == 2) ? -1 : 0);
				}

				snesRAM[0x4306 + (dmaChannel * 0x10)] = 0;
				snesRAM[0x4305 + (dmaChannel * 0x10)] = 0;
			}
			else if (dma_mode == 1)
			{
				//  001 -> transfer 2 bytes (xx, xx + 1) (e.g. VRAM)
				//while (((snesRAM[0x4306 + (dmaChannel * 0x10)] << 8) | snesRAM[0x4305 + (dmaChannel * 0x10)]) > 0)
				while (byteCount>0)
				{
					if (!dma_dir) 
					{
						write8(0x2100 + BBusAddr, read8(targetAddr));
						if (!--byteCount) return;
						write8(0x2100 + BBusAddr+1, read8(targetAddr+1));
						if (!--byteCount) return;
					}
					else 
					{
						write8(targetAddr, read8(0x2100 + BBusAddr));
						if (!--byteCount) return;
						write8(targetAddr + 1, read8(0x2100 + BBusAddr + 1));
						if (!--byteCount) return;
					}
					snesRAM[0x4306 + (dmaChannel * 0x10)] = (unsigned char)(byteCount >> 8);
					snesRAM[0x4305 + (dmaChannel * 0x10)] = (unsigned char)(byteCount & 0xff);
					targetAddr += (dma_step == 0) ? 2 : ((dma_step == 2) ? -2 : 0);
				}

				snesRAM[0x4306 + (dmaChannel * 0x10)] = 0;
				snesRAM[0x4305 + (dmaChannel * 0x10)] = 0;
			}
			else if (dma_mode == 2)
			{
				//	002 -> transfer 2 bytes (xx, xx) (e.g. OAM / CGRAM)
				//while (((snesRAM[0x4306 + (dmaChannel * 0x10)] << 8) | snesRAM[0x4305 + (dmaChannel * 0x10)]) > 0)
				while (byteCount > 0)
				{
					if (!dma_dir)
					{
						//write8(0x2100 + BBusAddr, snesRAM[targetAddr]);
						write8(0x2100 + BBusAddr, read8(targetAddr));
						if (!--byteCount) return;
						//write8(0x2100 + BBusAddr, snesRAM[targetAddr + 1]);
						write8(0x2100 + BBusAddr, read8(targetAddr+1));
						if (!--byteCount) return;
					}
					else
					{
						write8(targetAddr, read8(0x2100 + BBusAddr));
						if (!--byteCount) return;
						write8(targetAddr, read8(0x2100 + BBusAddr + 1));
						if (!--byteCount) return;
					}
					snesRAM[0x4306 + (dmaChannel * 0x10)] = (unsigned char)(byteCount >> 8);
					snesRAM[0x4305 + (dmaChannel * 0x10)] = (unsigned char)(byteCount & 0xff);
					targetAddr += (dma_step == 0) ? 2 : ((dma_step == 2) ? -2 : 0);
				}

				snesRAM[0x4306 + (dmaChannel * 0x10)] = 0;
				snesRAM[0x4305 + (dmaChannel * 0x10)] = 0;
			}
			else
			{
				glbTheLogger.logMsg("Error: unsupported dma mode " + std::to_string(dma_mode));
			}
		}
	}

	snesRAM[0x420b] = 0x00;
}

void mmu::mmuDMATransfer(unsigned char dma_mode, unsigned char dma_dir, unsigned char dma_step,unsigned int& cpu_address, unsigned char io_address) 
{
	switch (dma_mode) 
	{
	case 0: 
	{						//	transfer 1 byte (e.g. WRAM)
		if (!dma_dir)
			write8(0x2100 + io_address, read8(cpu_address));
		else
			write8(cpu_address, read8(0x2100 + io_address));
		cpu_address += (dma_step == 0) ? 1 : ((dma_step == 2) ? -1 : 0);
		break;
	}
	case 1:							//	transfer 2 bytes (xx, xx + 1) (e.g. VRAM)
		if (!dma_dir) 
		{
			write8(0x2100 + io_address, read8(cpu_address));
			write8(0x2100 + io_address + 1, read8(cpu_address + 1));
		}
		else 
		{
			write8(cpu_address,read8(0x2100 + io_address));
			write8(cpu_address + 1, read8(0x2100 + io_address + 1));
		}
		cpu_address += (dma_step == 0) ? 2 : ((dma_step == 2) ? -2 : 0);
		break;
	case 2:							//	transfer 2 bytes (xx, xx) (e.g. OAM / CGRAM)
		if (!dma_dir) 
		{
			write8(0x2100 + io_address, read8(cpu_address));
			write8(0x2100 + io_address, read8(cpu_address + 1));
		}
		else 
		{
			write8(cpu_address, read8(0x2100 + io_address));
			write8(cpu_address, read8(0x2100 + io_address + 1));
		}
		cpu_address += (dma_step == 0) ? 2 : ((dma_step == 2) ? -2 : 0);
		break;
	case 3:							//	transfer 4 bytes (xx, xx, xx + 1, xx + 1) (e.g. BGnxOFX, M7x)
		if (!dma_dir) 
		{
			write8(0x2100 + io_address, read8(cpu_address));
			write8(0x2100 + io_address, read8(cpu_address + 1));
			write8(0x2100 + io_address + 1, read8(cpu_address + 2));
			write8(0x2100 + io_address + 1, read8(cpu_address + 3));
		}
		else 
		{
			write8(cpu_address, read8(0x2100 + io_address));
			write8(cpu_address, read8(0x2100 + io_address + 1));
			write8(cpu_address + 1, read8(0x2100 + io_address + 2));
			write8(cpu_address + 1, read8(0x2100 + io_address + 3));
		}
		cpu_address += (dma_step == 0) ? 4 : ((dma_step == 2) ? -4 : 0);
		break;
	case 4:							//	transfer 4 bytes (xx, xx + 1, xx + 2, xx + 3) (e.g. BGnSC, Window, APU...)
		if (!dma_dir) 
		{
			write8(0x2100 + io_address, read8(cpu_address));
			write8(0x2100 + io_address + 1, read8(cpu_address + 1));
			write8(0x2100 + io_address + 2, read8(cpu_address + 2));
			write8(0x2100 + io_address + 3, read8(cpu_address + 3));
		}
		else 
		{
			write8(cpu_address, read8(0x2100 + io_address));
			write8(cpu_address + 1, read8(0x2100 + io_address + 1));
			write8(cpu_address + 2, read8(0x2100 + io_address + 2));
			write8(cpu_address + 2, read8(0x2100 + io_address + 3));
		}
		cpu_address += (dma_step == 0) ? 4 : ((dma_step == 2) ? -4 : 0);
		break;
	case 5:					//	transfer 4 bytes (xx, xx + 1, xx, xx + 1) - RESERVED
		break;
	case 6:					//	same as mode 2 - RESERVED
		break;
	case 7:					//	same as mode 3 - RESERVED
		break;
	default:
		break;
	}
}

void mmu::resetHDMA()
{
	for (unsigned char dma_id = 0; dma_id < 8; dma_id++)
	{
		if (HDMAS[dma_id].enabled)
		{
			HDMAS[dma_id].terminated = false;
			HDMAS[dma_id].IO = snesRAM[0x4301 + (dma_id * 0x10)];
			HDMAS[dma_id].addressing_mode = (snesRAM[0x4300 + (dma_id * 0x10)] >> 6) & 1;
			HDMAS[dma_id].direction = snesRAM[0x4300 + (dma_id * 0x10)] >> 7;
			HDMAS[dma_id].dma_mode = snesRAM[0x4300 + (dma_id * 0x10)] & 0b111;
			
			HDMAS[dma_id].indirect_address = (snesRAM[0x4307 + (dma_id * 0x10)] << 16) | (snesRAM[0x4306 + (dma_id * 0x10)] << 8) | snesRAM[0x4305 + (dma_id * 0x10)];
			HDMAS[dma_id].aaddress = (snesRAM[0x4304 + (dma_id * 0x10)] << 16) | (snesRAM[0x4303 + (dma_id * 0x10)] << 8) | snesRAM[0x4302 + (dma_id * 0x10)];
			HDMAS[dma_id].address = HDMAS[dma_id].aaddress;

			//HDMAS[dma_id].repeat = snesRAM[HDMAS[dma_id].address] >> 7;
			HDMAS[dma_id].repeat = snesRAM[0x430a + (dma_id * 0x10)] >> 7;

			HDMAS[dma_id].line_counter = snesRAM[HDMAS[dma_id].address] & 0x7f;

			//	initial transfer
			HDMAS[dma_id].address++;

			//	0 - Direct, 1 - Indirect
			if (HDMAS[dma_id].addressing_mode)
			{
				HDMAS[dma_id].indirect_address = (snesRAM[HDMAS[dma_id].address + 1] << 8) | snesRAM[HDMAS[dma_id].address];
				HDMAS[dma_id].address += 2;
				mmuDMATransfer(HDMAS[dma_id].dma_mode, HDMAS[dma_id].direction, 0, HDMAS[dma_id].indirect_address, HDMAS[dma_id].IO);
			}
			else
			{
				mmuDMATransfer(HDMAS[dma_id].dma_mode, HDMAS[dma_id].direction, 0, HDMAS[dma_id].address, HDMAS[dma_id].IO);
			}
		}
	}
}

void mmu::startHDMA()
{
	for (unsigned char dma_id = 0; dma_id < 8; dma_id++)
	{
		if (HDMAS[dma_id].enabled && !HDMAS[dma_id].terminated)
		{
			if (HDMAS[dma_id].line_counter-- == 0)
			{
				if (snesRAM[HDMAS[dma_id].address] == 0x00)
				{
					HDMAS[dma_id].terminated = true;
					goto nextHDMA;
				}
				
				//HDMAS[dma_id].repeat = snesRAM[HDMAS[dma_id].address] >> 7;
				HDMAS[dma_id].repeat = snesRAM[0x430a + (dma_id * 0x10)] >> 7;
				
				HDMAS[dma_id].line_counter = snesRAM[HDMAS[dma_id].address] & 0x7f;
				HDMAS[dma_id].address++;

				if (HDMAS[dma_id].addressing_mode)
				{		//	0 - Direct, 1 - Indirect
					HDMAS[dma_id].indirect_address = (snesRAM[HDMAS[dma_id].address + 1] << 8) | snesRAM[HDMAS[dma_id].address];
					HDMAS[dma_id].address += 2;
				}

				if (HDMAS[dma_id].addressing_mode)
				{		//	0 - Direct, 1 - Indirect
					mmuDMATransfer(HDMAS[dma_id].dma_mode, HDMAS[dma_id].direction, 0, HDMAS[dma_id].indirect_address, HDMAS[dma_id].IO);
					goto nextHDMA;
				}
				else
				{
					mmuDMATransfer(HDMAS[dma_id].dma_mode, HDMAS[dma_id].direction, 0, HDMAS[dma_id].address, HDMAS[dma_id].IO);
					goto nextHDMA;
				}
			}

			if (HDMAS[dma_id].repeat)
			{
				if (HDMAS[dma_id].addressing_mode)
				{
					//	0 - Direct, 1 - Indirect
					mmuDMATransfer(HDMAS[dma_id].dma_mode, HDMAS[dma_id].direction, 0, HDMAS[dma_id].indirect_address, HDMAS[dma_id].IO);
				}
				else
				{
					mmuDMATransfer(HDMAS[dma_id].dma_mode, HDMAS[dma_id].direction, 0, HDMAS[dma_id].address, HDMAS[dma_id].IO);
				}
			}
		}

		nextHDMA:
		int nxt = 1; nxt = 2;
	}
}

void mmu::write8(unsigned int address, unsigned char val)
{
	std::stringstream strr;
	strr << std::hex << std::setw(2) << std::setfill('0') << (int)val;

	unsigned char bank_nr = (unsigned char)(address >> 16);
	unsigned short int adr = address & 0xffff;

	if ( (adr < 0x8000) && ((bank_nr < 0x40) || (bank_nr >= 0x80 && bank_nr < 0xc0)) )
	{
		//	WRAM
		if (adr < 0x2000)
		{
			snesRAM[0x7e0000 + adr] = val;
			//snesRAM[adr] = val;
			//return;
		}
		else if (adr == 0x2100)
		{
			// 2100h - INIDISP - Display Control 1
			//glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2100 (INIDISP - Display Control 1)");
			pPPU->setINIDISP(val);
			return;
		}
		else if (adr == 0x2101)
		{
			// 2101h - OBSEL - Object Size and Object Base
			//glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2101 (OBSEL - Object Size and Object Base)");
			pPPU->setOBSEL(val);
			return;
		}
		else if (adr == 0x2102)
		{
			// 2102h/2103h - OAMADDL/OAMADDH - OAM Address and Priority Rotation (W)
			pPPU->writeOAMAddressLow(val);
			return;
		}
		else if (adr == 0x2103)
		{
			// 2102h/2103h - OAMADDL/OAMADDH - OAM Address and Priority Rotation (W)
			pPPU->writeOAMAddressHigh(val);
			return;
		}
		else if (adr == 0x2104)
		{
			// 2104h - OAMDATA - OAM Data Write(W)
			pPPU->writeOAM(val);
			return;
		}
		else if (adr == 0x4200)
		{
			// NMITIMEN - Interrupt Enable and Joypad Request (W)
			glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x4200 (NMITIMEN - Interrupt Enable and Joypad Request)");
			nmiTimen = val;
			return;
		}
		else if (adr == 0x2180)
		{
			// WMDATA - WRAM Data Read/Write (R/W)
			unsigned int waddr = (wram281x[0] | (wram281x[1] << 8) | (wram281x[2] << 16)) & 0x1ffff;
			snesRAM[0x7e0000 + waddr] = val;
			//snesRAM[waddr] = val;
			waddr += 1; waddr &= 0x1ffff;
			wram281x[0] = waddr & 0xff;
			wram281x[1] = (waddr>>8) & 0xff;
			wram281x[2] = (waddr>>16) & 0xff;
			return;
		}
		else if ((adr == 0x2181) || (adr == 0x2182) || (adr == 0x2183))
		{
			unsigned int byte = adr - 0x2181;
			wram281x[byte] = val;
			return;
		}
		else if (adr == 0x420B) 
		{
			// DMA start reg
			DMAstart(val);
			return;
		}
		else if (adr == 0x420C)
		{
			// HDMA start reg
			for (int i = 0; i < 8; i++) 
			{
				HDMAS[i].hdmaEnable((val >> i) & 1);
			}
			return;
		}
		else if (adr == 0x2105)
		{
			//glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2105 (BG Mode and Character Size Register)");
			pPPU->writeRegister(0x2105, val);
			return;
		}
		else if ((adr == 0x2107) || (adr == 0x2108) || (adr == 0x2109) || (adr == 0x210A))
		{
			int bgid = adr - 0x2107 +1;
			glbTheLogger.logMsg("Writing [" + strr.str() + "] for BG"+std::to_string(bgid)+" (BGx Screen Base and Screen Size)");
			pPPU->writeRegister(adr, val);
			return;
		}
		else if (adr == 0x2121)
		{
			//glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2121 (CGRAM index)");
			pPPU->writeRegister(0x2121, val);
			return;
		}
		else if (adr == 0x2122)
		{
			// write to cgram
			pPPU->writeRegister(0x2122, val);
			return;
		}
		else if (adr == 0x212d)
		{
			// write to sub screen designation 
			pPPU->writeRegister(0x212d, val);
			return;
		}
		else if (adr == 0x2115)
		{
			//glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2115 (VRAM Address Increment Mode)");
			pPPU->writeRegister(0x2115, val);
			return;
		}
		else if (adr == 0x2116)
		{
			//glbTheLogger.logMsg("Writing [" + strr.str() + "] to 0x2116 (VRAM Address Lower)");
			pPPU->writeRegister(0x2116, val);
			return;
		}
		else if (adr == 0x2117)
		{
			//glbTheLogger.logMsg("Writing [" + strr.str() + "] to 0x2117 (VRAM Address Upper)");
			pPPU->writeRegister(0x2117, val);
			return;
		}
		else if (adr == 0x2118)
		{
			//glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2118 (VRAM data write lower 8-bit)");
			pPPU->writeRegister(0x2118, val);
			return;
		}
		else if (adr == 0x2119)
		{
			//glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2118 (VRAM data write upper 8-bit)");
			pPPU->writeRegister(0x2119, val);
			return;
		}
		else if (adr == 0x211a)
		{
			// 211Ah - M7SEL   - Rotation/Scaling Mode Settings
			pPPU->writeM7SEL(val);
			return;
		}
		else if ((adr == 0x211b) || (adr == 0x211c) || (adr == 0x211d) || (adr == 0x211e)|| (adr==0x211f)|| (adr==0x2120))
		{
			pPPU->writeM7Matrix(adr - 0x211b, val);
			return;
		}
		else if (adr == 0x210B)
		{
			glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x210B (BG tile base address lower)");
			pPPU->writeRegister(0x210B, val);
			return;
		}
		else if (adr == 0x210C)
		{
			glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x210C (BG tile base address upper)");
			pPPU->writeRegister(0x210C, val);
			return;
		}
		else if (adr == 0x212c)
		{
			//glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x212C (TM - Main Screen Designation)");
			pPPU->writeRegister(adr, val);
			return;
		}
		else if ((adr == 0x210D) || (adr == 0x210F) || (adr == 0x2111) || (adr == 0x2113))
		{
			int bgid = (adr - 0x210D)>>1;
			//if (bgid==0) glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to BG "+std::to_string(bgid) + " scroll X");
			pPPU->writeBgScrollX(bgid, val);
			if (adr == 0x210d)
			{
				pPPU->writeM7HOFS(val);
			}
			return;
		}
		else if ((adr == 0x210E) || (adr == 0x2110) || (adr == 0x2112) || (adr == 0x2114))
		{
			int bgid = (adr - 0x210E)>>1;
			//if (bgid == 0) glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to BG " + std::to_string(bgid) + " scroll Y");
			pPPU->writeBgScrollY(bgid, val);
			if (adr == 0x210e)
			{
				pPPU->writeM7VOFS(val);
			}
			return;
		}
		else if ((adr == 0x2140) || (adr == 0x2141) || (adr == 0x2142) || (adr == 0x2143))
		{
			pAPU->write8(adr, val);
			return;
		}
		else if (adr == 0x4202)			//	CPU MATH - WRMPYA - Multiplicand
		{
			snesRAM[adr] = val;
			return;
		}
		else if (adr == 0x4203)			//	CPU MATH - WRMPYB - Multiplier & Start Multiplication
		{
			snesRAM[adr] = val;
			snesRAM[0x4216] = (val * snesRAM[0x4202]) & 0xff;
			snesRAM[0x4217] = (val * snesRAM[0x4202]) >> 8;
			snesRAM[0x4214] = val;
			snesRAM[0x4215] = 0x00;
			return;
		}
		else if (adr == 0x4204)			//	CPU MATH - WRDIVL - 16 Bit Dividend (lower 8 Bit)
		{
			snesRAM[adr] = val;
			return;
		}
		else if (adr == 0x4205)			//	CPU MATH - WRDIVH - 16 Bit Dividend (upper 8 Bit)
		{
			snesRAM[adr] = val;
			return;
		}
		else if (adr == 0x4206)			//	CPU MATH - WRDIVB - 8 Bit Divisor & Start Division
		{
			snesRAM[adr] = val;
			if (!val) 
			{			//	Division by zero, return 0xFFFF as Result, Remainder is Dividend
				snesRAM[0x4214] = 0xff;
				snesRAM[0x4215] = 0xff;
				snesRAM[0x4216] = snesRAM[0x4204];
				snesRAM[0x4217] = snesRAM[0x4205];
				return;
			}
			snesRAM[0x4214] = (unsigned short int)(((snesRAM[0x4205] << 8) | snesRAM[0x4204]) / val) & 0xff;
			snesRAM[0x4215] = (unsigned short int)(((snesRAM[0x4205] << 8) | snesRAM[0x4204]) / val) >> 8;
			snesRAM[0x4216] = (unsigned short int)(((snesRAM[0x4205] << 8) | snesRAM[0x4204]) % val) & 0xff;
			snesRAM[0x4217] = (unsigned short int)(((snesRAM[0x4205] << 8) | snesRAM[0x4204]) % val) >> 8;
			return;
		}
		else if (isHiRom && ((bank_nr >= 0x30) && (bank_nr < 0x3f)))
		{
			// SRAM
			snesRAM[adr] = val;
			return;
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
	unsigned char bank_nr = (unsigned char)(address >> 16);
	unsigned short int adr = address & 0xffff;

	if ((adr < 0x8000) && ((bank_nr < 0x40) || (bank_nr >= 0x80 && bank_nr < 0xc0)))
	{
		//	WRAM
		if (adr < 0x2000)
		{
			return snesRAM[0x7e0000 + adr];
		}
		else if (adr == 0x2180)
		{
			// WMDATA - WRAM Data Read/Write (R/W)
			unsigned int waddrRet = wram281x[0] | (wram281x[1] << 8) | (wram281x[2] << 16) & 0x1ffff;
			unsigned int waddr = waddrRet;
			waddr += 1; waddr &= 0x1ffff;
			wram281x[0] = waddr & 0xff;
			wram281x[1] = (waddr >> 8) & 0xff;
			wram281x[2] = (waddr >> 16) & 0xff;
			return snesRAM[0x7e0000 + waddrRet];
		}
		else if ((adr == 0x2134) || (adr == 0x2135) || (adr == 0x2136))
		{
			return pPPU->getMPY(adr);
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
			return pPPU->vmDataRead(adr);
		}
		else if (adr == 0x213b)
		{
			//	PPU - CGDATA - Palette CGRAM Data Read (R)
			//return PPU_readCGRAM(memory[0x2121]);
			glbTheLogger.logMsg("Error: reading from 213b (CGRAM)");
		}
		else if (adr == 0x213f)
		{
			unsigned char retval = 0x03;
			if (standard == 1) retval |= 0x10;
			return retval;
		}
		else if ((adr == 0x2140) || (adr == 0x2141) || (adr == 0x2142) || (adr == 0x2143))
		{
			return pAPU->read8(adr);
		}
		else if (adr == 0x4016)
		{
			// 4016h / Read - JOYA - Joypad Input Register A(R)
			// manual reading

			return 0x00;
		}
		else if (adr==0x4200) 
		{
			return (nmiFlag << 7);
		}
		else if (adr == 0x4210)
		{
			bool res = nmiFlag;
			if (res)
			{
				nmiFlag = false;
			}
			return (res << 7) | 0x62;
		}
		else if (adr == 0x4211)
		{
			//	PPU Interrupts - H/V-Timer IRQ Flag (R) [Read/Ack] - TODO
			return 0;
		}
		else if (adr == 0x4212)
		{
			//	PPU Interrupts - H/V-Blank Flag and Joypad Busy Flag (R) - TODO
			unsigned char res = 0;
			if (pPPU->isVBlankActive()) res |= 0x80;
			int intcyc = pPPU->getInternalCyclesCounter();

			if (/*(intcyc<1) ||*/ (intcyc>(1096 / 6))) res |= 0x40;

			return res;
		}
		else if (adr == 0x4017)
		{
			// 4017h/Read - JOYB - Joypad Input Register B (R) TODO
			return 0x1c;
		}
		else if (adr == 0x4218)
		{
			unsigned char res = 0;
			if (isKeyAPressed)
			{
				res |= 0x80;
			}
			if (isKeyXPressed)
			{
				res |= 0x40;
			}
			if (isKeyLPressed)
			{
				res |= 0x20;
			}
			if (isKeyRPressed)
			{
				res |= 0x10;
			}

			return res;
		}
		else if (adr == 0x4219)
		{
			unsigned char res = 0;
			if (isKeySelectPressed)
			{
				res |= 0x20;
			}
			if (isKeyStartPressed)
			{
				res |= 0x10;
			}
			if (isKeyRightPressed)
			{
				res |= 0x01;
			}
			if (isKeyLeftPressed)
			{
				res |= 0x02;
			}
			if (isKeyDownPressed)
			{
				res |= 0x04;
			}
			if (isKeyUpPressed)
			{
				res |= 0x08;
			}
			if (isKeyBPressed)
			{
				res |= 0x80;
			}
			if (isKeyYPressed)
			{
				res |= 0x40;
			}

			return res;
		}
		else if ((adr == 0x421a)|| (adr == 0x421b))
		{
			// 421Ah/421Bh - JOY2L/JOY2H - Joypad 2 (gameport 2, pin 4) (R) TODO
			return 0;
		}
		else if ((adr == 0x4214)|| (adr == 0x4215)|| (adr == 0x4216)|| (adr == 0x4217))
		{
			//4214h - RDDIVL - Unsigned Division Result(Quotient) (lower 8bit) (R)
			//4215h - RDDIVH - Unsigned Division Result(Quotient) (upper 8bit) (R)
			//4216h - RDMPYL - Unsigned Division Remainder / Multiply Product(lo.8bit) (R)
			//4217h - RDMPYH - Unsigned Division Remainder / Multiply Product(up.8bit) (R)
			return snesRAM[adr];
		}
		else if (isHiRom && ((bank_nr >= 0x30) && (bank_nr < 0x3f)))
		{
			// SRAM
			return snesRAM[adr];
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
		if ((!isHiRom) && (!hasSRAM))
		{
			if ((bank_nr >= 0x70) && (bank_nr <= 0x77))
			{
				return 0;
			}
			else return snesRAM[address];
		}

		return snesRAM[address];
	}

	return snesRAM[address];
}

mmu::~mmu()
{
	// save SRAM if present
	if (hasSRAM)
	{
		std::fstream fout(sramFileName, std::fstream::out | std::fstream::binary);
		for (int b = 0;b < 0x800;b++)
		{
			unsigned char theByte = read8(0x700000 + b);
			fout.write(reinterpret_cast<char*>(&theByte), 1);
		}
	}

	delete(snesRAM);
}
