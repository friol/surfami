
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
				//glbTheLogger.logMsg("Data will be written to:" + sDMATargetAddr2 + " DMA bytes to move:" + std::to_string(byteCount)+ " DMA mode: "+std::to_string(dma_mode));
			}
			else
			{
				//glbTheLogger.logMsg("DMA will be written to:" + sDMATargetAddr + " DMA bytes to move:" + std::to_string(byteCount) + " DMA mode: " + std::to_string(dma_mode));
			}

			if (dma_mode == 0)
			{
				// 000 -> transfer 1 byte
				while (byteCount > 0)
				{
					if (!dma_dir)
					{
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
					targetAddr = (hiBank << 16) | (targetAddr & 0xffff);
				}

				snesRAM[0x4303 + (dmaChannel * 0x10)] = (targetAddr >> 8) & 0xff;
				snesRAM[0x4302 + (dmaChannel * 0x10)] = targetAddr & 0xff;

				snesRAM[0x4306 + (dmaChannel * 0x10)] = 0;
				snesRAM[0x4305 + (dmaChannel * 0x10)] = 0;
			}
			else if (dma_mode == 1)
			{
				//  001 -> transfer 2 bytes (xx, xx + 1) (e.g. VRAM)
				while (byteCount>0)
				{
					if (!dma_dir) 
					{
						write8(0x2100 + BBusAddr, read8(targetAddr));
						targetAddr += (dma_step == 0) ? 1 : ((dma_step == 2) ? -1 : 0);
						targetAddr = (hiBank << 16) | (targetAddr & 0xffff);
						if (!--byteCount) goto endDma1;
						write8(0x2100 + BBusAddr+1, read8(targetAddr));
						targetAddr += (dma_step == 0) ? 1 : ((dma_step == 2) ? -1 : 0);
						targetAddr = (hiBank << 16) | (targetAddr & 0xffff);
						if (!--byteCount) goto endDma1;
					}
					else 
					{
						write8(targetAddr, read8(0x2100 + BBusAddr));
						targetAddr += (dma_step == 0) ? 1 : ((dma_step == 2) ? -1 : 0);
						targetAddr = (hiBank << 16) | (targetAddr & 0xffff);
						if (!--byteCount) goto endDma1;
						write8(targetAddr, read8(0x2100 + BBusAddr + 1));
						targetAddr += (dma_step == 0) ? 1 : ((dma_step == 2) ? -1 : 0);
						targetAddr = (hiBank << 16) | (targetAddr & 0xffff);
						if (!--byteCount) goto endDma1;
					}
					snesRAM[0x4306 + (dmaChannel * 0x10)] = (unsigned char)(byteCount >> 8);
					snesRAM[0x4305 + (dmaChannel * 0x10)] = (unsigned char)(byteCount & 0xff);
				}

				endDma1:
				snesRAM[0x4306 + (dmaChannel * 0x10)] = 0;
				snesRAM[0x4305 + (dmaChannel * 0x10)] = 0;
				
				//snesRAM[0x4304 + (dmaChannel * 0x10)] = (targetAddr >> 16) & 0xff;
				snesRAM[0x4303 + (dmaChannel * 0x10)] = (targetAddr >> 8) & 0xff;
				snesRAM[0x4302 + (dmaChannel * 0x10)] = targetAddr & 0xff;
			}
			else if (dma_mode == 2)
			{
				//	002 -> transfer 2 bytes (xx, xx) (e.g. OAM / CGRAM)
				while (byteCount > 0)
				{
					if (!dma_dir)
					{
						write8(0x2100 + BBusAddr, read8(targetAddr));
						if (!--byteCount) goto endDma2;
						write8(0x2100 + BBusAddr, read8(targetAddr+1));
						if (!--byteCount) goto endDma2;
					}
					else
					{
						write8(targetAddr, read8(0x2100 + BBusAddr));
						if (!--byteCount) goto endDma2;
						write8(targetAddr, read8(0x2100 + BBusAddr + 1));
						if (!--byteCount) goto endDma2;
					}
					snesRAM[0x4306 + (dmaChannel * 0x10)] = (unsigned char)(byteCount >> 8);
					snesRAM[0x4305 + (dmaChannel * 0x10)] = (unsigned char)(byteCount & 0xff);
					targetAddr += (dma_step == 0) ? 2 : ((dma_step == 2) ? -2 : 0);
				}

				endDma2:
				snesRAM[0x4306 + (dmaChannel * 0x10)] = 0;
				snesRAM[0x4305 + (dmaChannel * 0x10)] = 0;

				snesRAM[0x4303 + (dmaChannel * 0x10)] = (targetAddr >> 8) & 0xff;
				snesRAM[0x4302 + (dmaChannel * 0x10)] = targetAddr & 0xff;
			}
			else if (dma_mode == 4)
			{
				//	4  =  Transfer 4 bytes   xx, xx+1, xx+2, xx+3  ;eg. for BGnSC, Window, APU..
				while (byteCount > 0)
				{
					if (!dma_dir)
					{
						write8(0x2100 + BBusAddr, read8(targetAddr));
						if (!--byteCount) goto endDma4;
						write8(0x2100 + BBusAddr+1, read8(targetAddr + 1));
						if (!--byteCount) goto endDma4;
						write8(0x2100 + BBusAddr+2, read8(targetAddr + 2));
						if (!--byteCount) goto endDma4;
						write8(0x2100 + BBusAddr+3, read8(targetAddr + 3));
						if (!--byteCount) goto endDma4;
					}
					else
					{
						write8(targetAddr, read8(0x2100 + BBusAddr));
						if (!--byteCount) goto endDma4;
						write8(targetAddr+1, read8(0x2100 + BBusAddr + 1));
						if (!--byteCount) goto endDma4;
						write8(targetAddr+2, read8(0x2100 + BBusAddr + 2));
						if (!--byteCount) goto endDma4;
						write8(targetAddr+3, read8(0x2100 + BBusAddr + 3));
						if (!--byteCount) goto endDma4;
					}
					snesRAM[0x4306 + (dmaChannel * 0x10)] = (unsigned char)(byteCount >> 8);
					snesRAM[0x4305 + (dmaChannel * 0x10)] = (unsigned char)(byteCount & 0xff);
					targetAddr += (dma_step == 0) ? 4 : ((dma_step == 2) ? -4 : 0);
				}

				endDma4:
				snesRAM[0x4306 + (dmaChannel * 0x10)] = 0;
				snesRAM[0x4305 + (dmaChannel * 0x10)] = 0;

				snesRAM[0x4303 + (dmaChannel * 0x10)] = (targetAddr >> 8) & 0xff;
				snesRAM[0x4302 + (dmaChannel * 0x10)] = targetAddr & 0xff;
			}
			else
			{
				glbTheLogger.logMsg("Error: unsupported dma mode " + std::to_string(dma_mode));
			}
		}
	}

	snesRAM[0x420b] = 0x00;
}

void mmu::resetHDMA(bool midFrame)
{
	midFrame = midFrame;

	bool hdmaEnabled = false;
	for (int i = 0; i < 8; i++) 
	{
		if (HDMAS[i].enabled) hdmaEnabled = true;
		HDMAS[i].doTransfer = false;
		HDMAS[i].terminated = false;
	}
	
	if (!hdmaEnabled) return;

	// init

	for (unsigned char dma_id = 0; dma_id < 8; dma_id++)
	{
		if (HDMAS[dma_id].enabled)
		{
			HDMAS[dma_id].aaddress = (snesRAM[0x4304 + (dma_id * 0x10)] << 16) | (snesRAM[0x4303 + (dma_id * 0x10)] << 8) | snesRAM[0x4302 + (dma_id * 0x10)];
			HDMAS[dma_id].address = HDMAS[dma_id].aaddress;
			HDMAS[dma_id].addressing_mode = (snesRAM[0x4300 + (dma_id * 0x10)] >> 6) & 1;
			HDMAS[dma_id].dma_mode = snesRAM[0x4300 + (dma_id * 0x10)] & 0b111;
			HDMAS[dma_id].indBank = snesRAM[0x4307 + (dma_id * 0x10)];
			HDMAS[dma_id].bAdr = snesRAM[0x4301 + (dma_id * 0x10)];
			HDMAS[dma_id].aBank = snesRAM[0x4304 + (dma_id * 0x10)];
			HDMAS[dma_id].fromB = (snesRAM[0x4300 + (dma_id * 0x10)]) & 0x80;
			HDMAS[dma_id].repCount = read8((HDMAS[dma_id].aBank << 16) | HDMAS[dma_id].address++);

			if (HDMAS[dma_id].repCount == 0) HDMAS[dma_id].terminated = true;

			if (HDMAS[dma_id].addressing_mode==1)
			{
				HDMAS[dma_id].size = read8((HDMAS[dma_id].aBank << 16) | HDMAS[dma_id].address++);
				HDMAS[dma_id].size |= read8((HDMAS[dma_id].aBank << 16) | HDMAS[dma_id].address++) << 8;
			}
			
			/*if (midFrame == false)*/ HDMAS[dma_id].doTransfer = true;
		}
	}
}

void mmu::dma_transferByte(unsigned short int aAdr, unsigned char aBank, unsigned char bAdr, bool fromB) 
{
	bool validB = !(bAdr == 0x80 && (aBank == 0x7e || aBank == 0x7f || 
		(
		(aBank < 0x40 || (aBank >= 0x80 && aBank < 0xc0)) && aAdr < 0x2000
		)));

	bool validA = !((aBank < 0x40 || (aBank >= 0x80 && aBank < 0xc0)) && (
		aAdr == 0x420b || aAdr == 0x420c || (aAdr >= 0x4300 && aAdr < 0x4380) || (aAdr >= 0x2100 && aAdr < 0x2200)
		));

	if (fromB) 
	{
		unsigned char val = validB ? read8(0x2100+bAdr) : 0x00;
		if (validA) write8((aBank << 16) | aAdr, val);
	}
	else 
	{
		unsigned char val = validA ? read8((aBank << 16) | aAdr) : 0x00;
		if (validB) write8(0x2100+bAdr, val);
	}
}

void mmu::executeHDMA()
{
	const int bAdrOffsets[8][4] = {
	  {0, 0, 0, 0},
	  {0, 1, 0, 1},
	  {0, 0, 0, 0},
	  {0, 0, 1, 1},
	  {0, 1, 2, 3},
	  {0, 1, 0, 1},
	  {0, 0, 0, 0},
	  {0, 0, 1, 1}
	};

	const int transferLength[8] = {
	  1, 2, 2, 4, 4, 4, 2, 4
	};

	bool hdmaActive = false;
	int lastActive = 0;
	for (int i = 0; i < 8; i++) 
	{
		if (HDMAS[i].enabled)
		{
			hdmaActive = true;
			if (!HDMAS[i].terminated) lastActive = i;
		}
	}

	if (!hdmaActive) return;

	for (int i = 0; i < 8; i++) 
	{
		if (HDMAS[i].enabled && (!HDMAS[i].terminated))
		{
			if (HDMAS[i].doTransfer)
			{
				//HDMAS[i].bAdr = snesRAM[0x4301 + (i * 0x10)];
				//HDMAS[i].aBank = snesRAM[0x4304 + (i * 0x10)];
				//HDMAS[i].fromB = (snesRAM[0x4300 + (i * 0x10)]) & 0x80;

				//HDMAS[i].addressing_mode = (snesRAM[0x4300 + (i * 0x10)] >> 6) & 1;
				//HDMAS[i].dma_mode = snesRAM[0x4300 + (i * 0x10)] & 0b111;
				//HDMAS[i].indBank = snesRAM[0x4307 + (i * 0x10)];
				//HDMAS[i].repCount = read8((HDMAS[dma_id].aBank << 16) | HDMAS[dma_id].address++);

				for (int j = 0; j < transferLength[HDMAS[i].dma_mode]; j++)
				{
					if (HDMAS[i].addressing_mode == 1)
					{
						dma_transferByte(
							(unsigned short int)(HDMAS[i].size++), HDMAS[i].indBank,
							(unsigned char)(HDMAS[i].bAdr + bAdrOffsets[HDMAS[i].dma_mode][j]), HDMAS[i].fromB
						);
					}
					else 
					{
						dma_transferByte(
							(unsigned short int)(HDMAS[i].address++), HDMAS[i].aBank,
							(unsigned char)(HDMAS[i].bAdr + bAdrOffsets[HDMAS[i].dma_mode][j]), HDMAS[i].fromB
						);
					}
				}
			}
		}
	}
	
	for (int i = 0; i < 8; i++) 
	{
		if (HDMAS[i].enabled && (!HDMAS[i].terminated))
		{
			HDMAS[i].repCount--;
			HDMAS[i].doTransfer = HDMAS[i].repCount & 0x80;

			unsigned char newRepCount = read8((HDMAS[i].aBank << 16) | HDMAS[i].address);
			if ((HDMAS[i].repCount & 0x7f) == 0) 
			{
				HDMAS[i].repCount = newRepCount;
				HDMAS[i].address++;
				if (HDMAS[i].addressing_mode==1)
				{
					if (HDMAS[i].repCount == 0 && i == lastActive)
					{
						HDMAS[i].size = 0;
					}
					else 
					{
						HDMAS[i].size = read8((HDMAS[i].aBank << 16) | HDMAS[i].address++);
					}
					HDMAS[i].size |= read8((HDMAS[i].aBank << 16) | HDMAS[i].address++) << 8;
				}
				if (HDMAS[i].repCount == 0) HDMAS[i].terminated = true;
				HDMAS[i].doTransfer = true;
			}
		}
	}
}

void mmu::write8(unsigned int address, unsigned char val)
{
	pPPU->openBus = val;

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
		else if (adr == 0x2106)
		{
			// 2106h - MOSAIC - Mosaic Size and Mosaic Enable (W)
			pPPU->writeMosaic(val);
			return;
		}
		else if (adr == 0x4200)
		{
			// NMITIMEN - Interrupt Enable and Joypad Request (W)
			//glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x4200 (NMITIMEN - Interrupt Enable and Joypad Request)");
			nmiTimen = val;
			vIrqEnabled= val & 0x20;
			hIrqEnabled= val & 0x10;
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
		else if (adr == 0x4207)
		{
			hIrqTimer = (hIrqTimer & 0x100) | val;
			return;
		}
		else if (adr == 0x4208)
		{
			hIrqTimer = (hIrqTimer & 0x0ff) | ((val & 1) << 8);
			return;
		}
		else if (adr == 0x4209)
		{
			vIrqTimer = (vIrqTimer & 0x100) | val;
			return;
		}
		else if (adr == 0x420a)
		{
			vIrqTimer = (vIrqTimer & 0x0ff) | ((val & 1) << 8);
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
			bool atLeastOne = false;
			for (int i = 0; i < 8; i++) 
			{
				HDMAS[i].hdmaEnable((val >> i) & 1);
				if ((val >> i) & 1) atLeastOne = true;
			}

			// this fixes the capcom games
			//int scanline = pPPU->getCurrentScanline();
			if (atLeastOne) resetHDMA(true);
			//if ((atLeastOne)&&(scanline>=240)) resetHDMA(false);

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
			//int bgid = adr - 0x2107 +1;
			//glbTheLogger.logMsg("Writing [" + strr.str() + "] for BG"+std::to_string(bgid)+" (BGx Screen Base and Screen Size)");
			pPPU->writeRegister(adr, val);
			return;
		}
		else if (adr == 0x2121)
		{
			//glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2121 (CGRAM index)");
			pPPU->writeRegister(0x2121, val);
			cgramAddress = val << 1;
			return;
		}
		else if (adr == 0x2122)
		{
			// write to cgram
			pPPU->writeRegister(0x2122, val);
			cgramAddress = (cgramAddress + 1) & (0x200 - 1);
			return;
		}
		else if (adr == 0x2123)
		{
			// Enable (ABCD) and Invert (abcd) windows for BG1 (AB) and BG2 (CD)
			pPPU->writeRegister(0x2123, val);
			return;
		}
		else if (adr == 0x2124)
		{
			// Enable (EFGH) and Invert (efgh) windows for BG3 (EF) and BG2 (GH)
			pPPU->writeRegister(0x2124, val);
			return;
		}
		else if (adr == 0x2125)
		{
			// WOBJSEL - Window Mask Settings for OBJ and Color Window ($2125 write)
			pPPU->writeRegister(0x2125, val);
			return;
		}
		else if (adr == 0x2126)
		{
			// window 1 left position
			pPPU->writeRegister(0x2126, val);
			return;
		}
		else if (adr == 0x2127)
		{
			// window 1 right position
			pPPU->writeRegister(0x2127, val);
			return;
		}
		else if (adr == 0x2128)
		{
			// window 2 left position
			pPPU->writeRegister(0x2128, val);
			return;
		}
		else if (adr == 0x2129)
		{
			// window 2 right position
			pPPU->writeRegister(0x2129, val);
			return;
		}
		else if (adr == 0x212d)
		{
			// write to sub screen designation 
			pPPU->writeRegister(0x212d, val);
			return;
			}
		else if (adr == 0x212e)
		{
			// main screen layer window enable
			pPPU->writeRegister(0x212e, val);
			return;
		}
		else if (adr == 0x212f)
		{
			// subscreen layer window enable
			pPPU->writeRegister(0x212f, val);
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
			//glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x210B (BG tile base address lower)");
			pPPU->writeRegister(0x210B, val);
			return;
		}
		else if (adr == 0x210C)
		{
			//glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x210C (BG tile base address upper)");
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
		else if (adr == 0x2130)
		{
			// 2130h - CGWSEL - Color Math Control Register A (W)
			pPPU->setCGWSEL(val);
			return;
		}
		else if (adr == 0x2131)
		{
			// 2131h - CGADSUB - Color Math Control Register B (W)
			pPPU->setCGADDSUB(val);
			return;
		}
		else if (adr == 0x2132)
		{
			// 2132h - COLDATA - Color Math Sub Screen Backdrop Color (W)
			pPPU->writeSubscreenFixedColor(val);
			return;
		}
		else if (adr == 0x2133)
		{
			// 2133h - SETINI - Display Control 2 (W)
			pPPU->setSETINI(val);
			return;
		}
		else if ((adr == 0x2140) || (adr == 0x2141) || (adr == 0x2142) || (adr == 0x2143))
		{
			//std::stringstream strstrVal;
			//strstrVal << std::hex << std::setw(2) << std::setfill('0') << (int)val;
			//glbTheLogger.logMsg("Write ["+strstrVal.str() + "] to APU port " + std::to_string(adr - 0x2140));
			pAPU->externalWrite8(adr, val);
			return;
		}
		else if (adr == 0x4016)
		{
			// 4016h/Write - JOYWR - Joypad Output (W)
			latchLine = val & 0x01;

			if (latchLine)
			{
				if (isKeyBPressed) latchedState |= 0x100;
				else latchedState &= ~0x100;
				if (isKeyYPressed) latchedState |= 0x200;
				else latchedState &= ~0x200;
				if (isKeyLPressed) latchedState |= 0x400;
				else latchedState &= ~0x400;
				if (isKeyRPressed) latchedState |= 0x800;
				else latchedState &= ~0x800;

				if (isKeyUpPressed) latchedState |= 0x10;
				else latchedState &= ~0x10;
				if (isKeyDownPressed) latchedState |= 0x20;
				else latchedState &= ~0x20;
				if (isKeyLeftPressed) latchedState |= 0x040;
				else latchedState &= ~0x40;
				if (isKeyRightPressed) latchedState |= 0x80;
				else latchedState &= ~0x80;
				
				if (isKeyAPressed) latchedState |= 0x01;
				else latchedState &= ~0x01;
				if (isKeyXPressed) latchedState |= 0x02;
				else latchedState &= ~0x02;

				if (isKeySelectPressed) latchedState |= 0x4;
				else latchedState &= ~0x4;
				if (isKeyStartPressed) latchedState |= 0x8;
				else latchedState &= ~0x8;
			}

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
			{			
				//	Division by zero, return 0xFFFF as Result, Remainder is Dividend
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
		else if (
					isHiRom && 
					hasSRAM &&
					( (((bank_nr >= 0x20) && (bank_nr <= 0x3f))) || (((bank_nr >= 0xa0) && (bank_nr <= 0xbf))) ) &&
					(adr>=0x6000)
					)
		{
			// SRAM
			//glbTheLogger.logMsg("Writing "+std::to_string(val)+" on SRAM addr "+std::to_string(adr));
			snesRAM[0x206000+((adr-0x6000)%sramSize)] = val;
			return;
		}

		snesRAM[adr] = val;
	}
	else
	{
		if (
			(!isHiRom) && 
			hasSRAM && 
			( ((bank_nr >= 0x70) && (bank_nr < 0x7e)) || (bank_nr>=0xf0) )
		   )
		{
			snesRAM[0x700000 | (adr % sramSize)] = val;
		}
		else snesRAM[address] = val;
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
			unsigned char openbus = 0;
			if(!(cgramAddress & 0x01)) openbus = pPPU->cgramRead(cgramAddress);
			else
			{
				openbus &= 0x80;
				openbus |= pPPU->cgramRead(cgramAddress) & 0x7f;
			}

			cgramAddress = (cgramAddress + 1) & (0x200 - 1);
			return openbus;
		}
		else if (adr == 0x2137)
		{
			// 2137h - SLHV - Latch H/V-Counter by Software (R)
			// TODO 
			pPPU->m_stat78 |= 0x40;
			//return (unsigned char)(rand() % 256);
			return pPPU->openBus;
		}
		else if (adr == 0x213c)
		{
			/* Horizontal counter data by ext/soft latch */
			//return (unsigned char)(rand()%256);
			int hPos = pPPU->internalCyclesCounter / 4;
			if (m_read_ophct)
			{
				pPPU->m_ppu2_open_bus &= 0xfe;
				pPPU->m_ppu2_open_bus |= (hPos >> 8) & 0x01;
			}
			else
			{
				pPPU->m_ppu2_open_bus = hPos & 0xff;
			}
			m_read_ophct ^= 1;
			return pPPU->m_ppu2_open_bus;
		}
		else if (adr == 0x213d)
		{
			// 213Dh - OPVCT - Vertical Counter Latch (R)
			// TODO
			//return (unsigned char)pPPU->getCurrentScanline();
			int curScanline= pPPU->getCurrentScanline();
			if (m_read_opvct)
			{
				pPPU->m_ppu2_open_bus &= 0xfe;
				pPPU->m_ppu2_open_bus |= (curScanline >> 8) & 0x01;
			}
			else
			{
				pPPU->m_ppu2_open_bus = curScanline & 0xff;
			}
			m_read_opvct ^= 1;
			return pPPU->m_ppu2_open_bus;
		}
		else if (adr == 0x213f)
		{
			m_read_opvct = false;
			m_read_ophct = false;

			unsigned char retval = 0x03;
			if (standard == 1) retval |= 0x10;

			pPPU->m_stat78 = (pPPU->m_stat78 & ~0x2f) | (pPPU->m_ppu2_open_bus & 0x20) | retval;
			pPPU->m_ppu2_open_bus = pPPU->m_stat78;

			return pPPU->m_ppu2_open_bus;
			//return retval;
		}
		else if ((adr == 0x2140) || (adr == 0x2141) || (adr == 0x2142) || (adr == 0x2143))
		{
			return pAPU->externalRead8(adr);
		}
		else if (adr == 0x4016)
		{
			// 4016h / Read - JOYA - Joypad Input Register A(R)
			// manual reading

			if (latchLine) 
			{
				if (isKeyBPressed) latchedState |= 0x100;
				else latchedState &= ~0x100;
				if (isKeyYPressed) latchedState |= 0x200;
				else latchedState &= ~0x200;
				if (isKeyLPressed) latchedState |= 0x400;
				else latchedState &= ~0x400;
				if (isKeyRPressed) latchedState |= 0x800;
				else latchedState &= ~0x800;

				if (isKeyUpPressed) latchedState |= 0x10;
				else latchedState &= ~0x10;
				if (isKeyDownPressed) latchedState |= 0x20;
				else latchedState &= ~0x20;
				if (isKeyLeftPressed) latchedState |= 0x040;
				else latchedState &= ~0x40;
				if (isKeyRightPressed) latchedState |= 0x80;
				else latchedState &= ~0x80;

				if (isKeyAPressed) latchedState |= 0x01;
				else latchedState &= ~0x01;
				if (isKeyXPressed) latchedState |= 0x02;
				else latchedState &= ~0x02;

				if (isKeySelectPressed) latchedState |= 0x4;
				else latchedState &= ~0x4;
				if (isKeyStartPressed) latchedState |= 0x8;
				else latchedState &= ~0x8;
			}

			unsigned char ret = latchedState & 1;
			latchedState >>= 1;
			latchedState |= 0x8000;

			ret |= (pPPU->openBus & 0xfc);

			if ((nmiTimen & 0x01) == 0x01)
			{
				return 0xff;
			}
			
			return ret;
		}
		else if (adr == 0x4017)
		{
			// 4017h/Read - JOYB - Joypad Input Register B (R)
			return 0xff;
		}
		else if (adr == 0x4200)
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
			unsigned char openBusSim = (rand() % 256) & 0x70;
			return (res << 7) | openBusSim | 0x62;
		}
		else if (adr == 0x4211)
		{
			//	PPU Interrupts - H/V-Timer IRQ Flag (R) [Read/Ack] - TODO
			unsigned char val = ((vIrqEnabled||hIrqEnabled) && irqTriggered) << 7;
			irqTriggered = false;
			return val | (pPPU->openBus & 0x7f);
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
		//else if ((adr == 0x421a)|| (adr == 0x421b))
		//{
		//	// 421Ah/421Bh - JOY2L/JOY2H - Joypad 2 (gameport 2, pin 4) (R) TODO
		//	return 0;
		//}
		else if ((adr == 0x430a) || (adr == 0x431a) || (adr == 0x432a) || (adr == 0x433a) || (adr == 0x434a) || (adr == 0x435a) || (adr == 0x436a) || (adr == 0x437a))
		{
			// 43xAh - NTRLx - HDMA Line - Counter(from current Table entry) (R / W)
			int hdmaChan = (adr & 0x70)>>4;
			return HDMAS[hdmaChan].repCount;
		}
		/*else if ((adr & 0xff0f) == 0x4300)
		{
			//glbTheLogger.logMsg("Warning: game reads from 0x43n0");
			unsigned char c = (adr & 0x70) >> 4;
			return snesRAM[0x4300 + (c * 0x10)];
		}
		else if ((adr & 0xff0f) == 0x4301)
		{
			//glbTheLogger.logMsg("Warning: game reads from 0x43n1");
			unsigned char c = (adr & 0x70) >> 4;
			return HDMAS[c].bAdr;
		}
		else if ((adr & 0xff0f) == 0x4302)
		{
			//glbTheLogger.logMsg("Warning: game reads from 0x43n2");
			unsigned char c = (adr & 0x70) >> 4;
			return HDMAS[c].aaddress & 0xff;
		}
		else if ((adr & 0xff0f) == 0x4303)
		{
			unsigned char c = (adr & 0x70) >> 4;
			return (unsigned char)(HDMAS[c].aaddress >> 8);
		}
		else if ((adr & 0xff0f) == 0x4304)
		{
			unsigned char c = (adr & 0x70) >> 4;
			return HDMAS[c].aBank;
		}
		else if ((adr & 0xff0f) == 0x4305)
		{
			unsigned char c = (adr & 0x70) >> 4;
			return HDMAS[c].size & 0xff;
		}
		else if ((adr & 0xff0f) == 0x4306)
		{
			unsigned char c = (adr & 0x70) >> 4;
			return (unsigned char)(HDMAS[c].size >> 8);
		}
		else if ((adr & 0xff0f) == 0x4307)
		{
			unsigned char c = (adr & 0x70) >> 4;
			return HDMAS[c].indBank;
		}*/
		//else if ((adr & 0xff0f) == 0x4308)
		//{
		//	unsigned char c = (adr & 0x70) >> 4;
		//	return HDMAS[c].tableAdr & 0xff;
		//}
		//else if ((adr & 0xff0f) == 0x4309)
		//{
		//	unsigned char c = (adr & 0x70) >> 4;
		//	return HDMAS[c].tableAdr >> 8;
		//}
		else if ((adr == 0x4214)|| (adr == 0x4215)|| (adr == 0x4216)|| (adr == 0x4217))
		{
			//4214h - RDDIVL - Unsigned Division Result(Quotient) (lower 8bit) (R)
			//4215h - RDDIVH - Unsigned Division Result(Quotient) (upper 8bit) (R)
			//4216h - RDMPYL - Unsigned Division Remainder / Multiply Product(lo.8bit) (R)
			//4217h - RDMPYH - Unsigned Division Remainder / Multiply Product(up.8bit) (R)
			return snesRAM[adr];
		}
		else if (
					isHiRom && 
					((((bank_nr >= 0x20) && (bank_nr <= 0x3f))) || (((bank_nr >= 0xa0) && (bank_nr <= 0xbf)))) &&
					hasSRAM &&
					(adr >= 0x6000))
		{
			// SRAM
			//unsigned char res = snesRAM[(bank_nr << 16) | (adr % sramSize)];
			//glbTheLogger.logMsg("Returning ["+std::to_string(res) + "] for SRAM");
			//return snesRAM[(bank_nr << 16) | (adr % sramSize)];
			return snesRAM[0x206000+((adr-0x6000) % sramSize)];
		}
		else if (adr >= 0x2100 && adr < 0x2200)
		{
			return pPPU->openBus;
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
			if ((bank_nr >= 0x70) && (bank_nr < 0x7e))
			{
				return 0;
			}
		}
		else if ((!isHiRom) && (hasSRAM))
		{
			if ( ((bank_nr >= 0x70) && (bank_nr < 0x7e)) || (bank_nr>=0xf0) ) 
			{
				return snesRAM[0x700000 | (adr % sramSize)];
			}
			else return snesRAM[address];
		}

		return snesRAM[address];
	}

	//return snesRAM[address];
}

mmu::~mmu()
{
	// save SRAM if present
	if (hasSRAM)
	{
		std::fstream fout(sramFileName, std::fstream::out | std::fstream::binary);
		if (!isHiRom)
		{
			for (unsigned int b = 0;b < sramSize;b++)
			{
				unsigned char theByte = read8(0x700000 + b);
				fout.write(reinterpret_cast<char*>(&theByte), 1);
			}
		}
		else
		{
			for (unsigned int b = 0;b < sramSize;b++)
			{
				unsigned char theByte = read8(0x206000 + b);
				fout.write(reinterpret_cast<char*>(&theByte), 1);
			}
		}
	}

	delete(snesRAM);
}
