
#include "apu.h"
#include <cstdlib> 

apu::apu()
{
	/*for (int i = 0;i < 256;i++)
	{
		mockRetValues[i] = i;
	}*/
}

unsigned char apu::read8(int address)
{
	if (address == 0x2140)
	{
		// 2140h - APUI00 - Main CPU to Sound CPU Communication Port 0 (R / W)
		unsigned char retval;
		
		if (mockIdxPos < sizeof(mockRetValues))
		{
			retval = mockRetValues[mockIdxPos];
			mockIdxPos += 1;
			//mockIdxPos %= sizeof(mockRetValues);
		}
		else retval = 0;
		
		//return rand()%256;
		//return retval;
		return 0xaa;
	}
	else if (address == 0x2141)
	{
		// 2141h - APUI01 - Main CPU to Sound CPU Communication Port 1 (R/W)
		unsigned char retval;

		if (mockIdxPos < sizeof(mockRetValues))
		{
			retval = mockRetValues[mockIdxPos];
			mockIdxPos += 1;
			//mockIdxPos %= sizeof(mockRetValues);
		}
		else retval = 0;

		//return rand() % 256;
		//return retval;
		return 0xbb;
	}

	return 0;
}

apu::~apu()
{
}
