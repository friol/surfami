#ifndef CPU5A22_H
#define CPU5A22_H

#include <vector>
#include <string>
#include <iomanip>
#include <sstream>

#include "genericMMU.h"
#include "mmu.h"
#include "testMMU.h"

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
		return D != 0x00;
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

	void setByte(unsigned char val) 
	{
		C = val & 1;
		Z = (val >> 1) & 1;
		I = (val >> 2) & 1;
		D = (val >> 3) & 1;
		X = (val >> 4) & 1;
		M = (val >> 5) & 1;
		V = (val >> 6) & 1;
		N = (val >> 7) & 1;
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

	unsigned char getByte() 
	{
		if (!E) 
		{
			return (N << 7) | (V << 6) | (M << 5) | (X << 4) | (D << 3) | (I << 2) | (Z << 1) | (unsigned char)C;
		}

		return  (N << 7) | (V << 6) | (1 << 5) | (B << 4) | (D << 3) | (I << 2) | (Z << 1) | (unsigned char)C;
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

	bool isTestMMU;
	//mmu* pRealMMU;
	//testMMU* pMockMMU;
	//genericMMU* getMMU();
	genericMMU* pMMU;

	void pushToStack8(unsigned char val);
	void pushToStack16(unsigned short int val);
	unsigned char pullFromStack();

	unsigned int getImmediateAddress8();
	unsigned int getImmediateAddress16();
	unsigned int getAbsoluteAddress16();
	unsigned int getAbsoluteAddress16IndexedX();
	unsigned int getAbsoluteAddress16IndexedY();
	unsigned int getAbsoluteIndexedIndirectX();
	unsigned int getLongAddress();
	unsigned int getLongAddressIndexedX();
	unsigned int getDirectPageAddress();
	unsigned int getDirectPageIndirectLongAddress();
	unsigned int getDirectPageIndirectLongIndexedYAddress();
	unsigned int getDirectPageIndirectAddress();
	unsigned int getDirectPageIndexedXAddress();
	unsigned int getDirectPageIndirectIndexedYAddress();
	unsigned int getStackRelative();
	unsigned int getStackRelativeIndirectIndexedY();

	int doADC(unsigned int addr);
	int doSBC(unsigned int addr,int& pcAdder);

public:

	cpu5a22(genericMMU* theMMU,bool isTestMMUVar);
	void reset();
	int stepOne();
	
	unsigned short int getPC();
	unsigned char getPB() { return regPB;  }
	unsigned short int getSP() { return regSP;  }
	unsigned char getDBR() { return regDBR;  }
	unsigned short int getD() { return regD;  }
	unsigned short int getA() { return regA_lo | (regA_hi << 8); }
	unsigned short int getX() { return regX_lo | (regX_hi << 8); }
	unsigned short int getY() { return regY_lo | (regY_hi << 8); }
	unsigned char getP() { return regP.getByte(); }

	bool getIndexSize() { return regP.getIndexSize(); }
	bool getAccuMemSize() { return regP.getAccuMemSize(); }
	std::vector<std::string> getRegistersInfo();
	
	void setState(unsigned int pc, unsigned short int a, unsigned short int x, unsigned short int y,
		unsigned short int sp, unsigned char dbr, unsigned short int d, unsigned char pb, unsigned char p,unsigned char e);
	
	~cpu5a22();
};

#endif
