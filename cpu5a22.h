#ifndef CPU5A22_H
#define CPU5A22_H

#include <vector>
#include <string>
#include <iomanip>
#include <sstream>

#include "mmu.h"

class regStatus
{
private:
	bool N = false;			//	Negative
	bool V = false;			//	Overflow
	bool M = false;			//	Memory / Accumulator Select register size	-	0 = 16-bit, 1 = 8-bit	(only native mode)
	bool X = false;			//	Index register Select size					-	0 = 16-bit, 1 = 8-bit	(only native mode)
	bool D = false;			//	Decimal
	bool I = true;			//	IRQ Disable
	bool Z = false;			//	Zero
	bool C = false;			//	Carry
	bool B = true;	//	Break (emulation mode only)
	bool E = true;	//	6502 Emulation mode	

public:

	bool isDPLowNotZero() 
	{
		return (D & 0xff) != 0x00;
	}

	void setEmulation(int val)
	{
		E = val & 1;
	}

	void setAccuMemSize(unsigned char val) 
	{
		M = val & 1;
	}

	void setIndexSize(unsigned char val,unsigned char& hiX,unsigned char& hiY) 
	{
		X = val;
		if (X) 
		{
			hiX = 0;
			hiY = 0;
		}
	}

	void setIRQDisable(unsigned char val)
	{
		I = val & 1;
	}

	void setCarry(unsigned char val)
	{
		C = val & 1;
	}

	void setNegative(unsigned char val) 
	{
		N = val & 1;
	}

	void setZero(unsigned char val) 
	{
		Z = val & 1;
	}

	void setOverflow(unsigned char val) 
	{
		V = val & 1;
	}

	void setDecimal(unsigned char val)
	{
		D = val & 1;
	}
	
	void setBreak(unsigned char val)
	{
		B = val & 1;
	}

	//

	bool getIndexSize()
	{
		return X;
	}

	bool getNegative() 
	{
		return N;
	}
	bool getOverflow() 
	{
		return V;
	}
	bool getAccuMemSize() 
	{
		return M;
	}
	bool getDecimal() 
	{
		return D;
	}
	bool getIRQDisable() 
	{
		return I;
	}
	bool getZero() 
	{
		return Z;
	}
	bool getCarry() 
	{
		return C;
	}
	
	bool getBreak() 
	{
		return B;
	}
	bool getEmulation() 
	{
		return E;
	}


};

class cpu5a22
{
private:

	unsigned char regA_lo=0;
	unsigned char regA_hi=0;
	unsigned char regDBR=0;		//	Data Bank Register
	unsigned short int regD=0;		//	Direct Page Register
	unsigned char regPB=0;		//	Program Bank Register
	unsigned char regX_lo=0;	//	index X - low byte
	unsigned char regX_hi=0;	//	index X - high byte
	unsigned char regY_lo=0;	//	index Y - low byte
	unsigned char regY_hi=0;	//	index Y - high byte
	unsigned short int regSP;		//	Stack Pointer

	unsigned short int regPC;
	regStatus regP;

	mmu* pMMU;

	void pushToStack8(unsigned char val);
	void pushToStack16(unsigned short int val);
	unsigned char pullFromStack();

	unsigned int getImmediateAddress8();
	unsigned int getImmediateAddress16();
	unsigned int getAbsoluteAddress16();
	unsigned int getAbsoluteAddress16IndexedX();
	unsigned int getLongAddress();
	unsigned int getDirectPageAddress();

public:

	cpu5a22(mmu* theMMU);
	void reset();
	int stepOne();
	unsigned short int getPC();
	bool getIndexSize() { return regP.getIndexSize(); }
	bool getAccuMemSize() { return regP.getAccuMemSize(); }
	std::vector<std::string> getRegistersInfo();
	~cpu5a22();

};

#endif
