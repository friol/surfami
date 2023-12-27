
/* surFami emu - BUS, DMAs, etc. */

#include "mmu.h"
#include "logger.h"

extern logger glbTheLogger;

//

mmu::mmu(ppu& thePPU)
{
	pPPU = &thePPU;
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


		}
	}
}

void mmu::write8(unsigned int address, unsigned char val)
{
	if (address == 0x420B) // DMA start reg
	{
		DMAstart(val);
	}
	else if (address == 0x2105)
	{
		glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2105 (BG Mode and Character Size Register)");
		pPPU->writeRegister(0x2105, val);
	}
	else if (address == 0x2121)
	{
		glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2121 (CGRAM index)");
		pPPU->writeRegister(0x2121, val);
	}
	else if (address == 0x2122)
	{
		//glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2122 (CGRAM write reg)");
		pPPU->writeRegister(0x2122, val);
	}
	else if (address == 0x2115)
	{
		glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2115 (VRAM Address Increment Mode)");
		pPPU->writeRegister(0x2115, val);
	}
	else if (address == 0x2116)
	{
		glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2116 (VRAM Address Lower)");
		pPPU->writeRegister(0x2116, val);
	}
	else if (address == 0x2117)
	{
		glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2117 (VRAM Address Upper)");
		pPPU->writeRegister(0x2117, val);
	}
	else if (address == 0x2118)
	{
		//glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2118 (VRAM data write lower 8-bit)");
		pPPU->writeRegister(0x2118, val);
	}
	else if (address == 0x2119)
	{
		//glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x2118 (VRAM data write upper 8-bit)");
		pPPU->writeRegister(0x2119, val);
	}
	else if (address == 0x210B)
	{
		glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x210B (BG tile base address lower)");
		pPPU->writeRegister(0x210B, val);
	}
	else if (address == 0x210C)
	{
		glbTheLogger.logMsg("Writing [" + std::to_string(val) + "] to 0x210C (BG tile base address upper)");
		pPPU->writeRegister(0x210C, val);
	}



	snesRAM[address] = val;
}

unsigned char mmu::read8(unsigned int address)
{
	return snesRAM[address];
}

mmu::~mmu()
{
}
