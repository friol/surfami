/* our littley apu and SPC700 chip */

#include "apu.h"
#include <cstdlib> 
#include <fstream>
#include <vector>
#include <iomanip>
#include <sstream>

extern logger glbTheLogger;

apu::apu()
{
	// allocate memory
	spc700ram = new unsigned char[0x10000];

	for (int i = 0;i < 0x10000;i++)
	{
		spc700ram[i] = 0;
	}

	read8 = &apu::internalRead8;
	write8 = &apu::internalWrite8;

	// load ipl rom
	std::vector<unsigned char> vec;
	std::string bootRomName = "roms/ipl.rom";

	std::ifstream file(bootRomName, std::ios::binary);
	if (!file)
	{
		return;
	}

	file.unsetf(std::ios::skipws);

	std::streampos fileSize;
	file.seekg(0, std::ios::end);
	fileSize = file.tellg();

	if (fileSize != 64)
	{
		return;
	}

	file.seekg(0, std::ios::beg);

	vec.reserve(fileSize);
	vec.insert(vec.begin(), std::istream_iterator<unsigned char>(file), std::istream_iterator<unsigned char>());

	for (int i = 0;i < 64;i++)
	{
		bootRom[i] = vec[i];
	}

	bootRomLoaded = true;

	// the ipl rom goes to 0xffc0-0xffff in the SPC ram
	for (int i = 0;i < 64;i++)
	{
		spc700ram[0xffc0 + i] = bootRom[i];
	}

	for (int i = 0;i < 4;i++)
	{
		portsFromCPU[i] = 0;
		portsFromSPC[i] = 0;
	}

	// init timers
	for (int i = 0; i < 3; i++) 
	{
		timer[i].cycles = 0;
		timer[i].divider = 0;
		timer[i].target = 0;
		timer[i].counter = 0;
		timer[i].enabled = false;
	}
}

void apu::useTestMMU()
{
	read8 = &apu::testMMURead8;
	write8 = &apu::testMMUWrite8;
}

bool apu::isBootRomLoaded()
{
	return bootRomLoaded;
}

void apu::reset()
{
	//glbTheLogger.logMsg("APU reset");

	regA = 0;
	regX = 0;
	regY = 0;
	regSP = 0;

	flagN = false;
	flagV = false;
	flagP = false;
	flagB = false;
	flagH = false;
	flagI = false;
	flagZ = false;
	flagC = false;

	unsigned char pcLow = spc700ram[0xfffe];
	unsigned char pcHi = spc700ram[0xffff];
	regPC = pcLow | (pcHi << 8);

	std::stringstream strstrdgb;
	strstrdgb << std::hex << std::setw(4) << std::setfill('0') << (int)regPC;
	std::string sInitialPC = strstrdgb.str();

	//glbTheLogger.logMsg("SPC700 initial PC: 0x"+ sInitialPC);
}

void apu::setState(unsigned short int initialPC, unsigned char initialA, unsigned char initialX, unsigned char initialY, unsigned short int initialSP, unsigned char initialFlags)
{
	regPC = initialPC;
	regA = initialA;
	regX = initialX;
	regY = initialY;
	regSP = initialSP;

	if (initialFlags & 0x80) flagN = true;
	if (initialFlags & 0x40) flagV = true;
	if (initialFlags & 0x20) flagP = true;
	if (initialFlags & 0x10) flagB = true;
	if (initialFlags & 0x08) flagH = true;
	if (initialFlags & 0x04) flagI = true;
	if (initialFlags & 0x02) flagZ = true;
	if (initialFlags & 0x01) flagC = true;
}

std::vector<std::string> apu::getRegisters()
{
	std::vector<std::string> result;

	std::string row0;

	std::stringstream strstrdgb;
	strstrdgb << std::hex << std::setw(2) << std::setfill('0') << (int)regA;
	row0+="A:"+strstrdgb.str();

	std::stringstream strstrdgb2;
	strstrdgb2 << std::hex << std::setw(2) << std::setfill('0') << (int)regX;
	row0 += " X:" + strstrdgb2.str();

	std::stringstream strstrdgb3;
	strstrdgb3 << std::hex << std::setw(2) << std::setfill('0') << (int)regY;
	row0 += " Y:" + strstrdgb3.str();

	std::stringstream strstrdgb4;
	strstrdgb4 << std::hex << std::setw(4) << std::setfill('0') << (int)regPC;
	row0 += " PC:" + strstrdgb4.str();

	std::stringstream strstrdgb5;
	strstrdgb5 << std::hex << std::setw(4) << std::setfill('0') << (int)regSP;
	row0 += " SP:" + strstrdgb5.str();

	result.push_back(row0);
	result.push_back("");
	result.push_back("NVPBHIZC");

	std::string flagsBitstring;

	if (flagN) flagsBitstring += "1";
	else flagsBitstring += "0";
	if (flagV) flagsBitstring += "1";
	else flagsBitstring += "0";
	if (flagP) flagsBitstring += "1";
	else flagsBitstring += "0";
	if (flagB) flagsBitstring += "1";
	else flagsBitstring += "0";
	if (flagH) flagsBitstring += "1";
	else flagsBitstring += "0";
	flagsBitstring += "0";
	if (flagZ) flagsBitstring += "1";
	else flagsBitstring += "0";
	if (flagC) flagsBitstring += "1";
	else flagsBitstring += "0";

	result.push_back(flagsBitstring);

	// ports f4-f5-f6-f7
	std::string portsStr0="Port0:";
	std::stringstream strstrP0;
	strstrP0 << std::hex << std::setw(2) << std::setfill('0') << (int)portsFromCPU[0];
	portsStr0+=strstrP0.str();
	std::string portsStr1 = "Port1:";
	std::stringstream strstrP1;
	strstrP1 << std::hex << std::setw(2) << std::setfill('0') << (int)portsFromCPU[1];
	portsStr1+=strstrP1.str();
	std::string portsStr2 = "Port2:";
	std::stringstream strstrP2;
	strstrP2 << std::hex << std::setw(2) << std::setfill('0') << (int)portsFromCPU[2];
	portsStr2 += strstrP2.str();
	std::string portsStr3 = "Port3:";
	std::stringstream strstrP3;
	strstrP3 << std::hex << std::setw(2) << std::setfill('0') << (int)portsFromCPU[3];
	portsStr3 += strstrP2.str();

	result.push_back(portsStr0);
	result.push_back(portsStr1);
	result.push_back(portsStr2);
	result.push_back(portsStr3);

	return result;
}

unsigned char apu::getPSW()
{
	unsigned char retval = 0;

	if (flagN) retval |= 0x80;
	if (flagV) retval |= 0x40;
	if (flagP) retval |= 0x20;
	if (flagB) retval |= 0x10;
	if (flagH) retval |= 0x08;
	if (flagI) retval |= 0x04;
	if (flagZ) retval |= 0x02;
	if (flagC) retval |= 0x01;

	return retval;
}

unsigned char apu::testMMURead8(unsigned int addr)
{
	return spc700ram[addr];
}

void apu::testMMUWrite8(unsigned int addr, unsigned char val)
{
	spc700ram[addr] = val;
}

unsigned char apu::externalRead8(unsigned int address)
{
	if ((address == 0x2140) || (address == 0x2141) || (address == 0x2142) || (address == 0x2143))
	{
		// 2140/1/2/3h - APUI00 - Main CPU to Sound CPU Communication Port 0/1/2/3 (R / W)
		unsigned char value = portsFromSPC[address-0x2140];
		return value;
	}
}

void apu::externalWrite8(unsigned int address, unsigned char val)
{
	if ((address == 0x2140) || (address == 0x2141) || (address == 0x2142) || (address == 0x2143))
	{
		// 2140/1/2/3h - APUI00 - Main CPU to Sound CPU Communication Port 0/1/2/3 (R / W)
		portsFromCPU[address - 0x2140] = val;
	}
}

unsigned char apu::internalRead8(unsigned int address)
{
	if ((address >= 0xf4) && (address <= 0xf7))
	{
		return portsFromCPU[address - 0xf4];
	}
	else if ((address==0xfd)||(address==0xfe)||(address == 0xff))
	{
		unsigned char ret = timer[address - 0xfd].counter;
		timer[address - 0xfd].counter = 0;
		return ret;
	}
	else
	{
		return spc700ram[address];
	}
}

void apu::internalWrite8(unsigned int address, unsigned char val)
{
	if ((address >= 0xf4) && (address <= 0xf7))
	{
		portsFromSPC[address - 0xf4]=val;
	}
	else if (address == 0xf1)
	{
		// 00F1h - CONTROL - Timer, I/O and ROM Control (W)
		/*
		  0-2  Timer 0-2 Enable (0=Disable, set TnOUT=0 & reload divider, 1=Enable)
		  3    Not used
		  4    Reset Port 00F4h/00F5h Input-Latches (0=No change, 1=Reset to 00h)
		  5    Reset Port 00F6h/00F7h Input-Latches (0=No change, 1=Reset to 00h)
				Note: The CPUIO inputs are latched inside of the SPC700 (at time when
				the Main CPU writes them), above two bits allow to reset these latches.
		  6    Not used
		  7    ROM at FFC0h-FFFFh (0=RAM, 1=ROM) (writes do always go to RAM)		
		*/
		glbTheLogger.logMsg("apu::write [" + std::to_string(val) + "] to f1 (CONTROL)");

		for (int i = 0; i < 3; i++) 
		{
			if (!timer[i].enabled && (val & (1 << i))) 
			{
				timer[i].divider = 0;
				timer[i].counter = 0;
			}
			timer[i].enabled = val & (1 << i);
		}

		// ports
		if (val & 0x10) 
		{
			portsFromSPC[0] = 0;
			portsFromSPC[1] = 0;
		}
		if (val & 0x20) 
		{
			portsFromSPC[2] = 0;
			portsFromSPC[3] = 0;
		}

		romReadable = val & 0x80;
		return;
	}
	else if ((address==0xfa)||(address==0xfb)||(address == 0xfc))
	{
		// 00FAh - T0DIV - Timer 0 Divider(for 8000Hz clock source) (W)
		// 00FBh - T1DIV - Timer 1 Divider(for 8000Hz clock source) (W)
		// 00FCh - T2DIV - Timer 2 Divider(for 64000Hz clock source) (W)
		timer[address - 0xfa].target = val;
		return;
	}
	else
	{
		spc700ram[address]=val;
	}
}

void apu::step()
{
	//if ((apu->cycles & 0x1f) == 0) 
	//{
	//	dsp_cycle(apu->dsp);
	//}

	for (int i = 0; i < 3; i++) 
	{
		if (timer[i].cycles == 0) 
		{
			timer[i].cycles = i == 2 ? 16 : 128;
			if (timer[i].enabled) 
			{
				timer[i].divider++;
				if (timer[i].divider == timer[i].target) 
				{
					timer[i].divider = 0;
					timer[i].counter++;
					timer[i].counter &= 0xf;
				}
			}
		}
		timer[i].cycles--;
	}

	apuCycles++;
}

void apu::doFlagsNZ(unsigned char val)
{
	flagN = (val >> 7);
	flagZ = (val == 0);
}

void apu::doMoveToX(pAddrModeFun fn)
{
	unsigned short int addr=(this->*fn)();
	unsigned char val = (this->*read8)(addr);
	regX = val;
	doFlagsNZ(regX);
}

void apu::doMoveToY(pAddrModeFun fn)
{
	unsigned short int addr = (this->*fn)();
	unsigned char val = (this->*read8)(addr);
	regY = val;
	doFlagsNZ(regY);
}

void apu::doMoveToA(pAddrModeFun fn)
{
	unsigned short int addr = (this->*fn)();
	unsigned char val = (this->*read8)(addr);
	regA = val;
	doFlagsNZ(regA);
}

void apu::doDecX()
{
	regX -= 1;
	doFlagsNZ(regX);
}

void apu::doMoveWithRead(pAddrModeFun fn, unsigned char val)
{
	unsigned short int addr = (this->*fn)();
	(this->*read8)(addr);
	(this->*write8)(addr,val);
}

void apu::doCMP(pAddrModeFun fn, unsigned char val)
{
	unsigned short int addr = (this->*fn)();
	unsigned char vil = (this->*read8)(addr);

	val ^= 0xff;
	int result = vil + val + 1;
	flagC = result > 0xff;
	doFlagsNZ((unsigned char)result);
}

void apu::doCMPX(pAddrModeFun fn)
{
	unsigned short int addr = (this->*fn)();
	unsigned char vil = (this->*read8)(addr);

	vil ^= 0xff;
	int result = vil + regX + 1;
	flagC = result > 0xff;
	doFlagsNZ((unsigned char)result);
}

void apu::doCMPY(pAddrModeFun fn)
{
	unsigned short int addr = (this->*fn)();
	unsigned char vil = (this->*read8)(addr);

	vil ^= 0xff;
	int result = vil + regY + 1;
	flagC = result > 0xff;
	doFlagsNZ((unsigned char)result);
}

int apu::doBNE()
{
	signed char off = (this->*read8)(regPC+1);
	if (!flagZ) 
	{
		regPC += off+2;
		return 4;
	}

	regPC += 2;
	return 2;
}

void apu::doInc(pAddrModeFun fn)
{
	unsigned short int addr = (this->*fn)();
	unsigned char val = (this->*read8)(addr);
	val += 1;
	(this->*write8)(addr, val);
	doFlagsNZ(val);
}

int apu::doBranch(signed char offs, bool condition)
{
	if (condition)
	{
		regPC += offs+2;
		return 4;
	}
	else
	{
		regPC += 2;
		return 2;
	}
}

void apu::doAdc(pAddrModeFun fn)
{
	unsigned short int addr = (this->*fn)();
	unsigned char value = (this->*read8)(addr);
	int fc = flagC ? 1 : 0;
	int result = regA + value + fc;
	flagV = (regA & 0x80) == (value & 0x80) && (value & 0x80) != (result & 0x80);
	flagH = ((regA & 0xf) + (value & 0xf) + fc) > 0xf;
	flagC = result > 0xff;
	regA = result;
	doFlagsNZ(regA);
}

// addressing modez

unsigned short int apu::addrModePC()
{
	return regPC + 1;
}

unsigned short int apu::addrModeX()
{
	unsigned short int adr = regX;
	if (flagP) adr |= 0x100;
	return adr;
}

unsigned short int apu::addrDP()
{
	unsigned short int adr = (this->*read8)(regPC + 1);
	if (flagP) adr |= 0x100;
	return adr;
}

unsigned short int apu::addrIndirectY()
{
	unsigned char pointer = (this->*read8)(regPC + 1);

	unsigned short int adr = 0;
	unsigned short int low = pointer;
	if (flagP) low |= 0x100;

	unsigned short int high = (pointer+1)&0xff;
	if (flagP) high |= 0x100;

	adr= (this->*read8)(low);
	adr |= (this->*read8)(high) << 8;;

	return (adr + regY) & 0xffff;
}

unsigned short int apu::addrImmediate8()
{
	unsigned short int adr = (this->*read8)(regPC+2);
	if (flagP) adr |= 0x100;
	return adr;
}

unsigned short int apu::addrAbs()
{
	unsigned char lowaddr = (this->*read8)(regPC + 1);
	unsigned char highaddr = (this->*read8)(regPC + 2);
	unsigned short int addr = lowaddr | (highaddr << 8);
	return addr;
}

unsigned short int apu::addrAbsX()
{
	unsigned char lowaddr = (this->*read8)(regPC + 1);
	unsigned char highaddr = (this->*read8)(regPC + 2);
	unsigned short int addr = lowaddr | (highaddr << 8);
	addr = (addr + regX) & 0xffff;
	return addr;
}

unsigned short int apu::addrDPX()
{
	unsigned short int adr = ((this->*read8)(regPC + 1)+regX)&0xff;
	if (flagP) adr |= 0x100;
	return adr;
}

int apu::stepOne()
{
	unsigned char nextOpcode = (this->*read8)(regPC);
	int cycles = 0;

	switch (nextOpcode)
	{
		case 0xcd:
		{
			// MOV X,imm
			doMoveToX(&apu::addrModePC);
			regPC += 2;
			cycles = 2;
			break;
		}
		case 0xbd:
		{
			// MOV SP,X
			regSP = regX; 
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0xe8:
		{
			// MOV A,imm
			doMoveToA(&apu::addrModePC);
			regPC += 2;
			cycles = 2;
			break;
		}
		case 0xc6:
		{
			// MOV (X),A
			doMoveWithRead(&apu::addrModeX, regA);
			regPC += 1;
			cycles = 4;
			break;
		}
		case 0x1d:
		{
			// DEC X
			doDecX();
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0xd0:
		{
			// BNE rel
			cycles = doBNE();
			break;
		}
		case 0x8f:
		{
			// MOV dp, #imm
			unsigned char operand = (this->*read8)(regPC + 1);
			doMoveWithRead(&apu::addrImmediate8, operand);
			regPC += 3;
			cycles = 5;
			break;
		}
		case 0x78:
		{
			// CMP d,#i
			unsigned char operand = (this->*read8)(regPC + 1);
			doCMP(&apu::addrImmediate8, operand);
			regPC += 3;
			cycles = 5;
			break;
		}
		case 0x2f:
		{
			// BRA imm
			signed char offs = (this->*read8)(regPC + 1);
			regPC += offs+2;
			cycles = 4;
			break;
		}
		case 0xba:
		{
			// MOVW YA,(addr)
			unsigned char adr = (this->*read8)(regPC + 1);
			unsigned short int low = adr;
			if (flagP) low |= 0x100;
			unsigned short int high = (adr+1) & 0xff;
			if (flagP) high |= 0x100;
			unsigned char val1 = (this->*read8)(low);
			unsigned short int val = val1 | ((this->*read8)(high)<<8);
			regA = val & 0xff;
			regY = val >> 8;
			flagZ = val == 0;
			flagN = val & 0x8000;

			regPC += 2;
			cycles = 5;
			break;
		}
		case 0xda:
		{
			// MOVW dp,YA
			unsigned char adr = (this->*read8)(regPC + 1);
			unsigned short int low = adr;
			if (flagP) low |= 0x100;
			unsigned short int high = (adr + 1) & 0xff;
			if (flagP) high |= 0x100;

			(this->*read8)(low);
			
			(this->*write8)(low, regA);
			(this->*write8)(high, regY);

			regPC += 2;
			cycles = 5;
			break;
		}
		case 0xc4:
		{
			// mov d,a
			doMoveWithRead(&apu::addrDP, regA);
			regPC += 2;
			cycles = 4;
			break;
		}
		case 0xdd:
		{
			// mov a,y
			regA = regY;
			doFlagsNZ(regA);
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0x5d:
		{
			// mov x,a
			regX = regA;
			doFlagsNZ(regX);
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0xeb:
		{
			// mov y,d
			doMoveToY(&apu::addrDP);
			regPC += 2;
			cycles = 3;
			break;
		}
		case 0x7e:
		{
			// CMP y,d
			doCMPY(&apu::addrDP);
			regPC += 2;
			cycles = 3;
			break;
		}
		case 0xe4:
		{
			// mov a,d
			doMoveToA(&apu::addrDP);
			regPC += 2;
			cycles = 3;
			break;
		}
		case 0xcb:
		{
			// mov d,Y
			doMoveWithRead(&apu::addrDP, regY);
			regPC += 2;
			cycles = 4;
			break;
		}
		case 0xd7:
		{
			// mov [d]+Y,A
			doMoveWithRead(&apu::addrIndirectY, regA);
			regPC += 2;
			cycles = 7;
			break;
		}
		case 0xfc:
		{
			// inc y
			regY += 1;
			doFlagsNZ(regY);
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0xab:
		{
			// inc d
			doInc(&apu::addrDP);
			regPC += 2;
			cycles = 4;
			break;
		}
		case 0x10:
		{
			// BPL offs
			signed char offs = (this->*read8)(regPC + 1);
			cycles=doBranch(offs,!flagN);
			break;
		}
		case 0x1f:
		{
			// jmp [addr+x]
			unsigned char lowaddr, highaddr;
			lowaddr= (this->*read8)(regPC + 1);
			highaddr= (this->*read8)(regPC + 2);

			unsigned short int ptrAddr = lowaddr | (highaddr << 8);

			unsigned char lownewAddr, highnewAddr;
			lownewAddr= (this->*read8)((ptrAddr + regX)&0xffff);
			highnewAddr= (this->*read8)((ptrAddr + regX+1)&0xffff);

			regPC = lownewAddr | (highnewAddr << 8);

			cycles = 6;
			break;
		}
		case 0x20:
		{
			// clrp
			flagP = false;
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0xc5:
		{
			// mov !a,A
			doMoveWithRead(&apu::addrAbs, regA);
			regPC += 3;
			cycles = 5;
			break;
		}
		case 0xaf:
		{
			// mov (x)+,a
			unsigned short int adr = regX;
			if (flagP) adr |= 0x100;
			regX += 1;
			(this->*write8)(adr, regA);
			regPC += 1;
			cycles = 4;
			break;
		}
		case 0xc8:
		{
			// CMP x,imm
			doCMPX(&apu::addrModePC);
			regPC += 2;
			cycles = 2;
			break;
		}
		case 0xd5:
		{
			// MOV !a+X, A
			doMoveWithRead(&apu::addrAbsX, regA);
			regPC += 3;
			cycles = 6;
			break;
		}
		case 0x3d:
		{
			// INC X
			regX += 1;
			doFlagsNZ(regX);
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0xf5:
		{
			// MOV A,!a+X
			unsigned short int addr = addrAbsX();
			unsigned char val = (this->*read8)(addr);
			regA = val;
			doFlagsNZ(regA);
			regPC += 3;
			cycles = 5;
			break;
		}
		case 0xfd:
		{
			// MOV Y,A
			regY = regA;
			doFlagsNZ(regY);
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0x3f:
		{
			// CALL !addr
			unsigned short int dst;
			unsigned char lowaddr, highaddr;
			lowaddr = (this->*read8)(regPC + 1);
			highaddr = (this->*read8)(regPC + 2);
			dst = lowaddr | (highaddr << 8);

			(this->*write8)(0x100 | regSP, (regPC+3)>>8);
			regSP--;
			regSP &= 0xff;
			(this->*write8)(0x100 | regSP, (regPC+3)&0xff);
			regSP--;
			regSP &= 0xff;

			regPC = dst;
			cycles = 8;
			break;
		}
		case 0xcc:
		{
			// MOV !a,Y
			unsigned short int addr = addrAbs();
			(this->*write8)(addr, regY);
			regPC += 3;
			cycles = 5;
			break;
		}
		case 0x6f:
		{
			// RET
			unsigned char low, high;
			regSP++; regSP &= 0xff;
			low=(this->*read8)(0x100 | regSP);
			regSP++; regSP &= 0xff;
			high = (this->*read8)(0x100 | regSP);
			regPC = low | (high << 8);
			cycles = 5;
			break;
		}
		case 0xec:
		{
			// MOV Y,!a
			doMoveToY(&apu::addrAbs);
			regPC += 3;
			cycles = 4;
			break;
		}
		case 0xf0:
		{
			// BEQ offs
			signed char offs = (this->*read8)(regPC + 1);
			cycles = doBranch(offs,flagZ);
			break;
		}
		case 0x6d:
		{
			// PUSH Y
			(this->*write8)(0x100|regSP, regY);
			regSP--;
			regSP &= 0xff;
			regPC += 1;
			cycles = 4;
			break;
		}
		case 0xcf:
		{
			// MUL YA
			unsigned short int result = regA*regY;
			regA = result & 0xff;
			regY = result >> 8;
			doFlagsNZ(regY);
			regPC += 1;
			cycles = 9;
			break;
		}
		case 0x60:
		{
			// CLRC
			flagC = false;
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0x84:
		{
			// ADC A,dp
			doAdc(&apu::addrDP);
			regPC += 2;
			cycles = 3;
			break;
		}
		case 0x90:
		{
			// BCC offs
			signed char offs = (this->*read8)(regPC + 1);
			cycles= doBranch(offs, !flagC);
			break;
		}
		case 0xee:
		{
			// POP Y 
			regSP += 1; regSP &= 0xff;
			regY=(this->*read8)(0x100 | regSP);
			regPC += 1;
			cycles = 4;
			break;
		}
		case 0x30:
		{
			// BMI offs
			signed char offs = (this->*read8)(regPC + 1);
			cycles = doBranch(offs, flagN);
			break;
		}
		case 0xe5:
		{
			// MOV A,!a
			doMoveToA(&apu::addrAbs);
			regPC += 3;
			cycles = 4;
			break;
		}
		case 0x7d:
		{
			// MOV A,X
			regA = regX;
			doFlagsNZ(regA);
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0xf4:
		{
			// MOV A,d+X
			doMoveToA(&apu::addrDPX);
			regPC += 2;
			cycles = 4;
			break;
		}
		default:
		{
			// unknown opcode
			std::stringstream strr;
			strr << std::hex << std::setw(2) << std::setfill('0') << (int)nextOpcode;
			std::stringstream staddr;
			staddr << std::hex << std::setw(4) << std::setfill('0') << regPC;
			glbTheLogger.logMsg("Unknown SPC700 opcode [" + strr.str() + "] at 0x" + staddr.str());
			return -1;
		}
	}

	return cycles;
}

apu::~apu()
{
	delete(spc700ram);
}
