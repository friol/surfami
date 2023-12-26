/* mem mgmt unit */

#ifndef MMU_H
#define MMU_H

class mmu
{
private:

	unsigned char snesRAM[0x10000];

public:

	mmu();
	void write8(unsigned int address, unsigned char val);
	unsigned char read8(unsigned int address);
	~mmu();

};

#endif
