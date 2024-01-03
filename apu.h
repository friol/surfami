#ifndef APU_H
#define APU_H

class apu
{
private:

	int mockIdxPos = 0;
	unsigned char mockRetValues[2] = { 
		0xaa, 0xbb //, 0x05, 0xcc, 0xcd, 0x00, 0xcf, 0x01, 0xbd, 0x02, 0xe8, 0x03, 0x00, 0x04,
		//0x86, 0x06, 0x03, 0x07, 0xc5, 0x08, 0x87, 0x09, 0x03, 0x0a, 0xc5, 0x0b, 0x88, 0x0c,
		//0x03, 0x0d, 0xc5, 0x0e
		//0xaa, 0xbb, 0x3c, 0x0f, 0xcd, 0x00, 0xcf, 0x01, 0xbd, 0x02, 0xe8, 0x03, 0x00, 0x04,
		//0x86, 0x06, 0x03, 0x07, 0xc5, 0x08, 0x87, 0x09, 0x03, 0x0a, 0xc5, 0x0b, 0x88, 0x0c,
		//0x03, 0x0d, 0xc5, 0x0e
	};

public:

	apu();
	~apu();

	unsigned char read8(int addr);

};

#endif
