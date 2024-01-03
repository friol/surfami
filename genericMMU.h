#ifndef GENERICMMU_H
#define GENERICMMU_H

class genericMMU
{
private:

public:

	virtual void write8(unsigned int address, unsigned char val)=0;
	virtual unsigned char read8(unsigned int address)=0;

};

#endif
