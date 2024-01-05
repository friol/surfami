
/* 

	our Ricoh 5A22 CPU - derived from WDC 65C816 
	a lot of code "inspired" by
	https://github.com/LilaQ/q00.snes/
	plug&play

*/

#include <typeinfo>
#include "cpu5a22.h"
#include "debugger5a22.h"
#include "logger.h"
#include "testMMU.h"

extern logger glbTheLogger;

//
//
//

cpu5a22::cpu5a22(genericMMU* theMMU,bool isTestMMUVar)
{
	isTestMMU = isTestMMUVar;
	if (isTestMMU)
	{
		pMMU = (testMMU*)theMMU;
	}
	else
	{
		pMMU = (mmu*)theMMU;
	}

}

void cpu5a22::reset()
{
	regP.setEmulation(1);
	regD = 0;
	regP.setAccuMemSize(1);
	regP.setIndexSize(1,regX_hi,regY_hi);

	int loReset = pMMU->read8(0xfffc);
	int hiReset = pMMU->read8(0xfffd);

	regPC = (unsigned short int)(loReset | (hiReset << 8));
	regSP = 0x1ff;
}

std::vector<std::string> cpu5a22::getRegistersInfo()
{
	std::vector<std::string> ret;

	// A,X,Y,PC,SP

	std::string s = "A:";

	std::stringstream strr;
	strr << std::hex << std::setw(2) << std::setfill('0') << (int)regA_hi;
	s+= strr.str();
	strr.str(""); strr.clear();
	strr << std::hex << std::setw(2) << std::setfill('0') << (int)regA_lo;
	s+= strr.str();

	strr.str(""); strr.clear();
	strr << std::hex << std::setw(2) << std::setfill('0') << (int)regX_hi;
	s += " X:" + strr.str();
	strr.str(""); strr.clear();
	strr << std::hex << std::setw(2) << std::setfill('0') << (int)regX_lo;
	s += strr.str();

	strr.str(""); strr.clear();
	strr << std::hex << std::setw(2) << std::setfill('0') << (int)regY_hi;
	s += " Y:" + strr.str();
	strr.str(""); strr.clear();
	strr << std::hex << std::setw(2) << std::setfill('0') << (int)regY_lo;
	s += strr.str();

	strr.str(""); strr.clear();
	strr << std::hex << std::setw(4) << std::setfill('0') << (int)regPC;
	s += " PC:" + strr.str();

	strr.str(""); strr.clear();
	strr << std::hex << std::setw(4) << std::setfill('0') << (int)regSP;
	s += " SP:" + strr.str();

	ret.push_back(s);

	// D,DBR, PBR

	s = "";
	strr.str(""); strr.clear();
	strr << std::hex << std::setw(4) << std::setfill('0') << (int)regD;
	s += "D:" + strr.str();

	strr.str(""); strr.clear();
	strr << std::hex << std::setw(2) << std::setfill('0') << (int)regDBR;
	s += " DB:" + strr.str();

	strr.str(""); strr.clear();
	strr << std::hex << std::setw(2) << std::setfill('0') << (int)regPB;
	s += " PB:" + strr.str();

	ret.push_back(s);
	ret.push_back("");

	// flags (reg P)

	s = "NVMXDIZC    BE";
	ret.push_back(s);

	s = "";
	if (regP.getNegative()) s += "1";
	else s += "0";
	if (regP.getOverflow()) s += "1";
	else s += "0";
	if (regP.getAccuMemSize()) s += "1";
	else s += "0";
	if (regP.getIndexSize()) s += "1";
	else s += "0";
	if (regP.getDecimal()) s += "1";
	else s += "0";
	if (regP.getIRQDisable()) s += "1";
	else s += "0";
	if (regP.getZero()) s += "1";
	else s += "0";
	if (regP.getCarry()) s += "1";
	else s += "0";
	s += "    ";

	if (regP.getBreak()) s += "1";
	else s += "0";
	if (regP.getEmulation()) s += "1";
	else s += "0";


	ret.push_back(s);
	return ret;
}

unsigned short int cpu5a22::getPC()
{
	return regPC;
}

void cpu5a22::pushToStack8(unsigned char val)
{
	regSP = regSP - 1;
	pMMU->write8(regSP + 1, val);
}

void cpu5a22::pushToStack16(unsigned short int val)
{
	regSP = regSP - 2;
	pMMU->write8(regSP + 2, val >> 8);
	pMMU->write8(regSP + 1, val&0xff);
}

unsigned char cpu5a22::pullFromStack()
{
	unsigned char val = pMMU->read8((regSP+1)&0xffff);
	regSP = regSP + 1;
	return val;
}

unsigned int cpu5a22::getImmediateAddress8()
{
	unsigned int pb = regPB;
	return (pb << 16) | (regPC+1);
}

unsigned int cpu5a22::getImmediateAddress16()
{
	unsigned int pb = regPB;
	unsigned int adr = (pb << 16) | (regPC + 1);
	return adr;
}

unsigned int cpu5a22::getAbsoluteAddress16() 
{
	//unsigned char dbr = regDBR;
	//unsigned short int adr = (
	//		(pMMU->read8((regPB << 16) | (regPC+2)) << 8) | 
	//		pMMU->read8((regPB << 16) | (regPC+1))
	//		);
	//return (dbr << 16) | adr;
	unsigned short int adr = ((pMMU->read8((regPB << 16) | (regPC+2)) << 8) | pMMU->read8((regPB << 16) | (regPC+1)));
	return (regDBR << 16) | adr;
	return adr;
}

unsigned int cpu5a22::getAbsoluteAddress16IndexedX()
{
	unsigned int adr = ((pMMU->read8((regPB << 16) | (regPC+2)) << 8) | pMMU->read8((regPB << 16) | (regPC+1)));
	//regPB = (adr & 0xff00) != ((adr + (regX_lo | (regX_hi << 8))) & 0xff00);
	adr = adr + (regX_lo | (regX_hi << 8));
	return (regDBR << 16) | adr;
}

unsigned int cpu5a22::getAbsoluteAddress16IndexedY()
{
	unsigned int adr = (
		(pMMU->read8((regPB << 16) | (regPC + 2)) << 8) | 
		pMMU->read8((regPB << 16) | (regPC + 1))
		);
	
	//bool pbr = (adr & 0xff00) != ((adr + (regY_lo | (regY_hi << 8))) & 0xff00);

	//if (!regP.getIndexSize() && !regP.getEmulation()) 
	{
		adr = adr + (unsigned int)(regY_lo | (regY_hi << 8));
	}
	//else
	//{
	//	adr = adr + regY_lo;
	//}
	
	return ((regDBR << 16) | (adr&0xffffff));
}

unsigned int cpu5a22::getLongAddress()
{
	return (
		(pMMU->read8((regPB << 16) | ((regPC+3)&0xffff)) << 16) | 
		(pMMU->read8((regPB << 16) | ((regPC+2) & 0xffff)) << 8) |
		(pMMU->read8((regPB << 16) | ((regPC+1) & 0xffff))
		));
}

unsigned int cpu5a22::getLongAddressIndexedX() 
{
	unsigned int x = regX_lo;
	if (!regP.getIndexSize() && !regP.getEmulation()) 
	{
		x = regX_lo | (regX_hi << 8);
	}

	return 
		((
		(pMMU->read8((regPB << 16) | (regPC+3)&0xffff) << 16) | 
		(pMMU->read8((regPB << 16) | (regPC+2) & 0xffff) << 8) |
		(pMMU->read8((regPB << 16) | (regPC+1) & 0xffff))
			) +	x)&0xffffff ;
}

unsigned int cpu5a22::getDirectPageAddress()
{
	return (unsigned short int)(pMMU->read8((regPB << 16) | (regPC+1)) + regD);
}

unsigned int cpu5a22::getDirectPageIndirectLongAddress()
{
	unsigned short int dp_index = pMMU->read8(((regPB << 16) | (regPC+1))) + regD;
	unsigned int loByte = pMMU->read8(dp_index);
	unsigned int dp_adr = (pMMU->read8(dp_index + 2) << 16) | (pMMU->read8(dp_index + 1) << 8) | loByte;
	return dp_adr;
}

unsigned int cpu5a22::getDirectPageIndirectAddress()
{
	unsigned short int dp_index = pMMU->read8(((regPB << 16) | (regPC + 1))) + regD;
	unsigned short int dp_adr = ((pMMU->read8(dp_index + 1) << 8)) | (pMMU->read8(dp_index));
	return (regDBR << 16) | dp_adr;
}

unsigned int cpu5a22::getDirectPageIndirectLongIndexedYAddress() 
{
	unsigned int y = regY_lo;
	if (!regP.getIndexSize() && !regP.getEmulation())
	{
		y = regY_lo | (regY_hi << 8);
	}

	//unsigned char dp_index = pMMU->read8(((regPB << 16) | (regPC+1))) + regD;
	unsigned short int dp_index = pMMU->read8(((regPB << 16) | (regPC + 1))) + regD;
	unsigned int dp_adr = (pMMU->read8(dp_index + 2) << 16) | (pMMU->read8(dp_index + 1) << 8) | pMMU->read8(dp_index);
	//regPB = (dp_adr & 0xff00) != ((dp_adr + (regY_lo|(regY_hi<<8))) & 0xff00);
	dp_adr += y;
	return dp_adr;
}

unsigned int cpu5a22::getDirectPageIndexedXAddress()
{
	return regD | pMMU->read8(regPC+1) + (regX_lo|(regX_hi<<8));
}

unsigned int cpu5a22::getStackRelative()
{
	unsigned char bb = pMMU->read8((regPB << 16) | (regPC+1));
	return regSP + bb;
}

void cpu5a22::setState(unsigned int pc, unsigned short int a, unsigned short int x, unsigned short int y,
	unsigned short int sp, unsigned char dbr, unsigned short int d, unsigned char pb, unsigned char p, unsigned char e)
{
	regPC = pc;
	regA_lo = a & 0xff; regA_hi = a >> 8;
	regX_lo = x & 0xff; regX_hi = x >> 8;
	regY_lo = y & 0xff; regY_hi = y >> 8;
	regSP = sp;
	regDBR = dbr;
	regD = d;
	regPB = pb;
	regP.setByte(p);
	regP.setEmulation(e);
}

int cpu5a22::stepOne()
{
	//unsigned char nextOpcode = pMMU->read8(regPC);
	unsigned char nextOpcode = pMMU->read8((regPB << 16) | regPC);

	int cycles = 0;

	if (nextOpcode == 0x78)
	{
		// SEI - set interrupt disable flag
		regP.setIRQDisable(1);
		regPC += 1;
		cycles = 2;
	}
	else if (nextOpcode == 0x18)
	{
		// CLC - clear carry
		regP.setCarry(0);
		regPC += 1;
		cycles = 2;
	}
	else if (nextOpcode == 0xfb)
	{
		// XCE - exchange carry/emulation flags
		unsigned char tmp = regP.getCarry();

		regP.setCarry(regP.getEmulation());
		regP.setEmulation(tmp);

		if (tmp == 1) 
		{	
			//	if emulation mode
			regP.setAccuMemSize(1);
			regP.setIndexSize(1, regX_hi, regY_hi);
			regSP = 0x100 | (regSP & 0xff);
		}

		regPC += 1;
		cycles = 2;
	}
	else if (nextOpcode == 0x4b)
	{
		// PHK - Push Program Bank Register

		unsigned char pbReg = 0;
		if (!regP.getEmulation()) 
		{
			pbReg = regPB;
		}

		pushToStack8(pbReg);

		regPC += 1;
		cycles += 3;
	}
	else if (nextOpcode == 0xab)
	{
		// PLB - Pull data bank register

		regDBR = pullFromStack();
		regP.setNegative((regDBR & 0x80) == 0x80);
		regP.setZero(regDBR == 0x00);

		regPC += 1;
		cycles += 4;
	}
	else if (nextOpcode == 0xc2)
	{
		// REP - Reset Processor Status Bits
		const unsigned char nextByte = pMMU->read8((regPB << 16) | (regPC + 1));

		bool N = ((int)regP.getNegative() & (~((nextByte >> 7) & 1))) > 0;
		bool V = ((int)regP.getOverflow() & ~((nextByte >> 6) & 1)) > 0;
		bool M = ((int)regP.getAccuMemSize() & ~((nextByte >> 5) & 1)) > 0;
		bool X = ((int)regP.getIndexSize() & ~((nextByte >> 4) & 1)) > 0;
		bool B = ((int)regP.getBreak() & ~((nextByte >> 4) & 1)) > 0;
		bool D = ((int)regP.getDecimal() & ~((nextByte >> 3) & 1)) > 0;
		bool I = ((int)regP.getIRQDisable() & ~((nextByte >> 2) & 1)) > 0;
		bool Z = ((int)regP.getZero() & ~((nextByte >> 1) & 1)) > 0;
		bool C = ((int)regP.getCarry() & ~(nextByte & 1)) > 0;

		regP.setNegative(N);
		regP.setOverflow(V);
		if (!regP.getEmulation())
		{
			regP.setAccuMemSize(M);
			regP.setIndexSize(X,regX_hi,regY_hi);
		}
		else 
		{
			regP.setBreak(B);
			regP.setAccuMemSize(1);
			regP.setIndexSize(1, regX_hi, regY_hi);
		}

		regP.setDecimal(D);
		regP.setIRQDisable(I);
		regP.setZero(Z);
		regP.setCarry(C);

		regPC += 2;
		cycles= 3;
	}
	else if (nextOpcode == 0xa2)
	{
		// LDX - Load Index Register X from Memory
		int pcadder = 0;
		unsigned int addr;

		if (regP.getIndexSize()) 
		{
			// 8-bit regs
			addr = getImmediateAddress8();
			unsigned char lo = pMMU->read8(addr);
			regX_lo=lo;
			regP.setZero(lo == 0);
			regP.setNegative(lo >> 7);
		}
		else 
		{
			// 16-bit regs
			pcadder += 1;
			addr = getImmediateAddress16();
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned int val = (hi << 8) | lo;

			regX_lo = (val & 0xff);
			regX_hi = (unsigned char)(val >> 8);
			regP.setZero(val == 0);
			regP.setNegative(val >> 15);
		}

		int cycAdder = 0;
		if (regP.getIndexSize() == 0) cycAdder += 1;
		regPC+=2+pcadder;
		cycles+=2+cycAdder;
	}
	else if (nextOpcode == 0xa0)
	{
		// LDY - Load Index Register Y from Memory
		int pcadder = 0;
		unsigned int addr;

		if (regP.getIndexSize())
		{
			// 8-bit regs
			addr = getImmediateAddress8();
			unsigned char lo = pMMU->read8(addr);
			regY_lo = lo;
			regP.setZero(lo == 0);
			regP.setNegative(lo >> 7);
		}
		else
		{
			// 16-bit regs
			pcadder += 1;
			addr = getImmediateAddress16();
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned int val = (hi << 8) | lo;

			regY_lo = (val & 0xff);
			regY_hi = (unsigned char)(val >> 8);
			regP.setZero(val == 0);
			regP.setNegative(val >> 15);
		}

		int cycAdder = 0;
		if (regP.getIndexSize() == 0) cycAdder += 1;
		regPC += 2 + pcadder;
		cycles= 2 + cycAdder;
	}
	else if (nextOpcode == 0x9a)
	{
		// TXS - transfer X to SP
		if (regP.getEmulation()) 
		{
			regSP=0x100|regX_lo;
		}
		else 
		{
			regSP = regX_lo | (regX_hi << 8);
		}
		
		regPC++;
		cycles=2;
	}
	else if (nextOpcode == 0xa9)
	{
		// LDA - load accumulator

		int pcadder = 0;
		unsigned int addr;

		if (regP.getAccuMemSize())
		{
			// 8-bit regs
			addr = getImmediateAddress8();
			unsigned char lo = pMMU->read8(addr);
			regA_lo = lo;
			regP.setZero(lo == 0);
			regP.setNegative(lo >> 7);
		}
		else
		{
			// 16-bit regs
			pcadder += 1;
			addr = getImmediateAddress16();
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned int val = (hi << 8) | lo;

			regA_lo = (val & 0xff);
			regA_hi = (val >> 8);
			regP.setZero(val == 0);
			regP.setNegative(val >> 15);
		}

		int cycAdder = 0;
		if (regP.getAccuMemSize() == 0) cycAdder += 1;
		regPC += 2 + pcadder;
		cycles= 2 + cycAdder;
	}
	else if (nextOpcode == 0x5b)
	{
		// TCD - Transfer 16-bit Accumulator to Direct Page Register

		int accu = regA_lo | (regA_hi << 8);
		regD = accu;
		regP.setZero(regD == 0);
		regP.setNegative(regD >> 15);

		regPC += 1;
		cycles += 2;
	}
	else if (nextOpcode == 0xe2)
	{
		// SEP - Set Processor Status Bits
		const unsigned char nextByte = pMMU->read8((regPB<<16)|((regPC + 1)&0xffff));

		unsigned char N = (regP.getNegative() | ((nextByte >> 7) & 1)) > 0;
		unsigned char V = (regP.getOverflow() | ((nextByte >> 6) & 1)) > 0;
		unsigned char M = (regP.getAccuMemSize() | ((nextByte >> 5) & 1)) > 0;
		unsigned char X = (regP.getIndexSize() | ((nextByte >> 4) & 1)) > 0;
		unsigned char B = (regP.getBreak() | ((nextByte >> 4) & 1)) > 0;
		unsigned char D = (regP.getDecimal() | ((nextByte >> 3) & 1)) > 0;
		unsigned char I = (regP.getIRQDisable() | ((nextByte >> 2) & 1)) > 0;
		unsigned char Z = (regP.getZero() | ((nextByte >> 1) & 1)) > 0;
		unsigned char C = (regP.getCarry() | (nextByte & 1)) > 0;

		regP.setNegative(N);
		regP.setOverflow(V);
		if (!regP.getEmulation()) 
		{
			regP.setAccuMemSize(M);
			regP.setIndexSize(X,regX_hi,regY_hi);
		}
		else 
		{
			regP.setBreak(B);
			regP.setAccuMemSize(1);
			regP.setIndexSize(1, regX_hi, regY_hi);
		}

		regP.setDecimal(D);
		regP.setIRQDisable(I);
		regP.setZero(Z);
		regP.setCarry(C);

		regPC+=2;
		cycles=3;
	}
	else if (nextOpcode == 0x8c)
	{
		// STY - Store Y to Memory - Absolute
		int cycleAdder = 0;
		int addr = getAbsoluteAddress16();

		pMMU->write8(addr, regY_lo);
		if (regP.getIndexSize() == false)
		{
			pMMU->write8(addr + 1, regY_hi);
			cycleAdder += 1;
		}

		regPC += 3;
		cycles= 4 + cycleAdder;
	}
	else if (nextOpcode == 0x8d)
	{
		// STA - Store Accumulator to Memory - Absolute
		int cycleAdder = 0;
		int addr = getAbsoluteAddress16();

		pMMU->write8(addr, regA_lo);
		if (regP.getAccuMemSize() == false)
		{
			pMMU->write8(addr+1,regA_hi);
			cycleAdder += 1;
		}

		regPC += 3;
		cycles= 4 + cycleAdder;
	}
	else if (nextOpcode == 0x8e)
	{
		// STX - Store X to Memory - Absolute
		int cycleAdder = 0;
		int addr = getAbsoluteAddress16();

		pMMU->write8(addr, regX_lo);
		if (regP.getIndexSize() == false)
		{
			pMMU->write8(addr + 1, regX_hi);
			cycleAdder += 1;
		}

		regPC += 3;
		cycles= 4 + cycleAdder;
	}
	else if (nextOpcode == 0x9c)
	{
		// STZ - Store zero to memory - absolute
		int cycleAdder = 0;
		int addr = getAbsoluteAddress16();

		pMMU->write8(addr, 0);
		if (regP.getAccuMemSize() == false)
		{
			pMMU->write8(addr + 1, 0);
			cycleAdder += 1;
		}

		regPC += 3;
		cycles= 4 + cycleAdder;
	}
	else if (nextOpcode == 0xca)
	{
		// DEX - decrement X
		if (regP.getIndexSize()) 
		{
			regX_lo=(unsigned char)(regX_lo-1);
			regP.setZero(regX_lo == 0);
			regP.setNegative(regX_lo >> 7);
		}
		else 
		{
			unsigned short int ics = regX_lo | (regX_hi << 8);
			ics -= 1;

			regX_lo = ics & 0xff;
			regX_hi = ics >> 8;

			regP.setZero((regX_lo | (regX_hi << 8)) == 0);
			regP.setNegative((regX_lo | (regX_hi << 8)) >> 15);
		}

		regPC += 1;
		cycles += 2;
	}
	else if (nextOpcode == 0xd0)
	{
		// BNE - branch if not equal (Z==0)

		int branch_taken = 0;
		signed char offset = (signed char)pMMU->read8((regPB<<16) | (regPC + 1));
		
		if (regP.getZero() == 0) 
		{
			regPC += offset+2;
			branch_taken = 1;
		}
		else
		{
			regPC+=2;
		}
		
		cycles=2+branch_taken;
	}
	else if (nextOpcode == 0xea)
	{
		// NOPpy

		regPC += 1;
		cycles= 2;
	}
	else if (nextOpcode == 0x5c)
	{
		// JML - jump long
		unsigned long int addr = getLongAddress();
		unsigned char bnk = (addr >> 16) & 0xff;
		regPB = bnk;
		regPC = addr&0xffff;

		cycles = 4;
	}
	else if (nextOpcode == 0x2c)
	{
		// BIT - Test bits against accu Absolute
		int cycleAdder = 0;
		int addr = getAbsoluteAddress16();

		if (regP.getAccuMemSize() == 0)
		{
			cycleAdder += 1;

			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int val = (hi << 8) | lo;
			regP.setNegative(val >> 15);
			regP.setOverflow((val >> 14) & 1);
			regP.setZero((val & (regA_lo|(regA_hi<<8)) == 0x0000));
		}
		else
		{
			unsigned char val = pMMU->read8(addr);
			regP.setNegative(val >> 7);
			regP.setOverflow((val >> 6) & 1);
			regP.setZero((val & regA_lo) == 0x00);
		}

		regPC += 3;
		cycles = 4 + cycleAdder;
	}
	else if (nextOpcode == 0x10)
	{
		// BPL - branch if plus
		int branch_taken = 0;
		signed char offset = (signed char)pMMU->read8((regPB << 16) | (regPC + 1));

		if (regP.getNegative() == 0)
		{
			regPC += offset + 2;
			branch_taken = 1;
		}
		else
		{
			regPC += 2;
		}

		cycles = 2 + branch_taken;
	}
	else if (nextOpcode == 0xbd)
	{
		// LDA addr,X

		unsigned int addr = getAbsoluteAddress16IndexedX();
		
		if (regP.getAccuMemSize()) 
		{
			unsigned char lo = pMMU->read8(addr);
			regA_lo = lo;
			regP.setZero(lo == 0);
			regP.setNegative(lo >> 7);
		}
		else 
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned int val = (hi << 8) | lo;
			regA_lo = (val & 0xff);
			regA_hi = (val >> 8);
			regP.setZero(val == 0);
			regP.setNegative(val >> 15);
		}

		regPC += 3;
		cycles = 4;
		if (regP.getAccuMemSize() == 0) cycles += 1;
		if (regPB) { regPB = 0; cycles += 1; }
	}
	else if (nextOpcode == 0xe8)
	{
		// INX

		if (regP.getIndexSize()) 
		{
			regX_lo = (regX_lo + 1) & 0xff;
			regP.setZero(regX_lo == 0);
			regP.setNegative(regX_lo >> 7);
		}
		else 
		{
			int regX = (regX_lo | (regX_hi << 8));
			regX += 1;
			regX &= 0xffff;
			regX_lo = regX & 0xff; regX_hi = (regX >> 8);

			regP.setZero((regX_lo|(regX_hi<<8)) == 0);
			regP.setNegative((regX_lo | (regX_hi << 8)) >> 15);
		}

		regPC += 1;
		cycles += 2;
	}
	else if (nextOpcode == 0xe0)
	{
		// CPX - Compare Index Register X with Memory
		int cycAdder = 0;

		if (regP.getIndexSize()) 
		{
			unsigned int addr = getImmediateAddress8();
			unsigned char m = pMMU->read8(addr);
			unsigned char val = regX_lo - m;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
			regP.setCarry(regX_lo >= m);
			regPC += 2;
		}
		else 
		{
			unsigned int addr = getImmediateAddress16();
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int m = (hi << 8) | lo;
			unsigned short int val = (regX_lo|(regX_hi<<8)) - m;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
			regP.setCarry((regX_lo | (regX_hi << 8)) >= m);
			regPC += 3;
			cycAdder = 1;
		}

		cycles = 2 + cycAdder;
	}
	else if (nextOpcode == 0xb8)
	{
		// CLV - Clear overflow flag
		regP.setOverflow(0);

		regPC += 1;
		cycles = 2;
	}
	else if (nextOpcode == 0x3a)
	{
		// DEC A

		if (regP.getAccuMemSize()) 
		{
			unsigned char val = regA_lo;
			val--;
			regA_lo = val;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
		}
		else 
		{
			unsigned short int val = regA_lo | (regA_hi << 8);
			val--;
			
			regA_lo = val & 0xff;
			regA_hi = val >> 8;
			
			regP.setNegative(val >> 15);
			regP.setZero(val == 0);
		}

		regPC += 1;
		cycles = 2;
	}
	else if (nextOpcode == 0x85)
	{
		// STA dp
		int cycAdder = 0;
		unsigned int addr = getDirectPageAddress();

		pMMU->write8(addr,regA_lo);
		if (regP.getAccuMemSize()==0)
		{
			pMMU->write8((addr + 1)&0xffff,regA_hi);
			cycAdder += 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 3 + cycAdder;
	}
	else if (nextOpcode == 0x8a)
	{
		// TXA - Transfer Index Register X to Accumulator

		if (regP.getAccuMemSize()) 
		{
			unsigned char val = regX_lo;
			regA_lo=val;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
		}
		else 
		{
			unsigned short int val = regX_lo | (regX_hi << 8);
			regA_lo = regX_lo;
			regA_hi = regX_hi;
			regP.setNegative(val >> 15);
			regP.setZero(val == 0);
		}

		regPC += 1;
		cycles = 2;
	}
	else if (nextOpcode == 0xc9)
	{
		// CMP #const - Compare accu with memory

		int addr;
		int cycAdder = 0;

		if (regP.getAccuMemSize())
		{
			addr = getImmediateAddress8();
			
			unsigned char m = pMMU->read8(addr);
			unsigned char val = regA_lo - m;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
			regP.setCarry(regA_lo >= m);

			regPC += 2;
		}
		else
		{
			addr = getImmediateAddress16();

			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int m = (hi << 8) | lo;
			unsigned short int val = (regA_lo|(regA_hi<<8)) - m;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
			regP.setCarry((regA_lo | (regA_hi << 8)) >= m);

			regPC += 3;
			cycAdder = 1;
		}

		cycles = 2 + cycAdder;
	}
	else if (nextOpcode == 0x08)
	{
		// PHP - Push Processor Status Register

		pushToStack8(regP.getByte());

		regPC += 1;
		cycles = 3;
	}
	else if (nextOpcode == 0x68)
	{
		// PLA - Pull accu
		int cycAdder = 0;

		if (regP.getAccuMemSize()) 
		{
			regA_lo=pullFromStack();
			regP.setNegative((regA_lo & 0x80) == 0x80);
			regP.setZero(regA_lo == 0x00);
		}
		else 
		{
			unsigned char lo = pullFromStack();
			unsigned char hi = pullFromStack();
			unsigned short int val = (hi << 8) | lo;
			regA_lo = lo;
			regA_hi = hi;
			regP.setNegative((val & 0x8000) == 0x8000);
			regP.setZero(val == 0x00);
			cycAdder = 1;
		}

		regPC += 1;
		cycles = 4+cycAdder;
	}
	else if (nextOpcode == 0x4a)
	{
		// LSR A - Logical Shift Memory or Accumulator Right

		if (regP.getAccuMemSize()) 
		{
			regP.setNegative(0);
			regP.setCarry(regA_lo & 1);
			regA_lo=((unsigned char)((regA_lo & 0xff) >> 1));
			regP.setZero((regA_lo & 0xff) == 0);
		}
		else 
		{
			regP.setNegative(0);
			regP.setCarry(regA_lo & 1);
			unsigned short int accu=((unsigned short int)(((regA_lo|(regA_hi<<8)) >> 1) & 0xffff));
			regA_lo = accu & 0xff; regA_hi = accu >> 8;
			regP.setZero(accu == 0);
		}

		regPC += 1;
		cycles = 2;
	}
	else if (nextOpcode == 0x69)
	{
		// ADC #const - Add with carry
		int cycAdder = 0;

		if (regP.getAccuMemSize()) 
		{
			int addr = getImmediateAddress8();
			unsigned char val = pMMU->read8(addr);
			unsigned int res = 0;
			if (regP.getDecimal()) 
			{
				res = (regA_lo & 0xf) + (val & 0x0f) + regP.getCarry();
				if (res > 0x09) 
				{
					res += 0x06;
				}
				regP.setCarry(res > 0x0f);
				res = (regA_lo & 0xf0) + (val & 0xf0) + (regP.getCarry() << 4) + (res & 0x0f);
			}
			else 
			{
				res = (regA_lo & 0xff) + val + regP.getCarry();
			}
			regP.setOverflow((~(regA_lo ^ val) & (regA_lo ^ res) & 0x80) == 0x80);
			if (regP.getDecimal() && res > 0x9f) 
			{
				res += 0x60;
			}
			regP.setCarry(res > 0xff);
			regP.setZero((unsigned char)res == 0);
			regP.setNegative((res & 0x80) == 0x80);
			regA_lo=((unsigned char)(res & 0xff));

			regPC += 2;
		}
		else 
		{
			unsigned short int addr = getImmediateAddress16();
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);

			unsigned short int val = (hi << 8) | lo;
			unsigned long int res = 0;
			if (regP.getDecimal()) 
			{
				res = ((regA_lo|(regA_hi<<8)) & 0x000f) + (val & 0x000f) + regP.getCarry();
				if (res > 0x0009) 
				{
					res += 0x0006;
				}
				regP.setCarry(res > 0x000f);
				res = ((regA_lo | (regA_hi << 8)) & 0x00f0) + (val & 0x00f0) + (regP.getCarry() << 4) + (res & 0x000f);
				if (res > 0x009f) 
				{
					res += 0x0060;
				}
				regP.setCarry(res > 0x00ff);
				res = ((regA_lo | (regA_hi << 8)) & 0x0f00) + (val & 0x0f00) + (regP.getCarry() << 8) + (res & 0x00ff);
				if (res > 0x09ff) 
				{
					res += 0x0600;
				}
				regP.setCarry(res > 0x0fff);
				res = ((regA_lo | (regA_hi << 8)) & 0xf000) + (val & 0xf000) + (regP.getCarry() << 12) + (res & 0x0fff);
			}
			else 
			{
				res = (regA_lo | (regA_hi << 8)) + val + regP.getCarry();
			}
			regP.setOverflow((~((regA_lo | (regA_hi << 8)) ^ val) & ((regA_lo | (regA_hi << 8)) ^ res) & 0x8000) == 0x8000);
			if (regP.getDecimal() && res > 0x9fff) 
			{
				res += 0x6000;
			}
			regP.setCarry(res > 0xffff);
			regP.setZero((unsigned short int)(res) == 0);
			regP.setNegative((res & 0x8000) == 0x8000);

			regA_lo = res & 0xff;
			regA_hi = (res >> 8) & 0xff;

			regPC += 3;
			cycAdder = 1;
		}

		cycles = 2 + cycAdder;
	}
	else if (nextOpcode == 0x80)
	{
		// BRA - TODO: Add 1 cycle if branch taken crosses page boundary in emulation mode (e=1)
		
		unsigned int addr = getImmediateAddress8();
		signed char offset = pMMU->read8((regPB<<16)|addr);
		
		regPC += offset+2;
		cycles=3;
	}
	else if (nextOpcode == 0x29)
	{
		// AND Immediate
		int cycAdder = 0;

		if (regP.getAccuMemSize()) 
		{
			unsigned int addr = getImmediateAddress8();
			unsigned char val = pMMU->read8(addr);
			regA_lo=regA_lo & val;
			regP.setNegative(regA_lo >> 7);
			regP.setZero(regA_lo == 0);
			regPC += 2;
		}
		else 
		{
			unsigned int addr = getImmediateAddress16();
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int val = (hi << 8) | lo;

			unsigned short int accu = (regA_lo | (regA_hi << 8)) & val;
			regA_lo = accu&0xff;
			regA_hi = accu>>8;

			regP.setNegative((regA_lo | (regA_hi << 8)) >> 15);
			regP.setZero((regA_lo | (regA_hi << 8)) == 0);
			regPC += 3;
			cycAdder = 1;
		}

		cycles = 2 + cycAdder;
	}
	else if (nextOpcode == 0x88)
	{
		// DEY
		if (regP.getIndexSize())
		{
			regY_lo = (unsigned char)(regY_lo - 1);
			regP.setZero(regY_lo == 0);
			regP.setNegative(regY_lo >> 7);
		}
		else
		{
			unsigned short int ics = regY_lo | (regY_hi << 8);
			ics -= 1;

			regY_lo = ics & 0xff;
			regY_hi = ics >> 8;

			regP.setZero((regY_lo | (regY_hi << 8)) == 0);
			regP.setNegative((regY_lo | (regY_hi << 8)) >> 15);
		}

		regPC += 1;
		cycles = 2;
	}
	else if (nextOpcode == 0x20)
	{
		// JSR absolute
		unsigned int addr = getAbsoluteAddress16();

		pushToStack8((unsigned char)((regPC+2) >> 8));
		pushToStack8((unsigned char)((regPC+2) & 0xff));
		
		regPC = addr;

		cycles = 6;
	}
	else if (nextOpcode == 0x24)
	{
		// BIT dp
		int cycAdder = 0;

		if (regP.getAccuMemSize()) 
		{
			unsigned int addr = getDirectPageAddress();
			unsigned char val = pMMU->read8(addr);
			regP.setNegative(val >> 7);
			regP.setOverflow((val >> 6) & 1);
			regP.setZero((val & regA_lo) == 0x00);
		}
		else 
		{
			unsigned int addr = getDirectPageAddress();
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short val = (hi << 8) | lo;

			regP.setNegative(val >> 15);
			regP.setOverflow((val >> 14) & 1);
			regP.setZero((val & (regA_lo|(regA_hi<<8))) == 0x0000);

			cycAdder += 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 3 + cycAdder;
	}
	else if (nextOpcode == 0x60)
	{
		// RTS
		unsigned char lo = pullFromStack();
		unsigned char hi = pullFromStack();
		regPC = (hi << 8) | lo;
		regPC+=1;
		cycles=6;
	}
	else if (nextOpcode == 0xa5)
	{
		// LDA dp
		int cycAdder = 0;
		unsigned int addr = getDirectPageAddress();

		if (regP.getAccuMemSize())
		{
			unsigned char lo = pMMU->read8(addr);
			regA_lo = lo;
			regP.setZero(lo == 0);
			regP.setNegative(lo >> 7);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned int val = (hi << 8) | lo;

			regA_lo = (val & 0xff);
			regA_hi = (val >> 8);
			regP.setZero(val == 0);
			regP.setNegative(val >> 15);
			cycAdder += 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 3 + cycAdder;
	}
	else if (nextOpcode == 0xcd)
	{
		// CMP 
		int cycAdder = 0;
		int addr = getAbsoluteAddress16();

		if (regP.getAccuMemSize())
		{
			unsigned char m = pMMU->read8(addr);
			unsigned char val = regA_lo - m;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
			regP.setCarry(regA_lo >= m);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int m = (hi << 8) | lo;
			unsigned short int val = (regA_lo | (regA_hi << 8)) - m;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
			regP.setCarry((regA_lo | (regA_hi << 8)) >= m);
			cycAdder = 1;
		}

		regPC += 3;
		cycles = 4 + cycAdder;
	}
	else if (nextOpcode == 0xf0)
	{
		// BEQ
		int branch_taken = 0;
		signed char offset = (signed char)pMMU->read8((regPB<<16)|(regPC + 1));

		if (regP.getZero() == true)
		{
			regPC += offset + 2;
			branch_taken = 1;
		}
		else
		{
			regPC += 2;
		}

		cycles = 2 + branch_taken;
	}
	else if (nextOpcode == 0xa6)
	{
		// LDX dp
		int cycAdder = 0;
		int addr = getDirectPageAddress();

		if (regP.getIndexSize())
		{
			unsigned char lo = pMMU->read8(addr);
			regX_lo = lo;
			regP.setZero(lo == 0);
			regP.setNegative(lo >> 7);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned int val = (hi << 8) | lo;

			regX_lo = (val & 0xff);
			regX_hi = (unsigned char)(val >> 8);
			regP.setZero(val == 0);
			regP.setNegative(val >> 15);
			cycAdder += 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 3 + cycAdder;
	}
	else if (nextOpcode == 0xec)
	{
		// CPX Absolute
		int cycAdder = 0;
		int addr = getAbsoluteAddress16();

		if (regP.getIndexSize()) 
		{
			unsigned char m = pMMU->read8(addr);
			unsigned char val = regX_lo - m;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
			regP.setCarry(regX_lo >= m);
		}
		else {
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int m = (hi << 8) | lo;
			unsigned short int val = (regX_lo|(regX_hi<<8)) - m;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
			regP.setCarry((regX_lo | (regX_hi << 8)) >= m);
			cycAdder += 1;
		}

		regPC += 3;
		cycles = 4 + cycAdder;
	}
	else if (nextOpcode == 0xce)
	{
		// DEC addr
		int cycAdder = 0;
		int addr = getAbsoluteAddress16();

		if (regP.getAccuMemSize()) 
		{
			unsigned char val = pMMU->read8(addr);
			val--;
			pMMU->write8(addr,val);
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
		}
		else 
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int val = (hi << 8) | lo;
			val--;
			pMMU->write8(addr, val & 0xff);
			pMMU->write8(addr + 1, val >> 8);
			regP.setNegative(val >> 15);
			regP.setZero(val == 0);
			cycAdder += 2;
		}

		regPC += 3;
		cycles = 6 + cycAdder;
	}
	else if (nextOpcode == 0xc6)
	{
		// DEC dp
		int cycAdder = 0;
		int addr = getDirectPageAddress();

		if (regP.getAccuMemSize())
		{
			unsigned char val = pMMU->read8(addr);
			val--;
			pMMU->write8(addr, val);
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int val = (hi << 8) | lo;
			val--;
			pMMU->write8(addr, val & 0xff);
			pMMU->write8(addr + 1, val >> 8);
			regP.setNegative(val >> 15);
			regP.setZero(val == 0);
			cycAdder += 2;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 5 + cycAdder;
	}
	else if (nextOpcode == 0xde)
	{
		// DEC addr,X
		int cycAdder = 0;
		int addr = getAbsoluteAddress16IndexedX();

		if (regP.getAccuMemSize())
		{
			unsigned char val = pMMU->read8(addr);
			val--;
			pMMU->write8(addr, val);
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int val = (hi << 8) | lo;
			val--;
			pMMU->write8(addr, val & 0xff);
			pMMU->write8(addr + 1, val >> 8);
			regP.setNegative(val >> 15);
			regP.setZero(val == 0);
			cycAdder += 2;
		}

		regPC += 3;
		cycles = 7 + cycAdder; // TODO + pageBoundaryCrossed()
	}
	else if (nextOpcode == 0xd6)
	{
		// DEC dp,X
		int addr;
		int cycAdder = 0;

		addr = getDirectPageIndexedXAddress();

		if (regP.getAccuMemSize())
		{
			unsigned char val = pMMU->read8(addr);
			val--;
			pMMU->write8(addr, val);
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int val = (hi << 8) | lo;
			val--;
			pMMU->write8(addr, val & 0xff);
			pMMU->write8(addr + 1, val >> 8);
			regP.setNegative(val >> 15);
			regP.setZero(val == 0);
			cycAdder += 2;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 6 + cycAdder;
	}
	else if (nextOpcode == 0x4c)
	{
		// JMP Absolute
		unsigned short int adr = ((pMMU->read8((regPB << 16) | (regPC+2)) << 8) | pMMU->read8((regPB << 16) | (regPC+1)));
		regPC = (regPB << 16) | adr;
		//regPB = (adr >> 16) & 0xff;
		cycles = 3;
	}
	else if (nextOpcode == 0x86)
	{
		// STX dp - Store Index Register X to Memory
		int cycAdder = 0;
		int addr = getDirectPageAddress();

		pMMU->write8(addr,regX_lo);
		if (regP.getIndexSize()==false)
		{
			pMMU->write8(addr + 1, regX_hi);
			cycAdder += 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 3 + cycAdder;
	}
	else if (nextOpcode == 0x84)
	{
		// STY dp
		int cycAdder = 0;
		int addr = getDirectPageAddress();

		pMMU->write8(addr, regY_lo);
		if (regP.getIndexSize() == false)
		{
			pMMU->write8(addr + 1, regY_hi);
			cycAdder += 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 3 + cycAdder;
	}
	else if (nextOpcode == 0x54)
	{
		// MVN xx,xx

		unsigned char dst_bank = pMMU->read8(regPC + 1);
		unsigned char src_bank = pMMU->read8(regPC + 2);
		
		regDBR = dst_bank;

		unsigned int dst = (dst_bank << 16) | (regY_lo|(regY_hi<<8));
		unsigned int src = (src_bank << 16) | (regX_lo | (regX_hi << 8));

		unsigned char val = pMMU->read8(src);
		pMMU->write8(dst, val);
		
		unsigned short int accu = regA_lo | (regA_hi << 8);
		accu -= 1;
		regA_lo = accu & 0xff;
		regA_hi = accu >> 8;

		unsigned short int regX = regX_lo | (regX_hi << 8);
		unsigned short int regY = regY_lo | (regY_hi << 8);
		regX += 1; regY += 1;

		regX_lo = regX & 0xff; regY_lo = regY & 0xff;
		regX_hi = regX >>8; regY_hi = regY >>8;
		
		if (accu == 0xffff) regPC += 3;
		cycles = 7; // TODO verify this
	}
	else if (nextOpcode == 0xb5)
	{
		// LDA dp,X
		int addr = getDirectPageIndexedXAddress();
		int cycAdder = 0;

		if (regP.getAccuMemSize())
		{
			unsigned char lo = pMMU->read8(addr);
			regA_lo = lo;
			regP.setZero(lo == 0);
			regP.setNegative(lo >> 7);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned int val = (hi << 8) | lo;

			regA_lo = (val & 0xff);
			regA_hi = (val >> 8);
			regP.setZero(val == 0);
			regP.setNegative(val >> 15);
			cycAdder += 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 4 + cycAdder;
	}
	else if (nextOpcode == 0xdd)
	{
		// CMP absolute,X
		int addr = getAbsoluteAddress16IndexedX();
		int cycAdder = 0;

		if (regP.getAccuMemSize())
		{
			unsigned char m = pMMU->read8(addr);
			unsigned char val = regA_lo - m;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
			regP.setCarry(regA_lo >= m);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int m = (hi << 8) | lo;
			unsigned short int val = (regA_lo | (regA_hi << 8)) - m;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
			regP.setCarry((regA_lo | (regA_hi << 8)) >= m);
		}

		regPC += 3;
		cycles = 4 + cycAdder; // TODO: check pageboundarycrossed
	}
	else if (nextOpcode == 0x44)
	{
		// MVP - block move positive
		unsigned char dst_bank = pMMU->read8(regPC + 1);
		unsigned char src_bank = pMMU->read8(regPC + 2);

		regDBR = dst_bank;

		unsigned int dst = (dst_bank << 16) | (regY_lo | (regY_hi << 8));
		unsigned int src = (src_bank << 16) | (regX_lo | (regX_hi << 8));

		unsigned char val = pMMU->read8(src);
		pMMU->write8(dst, val);

		unsigned short int accu = regA_lo | (regA_hi << 8);
		accu -= 1;
		regA_lo = accu & 0xff;
		regA_hi = accu >> 8;

		unsigned short int regX = regX_lo | (regX_hi << 8);
		unsigned short int regY = regY_lo | (regY_hi << 8);
		regX -= 1; regY -= 1;

		regX_lo = regX & 0xff; regY_lo = regY & 0xff;
		regX_hi = regX >> 8; regY_hi = regY >> 8;

		if (accu == 0xffff) regPC += 3;
		cycles = 7; // TODO verify this
	}
	else if (nextOpcode == 0xeb)
	{
		// XBA - Exchange B and A 8-bit Accumulators
		unsigned char tmp = regA_lo;
		regA_lo = regA_hi;
		regA_hi = tmp;

		regP.setNegative(((regA_lo|(regA_hi<<8)) >> 7) & 1);
		regP.setZero(regA_hi == 0);

		regPC += 1;
		cycles = 3;
	}
	else if (nextOpcode == 0x30)
	{
		// BMI
		int addr = getImmediateAddress8();

		unsigned char branch_taken = 0;
		signed char offset = pMMU->read8(addr);

		if (regP.getNegative() == 1) 
		{
			regPC += offset+2;
			branch_taken = 1;
		}

		cycles = 2 + regP.getEmulation()+ branch_taken;
	}
	else if (nextOpcode == 0x48)
	{
		// PHA - push accu
		int cycAdder = 0;

		if (regP.getAccuMemSize()) 
		{
			pushToStack8((unsigned char)(regA_lo));
		}
		else 
		{
			pushToStack16((regA_lo|(regA_hi<<8)));
			cycAdder += 1;
		}

		regPC += 1;
		cycles = 3 + cycAdder;
	}
	else if (nextOpcode == 0xb9)
	{
		// LDA addr,Y
		int cycAdder = 0;
		unsigned int addr = getAbsoluteAddress16IndexedY();

		if (regP.getAccuMemSize())
		{
			unsigned char lo = pMMU->read8(addr);
			regA_lo = lo;
			regP.setZero(lo == 0);
			regP.setNegative(lo >> 7);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int val = (hi << 8) | lo;
			regA_lo = (val & 0xff);
			regA_hi = (val >> 8);
			regP.setZero(val == 0);
			regP.setNegative(val >> 15);
			cycAdder = 1;
		}

		regPC += 3;
		cycles = 4+cycAdder; // TODO check page boundary
	}
	else if (nextOpcode == 0xd9)
	{
		// CMP addr,Y
		int addr = getAbsoluteAddress16IndexedY();
		int cycAdder = 0;

		if (regP.getAccuMemSize())
		{
			unsigned char m = pMMU->read8(addr);
			unsigned char val = regA_lo - m;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
			regP.setCarry(regA_lo >= m);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int m = (hi << 8) | lo;
			unsigned short int val = (regA_lo | (regA_hi << 8)) - m;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
			regP.setCarry((regA_lo | (regA_hi << 8)) >= m);
		}

		regPC += 3;
		cycles = 4 + cycAdder; // TODO: check pageboundarycrossed
	}
	else if (nextOpcode == 0x74)
	{
		// STZ dp,X
		int addr;
		int cycAdder = 0;

		addr = getDirectPageIndexedXAddress();

		pMMU->write8(addr,0);
		if (regP.getAccuMemSize() == 0)
		{
			pMMU->write8(addr + 1,0);
			cycAdder += 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 4 + cycAdder;
	}
	else if (nextOpcode == 0x99)
	{
		// STA addr,Y
		int cycAdder = 0;
		unsigned int addr;
		addr = getAbsoluteAddress16IndexedY();

		pMMU->write8(addr,regA_lo);
		if (regP.getAccuMemSize() == 0)
		{
			pMMU->write8(addr+1,regA_hi);
			cycAdder = 1;
		}

		regPC += 3;
		cycles = 5 + cycAdder;
	}
	else if (nextOpcode == 0x22)
	{
		// JSL absolute long
		unsigned int addr = getLongAddress();
		
		pushToStack8(regPB);
		pushToStack8((unsigned char)((regPC+3) >> 8));
		pushToStack8((unsigned char)((regPC+3) & 0xff));

		regPC = addr;
		regPB = (addr >> 16) & 0xff;

		cycles = 8;
	}
	else if (nextOpcode == 0x1b)
	{
		// TCS Transfer 16-bit Accumulator to Stack Pointer
		regSP = regA_lo | (regA_hi << 8);
		regPC += 1;
		cycles = 2;
	}
	else if (nextOpcode == 0x8f)
	{
		// STA absolute long
		int cycAdder = 0;
		int addr = getLongAddress();

		pMMU->write8(addr,regA_lo);
		if (regP.getAccuMemSize() == 0)
		{
			pMMU->write8(addr + 1,regA_hi);
			cycAdder = 1;
		}

		regPC += 4;
		cycles = 5 + cycAdder;
	}
	else if (nextOpcode == 0x9f)
	{
		// STA absolute long,X
		int cycAdder = 0;
		unsigned int addr = getLongAddressIndexedX();

		pMMU->write8(addr, regA_lo);
		if (regP.getAccuMemSize() == 0)
		{
			pMMU->write8(addr + 1, regA_hi);
			cycAdder = 1;
		}

		regPC += 4;
		cycles = 5 + cycAdder;
	}
	else if (nextOpcode == 0x98)
	{
		// TYA - Transfer Index Register Y to Accumulator
		if (regP.getAccuMemSize())
		{
			unsigned char val = regY_lo;
			regA_lo = val;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
		}
		else
		{
			unsigned short int val = regY_lo | (regY_hi << 8);
			regA_lo = regY_lo;
			regA_hi = regY_hi;
			regP.setNegative(val >> 15);
			regP.setZero(val == 0);
		}

		regPC += 1;
		cycles = 2;
	}
	else if (nextOpcode == 0x38)
	{
		// SEC - set carry
		regP.setCarry(1);
		regPC += 1;
		cycles = 2;
	}
	else if (nextOpcode == 0xe9)
	{
		// SBC - Subtract with Borrow from Accumulator
		int cycAdder = 0;
		int pcAdder = 0;
		int addr;

		if (regP.getAccuMemSize())
		{
			addr = getImmediateAddress8();
			unsigned char val = pMMU->read8(addr);
			val = ~val;

			unsigned int res = 0;
			if (regP.getDecimal()) 
			{
				res = (regA_lo & 0xf) + (val & 0x0f) + regP.getCarry();
				if (res <= 0x0f) { res -= 0x06; }
				regP.setCarry(res > 0x0f);
				res = (regA_lo & 0xf0) + (val & 0xf0) + (regP.getCarry() << 4) + (res & 0x0f);
			}
			else 
			{
				res = (regA_lo & 0xff) + val + regP.getCarry();
			}
			regP.setOverflow((~((regA_lo & 0xff) ^ val) & ((regA_lo & 0xff) ^ res) & 0x80) == 0x80);
			if (regP.getDecimal() && res <= 0xff) { res -= 0x60; }
			regP.setCarry(res > 0xff);
			regP.setZero((unsigned char)res == 0);
			regP.setNegative((res & 0x80) == 0x80);
			regA_lo=((unsigned char)(res & 0xff));
		}
		else
		{
			addr = getImmediateAddress16();

			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int val = (hi << 8) | lo;
			val = ~val;
			
			unsigned int res = 0;
			if (regP.getDecimal()) 
			{
				res = ((regA_lo|(regA_hi<<8)) & 0x000f) + (val & 0x000f) + regP.getCarry();
				if (res <= 0x000f) {
					res -= 0x0006;
				}
				regP.setCarry(res > 0x000f);
				res = ((regA_lo | (regA_hi << 8)) & 0x00f0) + (val & 0x00f0) + (regP.getCarry() << 4) + (res & 0x000f);
				if (res <= 0x00ff) {
					res -= 0x0060;
				}
				regP.setCarry(res > 0x00ff);
				res = ((regA_lo | (regA_hi << 8)) & 0x0f00) + (val & 0x0f00) + (regP.getCarry() << 8) + (res & 0x00ff);
				if (res <= 0x0fff) {
					res -= 0x0600;
				}
				regP.setCarry(res > 0x0fff);
				res = ((regA_lo | (regA_hi << 8)) & 0xf000) + (val & 0xf000) + (regP.getCarry() << 12) + (res & 0x0fff);
			}
			else 
			{
				res = (regA_lo | (regA_hi << 8)) + val + regP.getCarry();
			}
			regP.setOverflow((~((regA_lo | (regA_hi << 8)) ^ val) & ((regA_lo | (regA_hi << 8)) ^ res) & 0x8000) == 0x8000);
			if (regP.getDecimal() && res <= 0xffff) 
			{
				res -= 0x6000;
			}
			regP.setCarry(res > 0xffff);
			regP.setZero((unsigned short int)(res) == 0);
			regP.setNegative((res & 0x8000) == 0x8000);

			regA_lo = res & 0xff;
			regA_hi = (res >> 8) & 0xff;

			pcAdder = 1;
		}

		regPC += 2 + pcAdder;
		cycles = 2 + cycAdder;
	}
	else if (nextOpcode == 0xa8)
	{
		// TAY TAY TAY - Transfer Accumulator to Index Register Y
		if (regP.getIndexSize()) 
		{
			unsigned char val = regA_lo;
			regY_lo=val;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
		}
		else 
		{
			unsigned short val = regA_lo | (regA_hi << 8);
			regY_lo = regA_lo;
			regY_hi = regA_hi;
			regP.setNegative(val >> 15);
			regP.setZero(val == 0);
		}

		regPC += 1;
		cycles = 2;
	}
	else if (nextOpcode == 0x2d)
	{
		// AND addr - the bidimensional opcode
		int cycAdder = 0;
		int addr = getAbsoluteAddress16();

		if (regP.getAccuMemSize()) 
		{
			unsigned char val = pMMU->read8(addr);
			regA_lo=regA_lo & val;
			regP.setNegative(regA_lo >> 7);
			regP.setZero((regA_lo & 0xff) == 0);
		}
		else {
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int val = (hi << 8) | lo;
			
			int accu = regA_lo | (regA_hi << 8);
			
			unsigned short int res=((unsigned short)(accu & val));
			regA_lo = res & 0xff;
			regA_hi = res >> 8;
			
			regP.setNegative((regA_lo | (regA_hi << 8)) >> 15);
			regP.setZero((regA_lo | (regA_hi << 8)) == 0);
			cycAdder = 1;
		}

		regPC += 3;
		cycles = 4 + cycAdder;
	}
	else if (nextOpcode == 0x2f)
	{
		// AND absolute long
		int cycAdder = 0;
		int addr = getLongAddress();

		if (regP.getAccuMemSize())
		{
			unsigned char val = pMMU->read8(addr);
			regA_lo = regA_lo & val;
			regP.setNegative(regA_lo >> 7);
			regP.setZero((regA_lo & 0xff) == 0);
		}
		else {
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int val = (hi << 8) | lo;

			int accu = regA_lo | (regA_hi << 8);

			unsigned short int res = ((unsigned short)(accu & val));
			regA_lo = res & 0xff;
			regA_hi = res >> 8;

			regP.setNegative((regA_lo | (regA_hi << 8)) >> 15);
			regP.setZero((regA_lo | (regA_hi << 8)) == 0);
			cycAdder = 1;
		}

		regPC += 4;
		cycles = 5 + cycAdder;
	}
	else if (nextOpcode == 0x25)
	{
		// AND dp
		int cycAdder = 0;
		int addr = getDirectPageAddress();

		if (regP.getAccuMemSize())
		{
			unsigned char val = pMMU->read8(addr);
			regA_lo = regA_lo & val;
			regP.setNegative(regA_lo >> 7);
			regP.setZero((regA_lo & 0xff) == 0);
		}
		else {
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int val = (hi << 8) | lo;

			int accu = regA_lo | (regA_hi << 8);

			unsigned short int res = ((unsigned short)(accu & val));
			regA_lo = res & 0xff;
			regA_hi = res >> 8;

			regP.setNegative((regA_lo | (regA_hi << 8)) >> 15);
			regP.setZero((regA_lo | (regA_hi << 8)) == 0);
			cycAdder = 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 3 + cycAdder;
	}
	else if (nextOpcode == 0x32)
	{
		// AND (dp)
		int cycAdder = 0;
		unsigned int addr = getDirectPageIndirectAddress();

		if (regP.getAccuMemSize())
		{
			unsigned char val = pMMU->read8(addr);
			regA_lo = regA_lo & val;
			regP.setNegative(regA_lo >> 7);
			regP.setZero((regA_lo & 0xff) == 0);
		}
		else 
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int val = (hi << 8) | lo;

			unsigned short int accu = regA_lo | (regA_hi << 8);

			unsigned short int res = ((unsigned short int)(accu & val));
			regA_lo = res & 0xff;
			regA_hi = res >> 8;

			regP.setNegative((regA_lo | (regA_hi << 8)) >> 15);
			regP.setZero((regA_lo | (regA_hi << 8)) == 0);
			cycAdder = 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 5 + cycAdder;
	}
	else if (nextOpcode == 0x27)
	{
		// AND [dp]
		int cycAdder = 0;
		int addr = getDirectPageIndirectLongAddress();

		if (regP.getAccuMemSize())
		{
			unsigned char val = pMMU->read8(addr);
			regA_lo = regA_lo & val;
			regP.setNegative(regA_lo >> 7);
			regP.setZero((regA_lo & 0xff) == 0);
		}
		else 
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int val = (hi << 8) | lo;

			int accu = regA_lo | (regA_hi << 8);

			unsigned short int res = ((unsigned short)(accu & val));
			regA_lo = res & 0xff;
			regA_hi = res >> 8;

			regP.setNegative((regA_lo | (regA_hi << 8)) >> 15);
			regP.setZero((regA_lo | (regA_hi << 8)) == 0);
			cycAdder = 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 6 + cycAdder;
	}
	else if (nextOpcode == 0xb7)
	{
		// LDA [dp],Y
		int cycAdder = 0;
		int addr = getDirectPageIndirectLongIndexedYAddress();

		if (regP.getAccuMemSize())
		{
			unsigned char lo = pMMU->read8(addr);
			regA_lo = lo;
			regP.setZero(lo == 0);
			regP.setNegative(lo >> 7);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned int val = (hi << 8) | lo;

			regA_lo = (val & 0xff);
			regA_hi = (val >> 8);
			regP.setZero(val == 0);
			regP.setNegative(val >> 15);
			cycAdder += 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 6 + cycAdder;
	}
	else if (nextOpcode == 0xc8)
	{
		// INY

		if (regP.getIndexSize())
		{
			regY_lo = (regY_lo + 1) & 0xff;
			regP.setZero(regY_lo == 0);
			regP.setNegative(regY_lo >> 7);
		}
		else
		{
			int regY = (regY_lo | (regY_hi << 8));
			regY += 1;
			regY &= 0xffff;
			regY_lo = regY & 0xff; regY_hi = (regY >> 8);

			regP.setZero((regY_lo | (regY_hi << 8)) == 0);
			regP.setNegative((regY_lo | (regY_hi << 8)) >> 15);
		}

		regPC += 1;
		cycles += 2;
	}
	else if (nextOpcode == 0xaa)
	{
		// TAX
		if (regP.getIndexSize())
		{
			unsigned char val = regA_lo;
			regX_lo = val;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
		}
		else
		{
			unsigned short val = regA_lo | (regA_hi << 8);
			regX_lo = regA_lo;
			regX_hi = regA_hi;
			regP.setNegative(val >> 15);
			regP.setZero(val == 0);
		}

		regPC += 1;
		cycles = 2;
	}
	else if (nextOpcode == 0x2a)
	{
		// ROL

		if (regP.getAccuMemSize()) 
		{
			unsigned char val = regA_lo;
			unsigned char old_C = regP.getCarry();
			regP.setCarry(val >> 7);
			val = (val << 1) + old_C;
			regA_lo = val;
			regP.setZero(val == 0);
			regP.setNegative(val >> 7);
		}
		else 
		{
			unsigned short int val = (regA_lo|(regA_hi<<8));
			unsigned char old_C = regP.getCarry();
			regP.setCarry(val >> 15);
			val = (val << 1) + old_C;
			
			regA_lo = val & 0xff;
			regA_hi = val >> 8;
			
			regP.setZero(val == 0);
			regP.setNegative(val >> 15);
		}

		regPC++;
	}
	else if (nextOpcode == 0x70)
	{
		// BVS
		unsigned char branch_taken = 0;
		unsigned char pb = regPB;
		int addr = getImmediateAddress8();

		signed char offset = pMMU->read8((pb << 16) | addr);

		if (regP.getOverflow() == 1) 
		{
			regPC += offset + 2;
			branch_taken = 1;
		}
		else
		{
			regPC += 2;
		}

		cycles=2+branch_taken; // TODO check getEmulation
	}
	else if (nextOpcode == 0x1a)
	{
		// INC A

		if (regP.getAccuMemSize()) 
		{
			unsigned char val = regA_lo;
			val++;
			regA_lo=val;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
		}
		else 
		{
			unsigned short int val = (regA_lo|(regA_hi<<8));
			val++;

			regA_lo = val & 0xff;
			regA_hi = val >> 8;

			regP.setNegative(val >> 15);
			regP.setZero(val == 0);
		}

		regPC += 1;
		cycles = 2;
	}
	else if (nextOpcode == 0xd8)
	{
		// CLD
		regP.setDecimal(0);
		regPC += 1;
		cycles = 2;
	}
	else if (nextOpcode == 0x8b)
	{
		// PHB
		pushToStack8(regDBR);
		regPC += 1;
		cycles = 3;
	}
	else if (nextOpcode == 0x9b)
	{
		// TXY
		if (regP.getIndexSize()) 
		{
			unsigned char val = regX_lo;
			regY_lo=regX_lo;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
		}
		else 
		{
			unsigned short int val = (regX_lo|(regX_hi<<8));
			regY_lo = val & 0xff;
			regY_hi = val >> 8;
			regP.setNegative(val >> 15);
			regP.setZero(val == 0);
		}

		regPC += 1;
		cycles = 2;
	}
	else if (nextOpcode == 0xad)
	{
		// LDA addr
		int cycAdder = 0;
		int addr = getAbsoluteAddress16();

		if (regP.getAccuMemSize())
		{
			unsigned char lo = pMMU->read8(addr);
			regA_lo = lo;
			regP.setZero(lo == 0);
			regP.setNegative(lo >> 7);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned int val = (hi << 8) | lo;
			regA_lo = (val & 0xff);
			regA_hi = (val >> 8);
			regP.setZero(val == 0);
			regP.setNegative(val >> 15);
			cycAdder = 1;
		}

		regPC += 3;
		cycles = 4 + cycAdder;
	}
	else if (nextOpcode == 0x09)
	{
		// ORA #const
		int cycAdder = 0;
		int pcAdder = 0;

		if (regP.getAccuMemSize()) 
		{
			unsigned int addr = getImmediateAddress8();
			unsigned char val = pMMU->read8(addr);
			unsigned char res = val | regA_lo;
			regA_lo = res;
			regP.setNegative(res >> 7);
			regP.setZero(res == 0);
		}
		else 
		{
			unsigned int addr = getImmediateAddress16();
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int val = (hi << 8) | lo;
			unsigned short int res = val | (regA_lo|(regA_hi<<8));

			regA_lo = res & 0xff;
			regA_hi = res >> 8;

			regP.setNegative(res >> 15);
			regP.setZero(res == 0);
			pcAdder = 1;
			cycAdder = 1;
		}

		regPC += 2 + pcAdder;
		cycles = 2 + cycAdder;
	}
	else if (nextOpcode == 0x6b)
	{
		// RTL
		unsigned char lo = pullFromStack();
		unsigned char hi = pullFromStack();
		regPB=pullFromStack();

		regPC = (regPB << 16) | (hi << 8) | lo;
		regPC+=1;
		cycles = 6;
	}
	else if (nextOpcode == 0xbf)
	{
		// LDA abslong,X
		int cycAdder = 0;
		int addr = getLongAddressIndexedX();

		if (regP.getAccuMemSize())
		{
			unsigned char lo = pMMU->read8(addr);
			regA_lo = lo;
			regP.setZero(lo == 0);
			regP.setNegative(lo >> 7);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned int val = (hi << 8) | lo;
			regA_lo = (val & 0xff);
			regA_hi = (val >> 8);
			regP.setZero(val == 0);
			regP.setNegative(val >> 15);
			cycAdder = 1;
		}

		regPC += 4;
		cycles = 5 + cycAdder;
	}
	else if (nextOpcode == 0x0a)
	{
		// ASL A

		if (regP.getAccuMemSize()) 
		{
			unsigned char val = regA_lo;
			regP.setCarry(val >> 7);
			regA_lo=((unsigned char)((val << 1) & 0xff));
			regP.setZero(regA_lo == 0);
			regP.setNegative(regA_lo >> 7);
		}
		else 
		{
			unsigned short int val = (regA_lo|(regA_hi<<8));
			regP.setCarry(val >> 15);
			regA_lo = (val << 1) & 0xff;
			regA_hi = (val << 1) >>8;
			regP.setZero((regA_lo|(regA_hi<<8)) == 0);
			regP.setNegative((regA_lo | (regA_hi << 8)) >> 15);
		}

		regPC += 1;
		cycles = 2;
	}
	else if (nextOpcode == 0x0b)
	{
		// PHD - the famous phd
		pushToStack8((unsigned char)(regD >> 8));
		pushToStack8((unsigned char)(regD & 0xff));
		regPC += 1;
		cycles=4;
	}
	else if (nextOpcode == 0x5a)
	{
		// PHY - push Y
		int cycAdder = 0;

		if (regP.getIndexSize()) 
		{
			pushToStack8((unsigned char)(regY_lo));
		}
		else 
		{
			pushToStack16(regY_lo|(regY_hi<<8));
			cycAdder += 1;
		}

		regPC += 1;
		cycles = 3+cycAdder;
	}
	else if (nextOpcode == 0x2b)
	{
		// PLD
		unsigned char lo = pullFromStack();
		unsigned char hi = pullFromStack();
		regD = (hi << 8) | lo;
		regP.setNegative((regDBR & 0x8000) == 0x8000);
		regP.setZero(regDBR == 0x0000);
		regPC += 1;
		cycles = 5;
	}
	else if (nextOpcode == 0x64)
	{
		// STZ dp
		int cycAdder = 0;

		unsigned int addr = getDirectPageAddress();
		pMMU->write8(addr,0);
		if (regP.getAccuMemSize()==0)
		{
			pMMU->write8(addr + 1, 0);
			cycAdder += 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 3 + cycAdder;
	}
	else if (nextOpcode == 0xb0)
	{
		// BCS
		int addr = getImmediateAddress8();

		unsigned char branch_taken = 0;
		signed char offset = pMMU->read8(addr);

		if (regP.getCarry() == 1) 
		{
			regPC += offset + 2;
			branch_taken = 1;
		}
		else
		{
			regPC += 2;
		}

		cycles=2 + branch_taken;
	}
	else if (nextOpcode == 0xa7)
	{
		// LDA [dp]
		int cycAdder = 0;
		int addr = getDirectPageIndirectLongAddress();

		if (regP.getAccuMemSize())
		{
			unsigned char lo = pMMU->read8(addr);
			regA_lo = lo;
			regP.setZero(lo == 0);
			regP.setNegative(lo >> 7);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned int val = (hi << 8) | lo;
			regA_lo = (val & 0xff);
			regA_hi = (val >> 8);
			regP.setZero(val == 0);
			regP.setNegative(val >> 15);
			cycAdder += 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 6 + cycAdder;
	}
	else if (nextOpcode == 0xe6)
	{
		// INC dp
		int cycAdder = 0;
		int addr = getDirectPageAddress();

		if (regP.getAccuMemSize()) 
		{
			unsigned char val = pMMU->read8(addr);
			val++;
			pMMU->write8(addr,val);
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
		}
		else 
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);

			unsigned short int val = (hi << 8) | lo;
			val++;

			pMMU->write8(addr, val & 0xff);
			pMMU->write8(addr+1, val>>8);
			regP.setNegative(val >> 15);
			regP.setZero(val == 0);
			cycAdder += 2;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 5 + cycAdder;
	}
	else if (nextOpcode == 0xc4)
	{
		// CPY dp
		int cycAdder = 0;
		int addr = getDirectPageAddress();

		if (regP.getIndexSize()) 
		{
			unsigned char m = pMMU->read8(addr);
			unsigned char val = regY_lo - m;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
			regP.setCarry(regY_lo >= m);
		}
		else 
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int m = (hi << 8) | lo;
			unsigned short val = (regY_lo|(regY_hi<<8)) - m;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
			regP.setCarry((regY_lo | (regY_hi << 8)) >= m);
			cycAdder += 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 3 + cycAdder;
	}
	else if (nextOpcode == 0x28)
	{
		// PLP - Pull Processor Status Register

		if (regP.getEmulation()) 
		{
			unsigned char val = pullFromStack();
			regP.setByte(val);
			regP.setAccuMemSize(1);
			regP.setIndexSize(1,regX_hi,regY_hi);
		}
		else 
		{
			unsigned char val = pullFromStack();
			regP.setByte(val);
		}

		regPC += 1;
		cycles = 4;
	}
	else if (nextOpcode == 0x58)
	{
		// CLI - Clear Interrupt Disable Flag
		regP.setIRQDisable(0);
		regPC += 1;
		cycles = 2;
	}
	else if (nextOpcode == 0x90)
	{
		// BCC - branch if carry clear

		int addr = getImmediateAddress8();

		unsigned char branch_taken = 0;
		signed char offset = pMMU->read8(addr);

		if (regP.getCarry() == 0)
		{
			regPC += offset + 2;
			branch_taken = 1;
		}
		else
		{
			regPC += 2;
		}

		cycles = 2 + branch_taken;
	}
	else if (nextOpcode == 0x05)
	{
		// ORA dp
		int cycAdder = 0;
		int addr = getDirectPageAddress();

		if (regP.getAccuMemSize())
		{
			unsigned char val = pMMU->read8(addr);
			unsigned char res = val | regA_lo;
			regA_lo = res;
			regP.setNegative(res >> 7);
			regP.setZero(res == 0);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int val = (hi << 8) | lo;
			unsigned short int res = val | (regA_lo | (regA_hi << 8));

			regA_lo = res & 0xff;
			regA_hi = res >> 8;

			regP.setNegative(res >> 15);
			regP.setZero(res == 0);
			cycAdder = 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 3 + cycAdder;
	}
	else if (nextOpcode == 0x9d)
	{
		// STA absolute,X
		int cycAdder = 0;
		unsigned int addr = getAbsoluteAddress16IndexedX();

		pMMU->write8(addr,regA_lo);
		if (regP.getAccuMemSize()==0)
		{
			pMMU->write8(addr + 1, regA_hi);
			cycAdder += 1;
		}

		regPC += 3;
		cycles = 5 + cycAdder;
	}
	else if (nextOpcode == 0x9e)
	{
		// STZ absolute,X
		int cycAdder = 0;
		unsigned int addr = getAbsoluteAddress16IndexedX();

		pMMU->write8(addr, 0);
		if (regP.getAccuMemSize() == 0)
		{
			pMMU->write8(addr + 1, 0);
			cycAdder += 1;
		}

		regPC += 3;
		cycles = 5 + cycAdder;
	}
	else if (nextOpcode == 0xbe)
	{
		// LDX addr,Y
		int cycAdder = 0;
		unsigned int addr = getAbsoluteAddress16IndexedY();

		if (regP.getIndexSize())
		{
			unsigned char lo = pMMU->read8(addr);
			regX_lo = lo;
			regP.setZero(lo == 0);
			regP.setNegative(lo >> 7);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned int val = (hi << 8) | lo;

			regX_lo = (val & 0xff);
			regX_hi = (unsigned char)(val >> 8);
			regP.setZero(val == 0);
			regP.setNegative(val >> 15);
			cycAdder += 1;
		}

		regPC += 3;
		cycles = 4 + cycAdder; // TODO pagecross
	}
	else if (nextOpcode == 0xc0)
	{
		// CPY #const
		int cycAdder = 0;

		if (regP.getIndexSize())
		{
			unsigned int addr = getImmediateAddress8();
			unsigned char m = pMMU->read8(addr);
			unsigned char val = regY_lo - m;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
			regP.setCarry(regY_lo >= m);
		}
		else
		{
			unsigned int addr = getImmediateAddress16();
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int m = (hi << 8) | lo;
			unsigned short val = (regY_lo | (regY_hi << 8)) - m;
			regP.setNegative(val >> 7);
			regP.setZero(val == 0);
			regP.setCarry((regY_lo | (regY_hi << 8)) >= m);
			cycAdder += 1;
			regPC += 1;
		}

		regPC += 2;
		cycles = 2 + cycAdder;
	}
	else if (nextOpcode == 0x4d)
	{
		// EOR Absolute
		int cycAdder = 0;
		unsigned int addr = getAbsoluteAddress16();

		if (regP.getAccuMemSize()) 
		{
			unsigned char val = pMMU->read8(addr);
			unsigned char res = val ^ (regA_lo & 0xff);
			regA_lo = res;
			regP.setNegative(res >> 7);
			regP.setZero(res == 0);
		}
		else 
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int val = (hi << 8) | lo;
			unsigned short int res = val ^ (regA_lo|(regA_hi<<8));
			regA_lo = res & 0xff;
			regA_hi = res >> 8;
			regP.setNegative(res >> 15);
			regP.setZero(res == 0);
		}

		regPC += 3;
		cycles = 4 + cycAdder;
	}
	else if (nextOpcode == 0xae)
	{
		// LDX addr
		int cycAdder = 0;
		unsigned int addr = getAbsoluteAddress16();

		if (regP.getIndexSize())
		{
			unsigned char lo = pMMU->read8(addr);
			regX_lo = lo;
			regP.setZero(lo == 0);
			regP.setNegative(lo >> 7);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned int val = (hi << 8) | lo;

			regX_lo = (val & 0xff);
			regX_hi = (unsigned char)(val >> 8);
			regP.setZero(val == 0);
			regP.setNegative(val >> 15);
			cycAdder += 1;
		}

		regPC += 3;
		cycles = 4 + cycAdder;
	}
	else if (nextOpcode == 0x0c)
	{
		// TSB addr - Test and Set Memory Bits Against Accumulator
		int cycAdder = 0;
		unsigned int addr = getAbsoluteAddress16();

		if (regP.getAccuMemSize()) 
		{
			unsigned char val = pMMU->read8(addr);
			regP.setZero((val & (regA_lo)) == 0);
			unsigned char res = val | (regA_lo);
			pMMU->write8(addr,res);
		}
		else 
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int val = (hi << 8) | lo;
			regP.setZero((val & (regA_lo|(regA_hi>>8))) == 0);
			unsigned short int res = val | (regA_lo | (regA_hi >> 8));
			pMMU->write8(addr, res & 0xff);
			pMMU->write8(addr + 1, res >> 8);
		}

		regPC += 3;
		cycles = 6 + cycAdder;
	}
	else if (nextOpcode == 0x0d)
	{
		// ORA addr
		int cycAdder = 0;
		unsigned int addr = getAbsoluteAddress16();

		if (regP.getAccuMemSize())
		{
			unsigned char val = pMMU->read8(addr);
			unsigned char res = val | regA_lo;
			regA_lo = res;
			regP.setNegative(res >> 7);
			regP.setZero(res == 0);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int val = (hi << 8) | lo;
			unsigned short int res = val | (regA_lo | (regA_hi << 8));

			regA_lo = res & 0xff;
			regA_hi = res >> 8;

			regP.setNegative(res >> 15);
			regP.setZero(res == 0);
			cycAdder = 1;
		}

		regPC += 3;
		cycles = 4 + cycAdder;
	}
	else if (nextOpcode == 0x5d)
	{
		// EOR addr,X
		int cycAdder = 0;
		unsigned int addr = getAbsoluteAddress16IndexedX();

		if (regP.getAccuMemSize())
		{
			unsigned char val = pMMU->read8(addr);
			unsigned char res = val ^ (regA_lo & 0xff);
			regA_lo = res;
			regP.setNegative(res >> 7);
			regP.setZero(res == 0);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned short int val = (hi << 8) | lo;
			unsigned short int res = val ^ (regA_lo | (regA_hi << 8));
			regA_lo = res & 0xff;
			regA_hi = res >> 8;
			regP.setNegative(res >> 15);
			regP.setZero(res == 0);
		}

		regPC += 3;
		cycles = 4 + cycAdder; // TODO check page boundary
	}
	else if (nextOpcode == 0xbc)
	{
		// LDY addr,X
		int cycAdder = 0;
		unsigned int addr = getAbsoluteAddress16IndexedX();

		if (regP.getIndexSize())
		{
			unsigned char lo = pMMU->read8(addr);
			regY_lo = lo;
			regP.setZero(lo == 0);
			regP.setNegative(lo >> 7);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned int val = (hi << 8) | lo;

			regY_lo = (val & 0xff);
			regY_hi = (unsigned char)(val >> 8);
			regP.setZero(val == 0);
			regP.setNegative(val >> 15);
			cycAdder += 1;
		}

		regPC += 3;
		cycles = 4 + cycAdder; // TODO check p.boundary
	}
	else if (nextOpcode == 0xda)
	{
		// PHB
		pushToStack8(regDBR);
		regPC += 1;
		cycles = 3;
	}
	else if (nextOpcode == 0x95)
	{
		// STA _dp_,X
		int cycAdder = 0;
		unsigned int addr = getDirectPageIndexedXAddress();

		pMMU->write8(addr,regA_lo);
		if (regP.getAccuMemSize()==0)
		{
			pMMU->write8(addr + 1,regA_hi);
			cycAdder += 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 4+cycAdder;
	}
	else if (nextOpcode == 0xfa)
	{
		// PLX
		int cycAdder = 0;
		
		if (regP.getIndexSize()) 
		{
			regX_lo=pullFromStack();
			regP.setNegative((regX_lo & 0x80) == 0x80);
			regP.setZero(regX_lo == 0x00);
		}
		else 
		{
			unsigned char lo = pullFromStack();
			unsigned char hi = pullFromStack();

			regX_lo = lo;
			regX_hi = hi;

			regP.setNegative(((regX_lo|(regX_hi<<8)) & 0x8000) == 0x8000);
			regP.setZero((regX_lo | (regX_hi << 8)) == 0x0000);
		}

		regPC += 1;
		cycles = 4 + cycAdder;
	}
	else if (nextOpcode == 0xf4)
	{
		// PEA addr
		unsigned int addr = getAbsoluteAddress16();
		pushToStack8((unsigned char)(addr >> 8));
		pushToStack8((unsigned char)(addr & 0xff));
		regPC += 3;
		cycles = 5;
	}
	else if (nextOpcode == 0xa3)
	{
		// LDA sr,S
		int cycAdder = 0;
		unsigned int addr = getStackRelative();

		if (regP.getAccuMemSize())
		{
			unsigned char lo = pMMU->read8(addr);
			regA_lo = lo;
			regP.setZero(lo == 0);
			regP.setNegative(lo >> 7);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned int val = (hi << 8) | lo;

			regA_lo = (val & 0xff);
			regA_hi = (val >> 8);
			regP.setZero(val == 0);
			regP.setNegative(val >> 15);
			cycAdder += 1;
		}

		regPC += 2;
		cycles = 4 + cycAdder;
	}
	else if (nextOpcode == 0x92)
	{
		// STA (dp)
		int cycAdder = 0;
		unsigned int addr = getDirectPageIndirectAddress();

		pMMU->write8(addr, regA_lo);
		if (regP.getAccuMemSize() == 0)
		{
			pMMU->write8((addr + 1) & 0xffff, regA_hi);
			cycAdder += 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 5 + cycAdder;
	}
	else if (nextOpcode == 0x26)
	{
		// ROL dp
		int cycAdder = 0;
		unsigned int addr = getDirectPageAddress();

		if (regP.getAccuMemSize()) 
		{
			unsigned char val = pMMU->read8(addr);
			unsigned char old_C = regP.getCarry();
			regP.setCarry(val >> 7);
			val = (val << 1) + old_C;
			pMMU->write8(addr,val);
			regP.setZero(val == 0);
			regP.setNegative(val >> 7);
		}
		else 
		{
			unsigned short int val = (pMMU->read8(addr + 1) << 8) | pMMU->read8(addr);
			unsigned char old_C = regP.getCarry();
			regP.setCarry(val >> 15);
			val = (val << 1) + old_C;
			pMMU->write8(addr, (unsigned char)val);
			pMMU->write8(addr + 1, val >> 8);
			regP.setZero(val == 0);
			regP.setNegative(val >> 15);
			cycAdder += 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 5 + cycAdder;
	}
	else if (nextOpcode == 0x97)
	{
		// STA [dp],Y
		int cycAdder = 0;
		unsigned int addr = getDirectPageIndirectLongIndexedYAddress();

		pMMU->write8(addr, regA_lo);
		if (regP.getAccuMemSize() == 0)
		{
			pMMU->write8(addr + 1, regA_hi);
			cycAdder = 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 6 + cycAdder;
	}
	else if (nextOpcode == 0xa4)
	{
		// LDY dp
		int cycAdder = 0;
		unsigned int addr = getDirectPageAddress();

		if (regP.getIndexSize())
		{
			unsigned char lo = pMMU->read8(addr);
			regY_lo = lo;
			regP.setZero(lo == 0);
			regP.setNegative(lo >> 7);
		}
		else
		{
			unsigned char lo = pMMU->read8(addr);
			unsigned char hi = pMMU->read8(addr + 1);
			unsigned int val = (hi << 8) | lo;

			regY_lo = (val & 0xff);
			regY_hi = (unsigned char)(val >> 8);
			regP.setZero(val == 0);
			regP.setNegative(val >> 15);
			cycAdder += 1;
		}

		if (regP.isDPLowNotZero()) cycAdder += 1;

		regPC += 2;
		cycles = 3 + cycAdder;
	}
	else
	{
		// unknown opcode
		std::stringstream strr;
		strr << std::hex << std::setw(2) << std::setfill('0') << (int)nextOpcode;
		std::stringstream staddr;
		staddr << std::hex << std::setw(6) << std::setfill('0') << regPC;
		glbTheLogger.logMsg("Unknown opcode [" + strr.str()+"] at 0x"+staddr.str());
		return -1;
	}

	return cycles;
}

cpu5a22::~cpu5a22()
{
}
