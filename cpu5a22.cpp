
/* 

	our Ricoh 5A22 CPU - derived from WDC 65C816 
	a lot of code "inspired" by
	https://github.com/LilaQ/q00.snes/

*/

#include "cpu5a22.h"
#include "debugger5a22.h"

cpu5a22::cpu5a22(mmu* theMMU)
{
	pMMU = theMMU;
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

	// D,DBR

	s = "";
	strr.str(""); strr.clear();
	strr << std::hex << std::setw(4) << std::setfill('0') << (int)regD;
	s += "D:" + strr.str();

	strr.str(""); strr.clear();
	strr << std::hex << std::setw(4) << std::setfill('0') << (int)regDBR;
	s += " DB:" + strr.str();

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

}

unsigned char cpu5a22::pullFromStack()
{
	unsigned char val = pMMU->read8(regSP+1);
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
	unsigned char dbr = regDBR;
	unsigned short int addr = ((pMMU->read8(regPC+2) << 8) | pMMU->read8(regPC+1));
	return (dbr << 16) | addr;
}

unsigned int cpu5a22::getLongAddress()
{
	// TODO check this
	// unsigned char dbr = regDBR;
	return ((pMMU->read8(regPC+3)) << 16) | (pMMU->read8(regPC + 2) << 8) | pMMU->read8(regPC+1);
}

int cpu5a22::stepOne()
{
	unsigned char nextOpcode = pMMU->read8(regPC);
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

		if (tmp == 0) 
		{	
			//	if emulation mode
			regP.setAccuMemSize(1);
			regP.setIndexSize(1, regX_hi, regY_hi);
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
		const unsigned char nextByte = pMMU->read8(regPC + 1);

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
		const unsigned char nextByte = pMMU->read8(regPC + 1);

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

		int ics = regX_lo | (regX_hi << 8);
		ics -= 1;
		ics &= 0xffff;

		regX_lo = ics & 0xff;
		regX_hi = (unsigned char)(ics >> 8);

		regP.setZero(ics == 0);
		regP.setNegative(ics >> (7 + ((1 - regP.getIndexSize()) * 8)));

		regPC += 1;
		cycles += 2;
	}
	else if (nextOpcode == 0xd0)
	{
		// BNE - branch if not equal (Z==0)

		int branch_taken = 0;
		
		signed char offset = (signed char)pMMU->read8(regPC + 1);
		
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
		int addr = getLongAddress();

		unsigned char bnk = (addr >> 16) & 0xff;
		regPC = addr;
		regPB = bnk;

		cycles = 4;
	}
	else if (nextOpcode == 0x2c)
	{
		// BIT - Test bits Absolute
		int cycleAdder = 0;

		if (regP.getAccuMemSize() == 0)
		{
			cycleAdder += 1;
		}



		regPC += 3;
		cycles = 4 + cycleAdder;
	}
	else
	{
		// unknown opcode, do something
	}

	return cycles;
}

cpu5a22::~cpu5a22()
{
}
