#ifndef APU_H
#define APU_H

class apu
{
private:

	unsigned char apuChan[4] = { 0xaa,0,0,0 };

public:

	apu();
	~apu();

	unsigned char read8(unsigned int addr);
	void write8(unsigned int addr,unsigned char val);

};

#endif
