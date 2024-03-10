/* our littley apu, dsp and SPC700 chip, all together */

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

	// init DSP channels
	for (int i = 0;i < 8;i++)
	{
		channels[i].keyOn = false;
		channels[i].keyOff= true;
		channels[i].leftVol = 0;
		channels[i].rightVol = 0;
		channels[i].samplePitch = 0;
		channels[i].sampleSourceEntry = 0;
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

	unsigned char pcLow = internalRead8(0xfffe);
	unsigned char pcHi = internalRead8(0xffff);
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

void apu::calcBRRSampleStart(int voiceNum)
{
	unsigned short int dirAddress = dspDIR << 8;
	unsigned short int BRRSampleStart = 0;
	unsigned short int BRRLoopStart = 0;

	dirAddress += (channels[voiceNum].sampleSourceEntry)*4;

	int low = spc700ram[dirAddress];
	int high = spc700ram[dirAddress+1];

	BRRSampleStart = (unsigned short int)(low | (high << 8));

	std::stringstream strstrSS;
	strstrSS << std::hex << std::setw(4) << std::setfill('0') << (int)BRRSampleStart;

	glbTheLogger.logMsg("apu::dsp BRR sample block starts at address [" + strstrSS.str()+"]");

	channels[voiceNum].BRRSampleStart = BRRSampleStart;

	low = spc700ram[dirAddress+2];
	high = spc700ram[dirAddress + 3];
	BRRLoopStart = (unsigned short int)(low | (high << 8));

	std::stringstream strstrLS;
	strstrLS << std::hex << std::setw(4) << std::setfill('0') << (int)BRRLoopStart;

	glbTheLogger.logMsg("apu::dsp BRR loop starts at address [" + strstrLS.str() + "]");

	channels[voiceNum].BRRLoopStart = BRRLoopStart;
}

void apu::writeToDSPRegister(unsigned char val)
{
	std::stringstream strstrVal;
	strstrVal << std::hex << std::setw(2) << std::setfill('0') << (int)val;

	std::stringstream sDspReg;
	sDspReg << std::hex << std::setw(2) << std::setfill('0') << (int)dspSelectedRegister;

	if (dspSelectedRegister == 0x0c)
	{
		glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to MVOL (L)");
		mainVolLeft = (signed char)val;
	}
	else if (dspSelectedRegister == 0x1c)
	{
		glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to MVOL (R)");
		mainVolRight = (signed char)val;
	}
	else if (dspSelectedRegister == 0x2c)
	{
		glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to EVOL (L)");
		echoVolLeft = (signed char)val;
	}
	else if (dspSelectedRegister == 0x3c)
	{
		glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to EVOL (R)");
		echoVolRight = (signed char)val;
	}
	else if (dspSelectedRegister == 0x4c)
	{
		glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to KEYON");
		for (int b = 0;b < 8;b++)
		{
			if (val & (1 << b))
			{
				channels[b].keyOn = true;
			}
		}
	}
	else if (dspSelectedRegister == 0x5c)
	{
		glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to KEYOFF");
		for (int b = 0;b < 8;b++)
		{
			if (val & (1 << b))
			{
				channels[b].keyOff = true;
			}
		}
	}
	else if (dspSelectedRegister == 0x6c)
	{
		glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to FLAGREG");
		dspFlagReg= val;
	}
	else if (dspSelectedRegister == 0x5d)
	{
		glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to DIR 5d sample source directory page");
		dspDIR = val;
	}
	else if (dspSelectedRegister == 0x6d)
	{
		glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to ESA 6d start echo mem reg");
	}
	else if (dspSelectedRegister == 0x7d)
	{
		glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to 7d echo delay time");
	}
	else if ((dspSelectedRegister & 0x0f) == 0)
	{
		// left channel volume for voice
		int voiceNum = (dspSelectedRegister & 0xf0) >> 4;
		glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] VOL (L) voice "+std::to_string(voiceNum));
		channels[voiceNum].leftVol = (signed char)val;
	}
	else if ((dspSelectedRegister & 0x0f) == 1)
	{
		// right channel volume for voice
		int voiceNum = (dspSelectedRegister & 0xf0) >> 4;
		glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] VOL (R) voice " + std::to_string(voiceNum));
		channels[voiceNum].rightVol = (signed char)val;
	}
	else if ((dspSelectedRegister & 0x0f) == 2)
	{
		// low 8 bits of sample pitch
		int voiceNum = (dspSelectedRegister & 0xf0) >> 4;
		glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] PITCH (L) voice " + std::to_string(voiceNum));
		channels[voiceNum].samplePitch |= val;
	}
	else if ((dspSelectedRegister & 0x0f) == 3)
	{
		// high 6 bits of sample pitch
		int voiceNum = (dspSelectedRegister & 0xf0) >> 4;
		glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] PITCH (H) voice " + std::to_string(voiceNum));
		channels[voiceNum].samplePitch |= (val<<8);
	}
	else if ((dspSelectedRegister & 0x0f) == 4)
	{
		// sample source entry from DIR
		int voiceNum = (dspSelectedRegister & 0xf0) >> 4;
		glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] SCRN voice " + std::to_string(voiceNum));
		channels[voiceNum].sampleSourceEntry = val;
		calcBRRSampleStart(voiceNum);
	}
	else
	{
		glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to unmapped register "+ sDspReg.str());
	}

	dspRam[dspSelectedRegister] = val;
}

// 

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

	return 0;
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
	std::stringstream strstrAddr;
	strstrAddr << std::hex << std::setw(4) << std::setfill('0') << (int)address;

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
	else if ((address == 0xf0)|| (address == 0xf1)|| (address == 0xfa)|| (address == 0xfb)|| (address == 0xfc))
	{
		return 0;
	}
	else if (address == 0xf2)
	{
		return dspSelectedRegister;
	}
	else if (address == 0xf3)
	{
		return dspRam[dspSelectedRegister&0x7f];
	}
	else if ((address >= 0xf0) && (address <= 0xff))
	{
		glbTheLogger.logMsg("apu::unmapped read from register "+ strstrAddr.str());
	}
	else if ((address >= 0xffc0) && (address <= 0xffff))
	{
		if (romReadable)
		{
			return bootRom[address - 0xffc0];
		}
		else
		{
			return spc700ram[address];
		}
	}
	else
	{
		return spc700ram[address];
	}

	return 0;
}

void apu::internalWrite8(unsigned int address, unsigned char val)
{
	if (address == 0xf0)
	{
		// 00F0h - TEST - Testing functions (W)
		glbTheLogger.logMsg("apu::write [" + std::to_string(val) + "] to f0 (should not happen)");
		return;
	}
	else if ((address >= 0xf4) && (address <= 0xf7))
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
		//glbTheLogger.logMsg("apu::write [" + std::to_string(val) + "] to f1 (CONTROL)");

		for (int i = 0; i < 3; i++) 
		{
			if (!timer[i].enabled && (val & (1 << i))) 
			{
				timer[i].divider = 0;
				timer[i].counter = 0;
			}
			timer[i].enabled = val & (1 << i);
		}

		if (val & 0x10) 
		{
			portsFromCPU[0] = 0;
			portsFromCPU[1] = 0;
		}
		if (val & 0x20) 
		{
			portsFromCPU[2] = 0;
			portsFromCPU[3] = 0;
		}

		romReadable = val & 0x80;
		//if (!(val & 0x80))
		//{
		//	glbTheLogger.logMsg("apu::warning ROM not readable");
		//}

		return;
	}
	else if (address == 0xf2)
	{
		// 00F2h - DSPADDR - DSP Register Index (R/W)
		std::stringstream strstrReg;
		strstrReg << std::hex << std::setw(2) << std::setfill('0') << (int)val;
		//glbTheLogger.logMsg("apu::write [" + strstrReg.str() + "] to f2 (DSPADDR)");
		
		dspSelectedRegister = val;
		
		return;
	}
	else if (address == 0xf3)
	{
		// 00F3h - DSPDATA - DSP Register Data (R/W)
		std::stringstream strstrReg;
		strstrReg << std::hex << std::setw(2) << std::setfill('0') << (int)val;
		//glbTheLogger.logMsg("apu::wrote [" + strstrReg.str() + "] to f3 (DSPDATA)");
		if (dspSelectedRegister < 0x80) writeToDSPRegister(val);
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
	else if ((address >= 0xf0) && (address <= 0xff))
	{
		glbTheLogger.logMsg("apu::unmapped write to register " + std::to_string(address));
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

void apu::doCMPA(pAddrModeFun fn)
{
	unsigned short int addr = (this->*fn)();
	unsigned char vil = (this->*read8)(addr);

	vil ^= 0xff;
	int result = vil + regA + 1;
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
	regA = (unsigned char)result;
	doFlagsNZ(regA);
}

void apu::doEor(pAddrModeFun fn)
{
	unsigned short int addr = (this->*fn)();
	unsigned char value = (this->*read8)(addr);
	regA ^= value;
	doFlagsNZ(regA);
}

void apu::doLsr(pAddrModeFun fn)
{
	unsigned short int addr = (this->*fn)();
	unsigned char val = (this->*read8)(addr);
	flagC = val & 1;
	val >>= 1;
	(this->*write8)(addr, val);
	doFlagsNZ(val);
}

void apu::doAnd(pAddrModeFun fn)
{
	unsigned short int addr = (this->*fn)();
	unsigned char val = (this->*read8)(addr);
	regA &= val;
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
	unsigned short int adr = (this->*read8)((regPC + 1)&0xffff);
	if (flagP) adr |= 0x100;
	return adr;
}

unsigned short int apu::addrIndirectX()
{
	unsigned char pointer = (this->*read8)(regPC + 1);

	unsigned short int adr = 0;
	unsigned short int low = (pointer+regX)&0xff;
	if (flagP) low |= 0x100;

	unsigned short int high = (pointer + regX+1) & 0xff;
	if (flagP) high |= 0x100;

	adr = (this->*read8)(low);
	adr |= (this->*read8)(high) << 8;;

	return adr&0xffff;
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

unsigned short int apu::addrAbsY()
{
	unsigned char lowaddr = (this->*read8)(regPC + 1);
	unsigned char highaddr = (this->*read8)(regPC + 2);
	unsigned short int addr = lowaddr | (highaddr << 8);
	addr = (addr + regY) & 0xffff;
	return addr;
}

unsigned short int apu::addrDPX()
{
	unsigned short int adr = ((this->*read8)(regPC + 1)+regX)&0xff;
	if (flagP) adr |= 0x100;
	return adr;
}

unsigned short int apu::addrDPY()
{
	unsigned short int adr = ((this->*read8)(regPC + 1) + regY) & 0xff;
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
		case 0x75:
		{
			// cmp a,!addr+X
			doCMPA(&apu::addrAbsX);
			regPC += 3;
			cycles = 5;
			break;
		}
		case 0xdb:
		{
			// mov d+x,y
			doMoveWithRead(&apu::addrDPX, regY);
			regPC += 2;
			cycles = 5;
			break;
		}
		case 0xde:
		{
			// CBNE d+x,offs
			unsigned short int addr = apu::addrDPX();
			unsigned char val = (this->*read8)(addr) ^ 0xff;
			unsigned char result = regA + val + 1;
			signed char offs = (this->*read8)(regPC + 2);
			cycles = doBranch(offs, result != 0)+4;
			regPC += 1;
			break;
		}
		case 0x8d:
		{
			// MOV Y,#$imm
			doMoveToY(&apu::addrModePC);
			regPC += 2;
			cycles = 2;
			break;
		}
		case 0x68:
		{
			// CMP A,#imm
			doCMPA(&apu::addrModePC);
			regPC += 2;
			cycles = 2;
			break;
		}
		case 0x5e:
		{
			// cmp y,!d
			doCMPY(&apu::addrAbs);
			regPC += 3;
			cycles = 4;
			break;
		}
		case 0xc9:
		{
			// MOV !a,X
			doMoveWithRead(&apu::addrAbs, regX);
			regPC += 3;
			cycles = 5;
			break;
		}
		case 0x5f:
		{
			// JMP !a
			unsigned short int dst;
			unsigned char lowaddr, highaddr;
			lowaddr = (this->*read8)(regPC + 1);
			highaddr = (this->*read8)(regPC + 2);
			dst = lowaddr | (highaddr << 8);
			regPC = dst;
			cycles = 3;
			break;
		}
		case 0x80:
		{
			// SETC
			flagC = true;
			cycles = 2;
			regPC += 1;
			break;
		}
		case 0x1c:
		{
			// ASL A
			flagC = regA & 0x80;
			regA <<= 1;
			doFlagsNZ(regA);
			cycles = 2;
			regPC += 1;
			break;
		}
		case 0xf6:
		{
			// MOV A,!d+Y
			doMoveToA(&apu::addrAbsY);
			regPC += 3;
			cycles = 5;
			break;
		}
		case 0xd4:
		{
			// MOV d+X,A
			doMoveWithRead(&apu::addrDPX, regA);
			regPC += 2;
			cycles = 5;
			break;
		}
		case 0xd6:
		{
			// MOV !d+Y,A
			doMoveWithRead(&apu::addrAbsY, regA);
			regPC += 3;
			cycles = 6;
			break;
		}
		case 0xfe:
		{
			// DBNZ Y,rel
			signed char offs = (this->*read8)(regPC + 1);
			regY--;
			cycles=doBranch(offs,regY != 0)+2;
			break;
		}
		case 0x48:
		{
			// EOR a,imm
			doEor(&apu::addrModePC);
			regPC += 2;
			cycles = 2;
			break;
		}
		case 0x4b:
		{
			// LSR d
			doLsr(&apu::addrDP);
			regPC += 2;
			cycles = 4;
			break;
		}
		case 0x28:
		{
			// AND $#d
			doAnd(&apu::addrModePC);
			regPC += 2;
			cycles = 2;
			break;
		}
		case 0x02:
		case 0x22:
		case 0x42:
		case 0x62:
		case 0x82:
		case 0xa2:
		case 0xc2:
		case 0xe2:
		{
			// SET1 d.b
			unsigned short int addr = addrDP();
			unsigned char val = (this->*read8)(addr);
			(this->*write8)(addr, val | (1 << (nextOpcode >> 5)));
			regPC += 2;
			cycles = 4;
			break;
		}
		case 0x6e:
		{
			// DBNZ d,r
			unsigned short int addr = addrDP();
			signed char offs = (this->*read8)(regPC + 2);
			unsigned char val = (this->*read8)(addr);
			val--;
			(this->*write8)(addr, val);
			cycles = doBranch(offs, val != 0) + 3;
			regPC += 1;
			break;
		}
		case 0xf7:
		{
			// MOV A, [d]+Y
			doMoveToA(&apu::addrIndirectY);
			regPC += 2;
			cycles = 6;
			break;
		}
		case 0x3a:
		{
			// INCW d
			unsigned char adr = (this->*read8)(regPC + 1);
			unsigned short int low = adr;
			if (flagP) low |= 0x100;
			unsigned short int high = (adr + 1) & 0xff;
			if (flagP) high |= 0x100;
			unsigned char val1 = (this->*read8)(low);
			unsigned short int val = val1 | ((this->*read8)(high) << 8);

			val += 1;

			(this->*write8)(low, val & 0xff);
			(this->*write8)(high, val>>8);

			flagZ = val == 0;
			flagN = val & 0x8000;

			regPC += 2;
			cycles = 6;
			break;
		}
		case 0x2d:
		{
			// PUSH A
			(this->*write8)(0x100 | regSP, regA);
			regSP--;
			regSP &= 0xff;
			regPC += 1;
			cycles = 4;
			break;
		}
		case 0xae:
		{
			// POP A
			regSP += 1; regSP &= 0xff;
			regA = (this->*read8)(0x100 | regSP);
			regPC += 1;
			cycles = 4;
			break;
		}
		case 0xdc:
		{
			// DEC Y
			regY -= 1;
			doFlagsNZ(regY);
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0xbc:
		{
			// INC A
			regA += 1;
			doFlagsNZ(regA);
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0x9c:
		{
			// DEC A
			regA -= 1;
			doFlagsNZ(regA);
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0xac:
		{
			// INC !a
			unsigned short int addr = addrAbs();
			unsigned char val = (this->*read8)(addr);
			val += 1;
			(this->*write8)(addr, val);
			doFlagsNZ(val);
			regPC += 3;
			cycles = 5;
			break;
		}
		case 0x13:
		case 0x33:
		case 0x53:
		case 0x73:
		case 0x93:
		case 0xb3:
		case 0xd3:
		case 0xf3: 
		{ 
			// bbc dp, rel
			unsigned short int addr = addrDP();
			unsigned char val = (this->*read8)(addr);
			signed char offs = (this->*read8)(regPC + 2);
			cycles = doBranch(offs, (val & (1 << (nextOpcode >> 5))) == 0)+3;
			regPC += 1;
			break;
		}
		case 0xad:
		{
			// CMP Y,imm
			doCMPY(&apu::addrModePC);
			regPC += 2;
			cycles = 2;
			break;
		}
		case 0x7a:
		{
			// ADDW YA,d
			unsigned char adr = (this->*read8)(regPC + 1);
			unsigned short int low = adr;
			if (flagP) low |= 0x100;
			unsigned short int high = (adr + 1) & 0xff;
			if (flagP) high |= 0x100;
			unsigned char val1 = (this->*read8)(low);

			unsigned short int value = val1 | ((this->*read8)(high) << 8);
			unsigned short int ya = regA | (regY << 8);
			
			int result = ya + value;
			flagV = (ya & 0x8000) == (value & 0x8000) && (value & 0x8000) != (result & 0x8000);
			flagH = ((ya & 0xfff) + (value & 0xfff)) > 0xfff;
			flagC = result > 0xffff;
			flagZ = (result & 0xffff) == 0;
			flagN = result & 0x8000;
			regA = result & 0xff;
			regY = (unsigned char)(result >> 8);
			
			regPC += 2;
			cycles = 5;
			break;
		}
		case 0xe7:
		{
			// mov a,[d+X]
			doMoveToA(&apu::addrIndirectX);
			doFlagsNZ(regA);
			regPC += 2;
			cycles = 6;
			break;
		}
		case 0xb0:
		{
			// BCS offs
			signed char offs = (this->*read8)(regPC + 1);
			cycles = doBranch(offs, flagC);
			break;
		}
		case 0x09:
		{
			// or dd,ds
			unsigned short int adr0 = (this->*read8)(regPC + 1);
			if (flagP) adr0 |= 0x100;
			unsigned short int adr1 = (this->*read8)(regPC + 2);
			if (flagP) adr1 |= 0x100;

			unsigned char val0 = (this->*read8)(adr0);
			unsigned char val1 = (this->*read8)(adr1);

			unsigned char result = val0 | val1;
			(this->*write8)(adr1,result);
			doFlagsNZ(result);

			regPC += 3;
			cycles = 6;
			break;
		}
		case 0xa8:
		{
			// sbc a,#i
			unsigned short int addr = addrModePC();

			unsigned char value = (this->*read8)(addr) ^ 0xff;
			int result = regA + value + (flagC?1:0);
			flagV = (regA & 0x80) == (value & 0x80) && (value & 0x80) != (result & 0x80);
			flagH = ((regA & 0xf) + (value & 0xf) + (flagC?1:0)) > 0xf;
			flagC = result > 0xff;
			regA = (unsigned char)result;
			doFlagsNZ(regA);

			regPC += 2;
			cycles = 2;
			break;
		}
		case 0x4d:
		{
			// PUSH X
			(this->*write8)(0x100 | regSP, regX);
			regSP--;
			regSP &= 0xff;
			regPC += 1;
			cycles = 4;
			break;
		}
		case 0x9e:
		{
			// DIV YA,X

			flagH = (regX & 0xf) <= (regY & 0xf);
			int yva = (regY << 8) | regA;
			int x = regX << 9;
			for (int i = 0; i < 9; i++) 
			{
				yva <<= 1;
				yva |= (yva & 0x20000) ? 1 : 0;
				yva &= 0x1ffff;
				if (yva >= x) yva ^= 1;
				if (yva & 1) yva -= x;
				yva &= 0x1ffff;
			}
			regY = (unsigned char)(yva >> 9);
			flagV = yva & 0x100;
			regA = yva & 0xff;
			doFlagsNZ(regA);

			regPC += 1;
			cycles = 12;
			break;
		}
		case 0x7c:
		{
			// ROR A
			bool newC = regA & 1;
			regA = (regA >> 1) | ((flagC?1:0) << 7);
			flagC = newC;
			doFlagsNZ(regA);
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0xce:
		{
			// POP X 
			regSP += 1; regSP &= 0xff;
			regX = (this->*read8)(0x100 | regSP);
			regPC += 1;
			cycles = 4;
			break;
		}
		case 0x9a:
		{
			// SUBW YA,d
			unsigned char adr = (this->*read8)(regPC + 1);
			unsigned short int low = adr;
			if (flagP) low |= 0x100;
			unsigned short int high = (adr + 1) & 0xff;
			if (flagP) high |= 0x100;
			unsigned char vall = (this->*read8)(low);

			unsigned short int value = (vall | ((this->*read8)(high) << 8)) ^ 0xffff;
			unsigned short int ya = regA | (regY << 8);
			int result = ya + value + 1;

			flagV = (ya & 0x8000) == (value & 0x8000) && (value & 0x8000) != (result & 0x8000);
			flagH = ((ya & 0xfff) + (value & 0xfff) + 1) > 0xfff;
			flagC = result > 0xffff;
			flagZ = (result & 0xffff) == 0;
			flagN = result & 0x8000;
			regA = result & 0xff;
			regY = (unsigned char)(result >> 8);

			regPC += 2;
			cycles = 5;
			break;
		}
		case 0x9f:
		{
			// XCN A
			regA = (regA >> 4) | (regA << 4);
			doFlagsNZ(regA);
			regPC += 1;
			cycles = 5;
			break;
		}
		case 0x5c:
		{
			// LSR A
			flagC = regA & 1;
			regA >>= 1;
			doFlagsNZ(regA);
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0x08:
		{
			// OR A,#imm
			unsigned short int addr = addrModePC();
			regA |= (this->*read8)(addr);
			doFlagsNZ(regA);
			regPC += 2;
			cycles = 2;
			break;
		}
		case 0x24:
		{
			// AND A,d
			doAnd(&apu::addrDP);
			regPC += 2;
			cycles = 3;
			break;
		}
		case 0x12:
		case 0x32:
		case 0x52:
		case 0x72:
		case 0x92:
		case 0xb2:
		case 0xd2:
		case 0xf2:
		{
			// CLR1 d.n
			unsigned short int addr = addrDP();
			unsigned char val=(this->*read8)(addr);
			val = val & ~(1 << (nextOpcode >> 5));
			(this->*write8)(addr, val);
			regPC += 2;
			cycles = 4;
			break;
		}
		case 0x65:
		{
			// CMP A,!$d
			doCMPA(&apu::addrAbs);
			regPC += 3;
			cycles = 4;
			break;
		}
		case 0x8c:
		{
			// DEC !d
			unsigned short int addr = addrAbs();
			unsigned char val = (this->*read8)(addr);
			val--;
			(this->*write8)(addr, val);
			doFlagsNZ(val);
			regPC += 3;
			cycles = 5;
			break;
		}
		case 0xc0:
		{
			// DI
			flagI = false;
			regPC += 1;
			cycles = 3;
			break;
		}
		case 0x8e:
		{
			// POP PSW
			regSP += 1; regSP &= 0xff;
			unsigned char poppedFlags = (this->*read8)(0x100 | regSP);

			flagN = poppedFlags & 0x80;
			flagV = poppedFlags & 0x40;
			flagP = poppedFlags & 0x20;
			flagB = poppedFlags & 0x10;
			flagH = poppedFlags & 8;
			flagI = poppedFlags & 4;
			flagZ = poppedFlags & 2;
			flagC = poppedFlags & 1;

			regPC += 1;
			cycles = 4;
			break;
		}
		case 0x04:
		{
			// OR A,dp
			unsigned short int addr = addrDP();
			unsigned char val = (this->*read8)(addr);
			regA |= val;
			doFlagsNZ(regA);
			cycles = 3;
			regPC += 2;
			break;
		}
		case 0x64:
		{
			// CMP A,d
			doCMPA(&apu::addrDP);
			regPC += 2;
			cycles = 3;
			break;
		}
		case 0xf8:
		{
			// MOV X,d
			doMoveToX(&apu::addrDP);
			regPC += 2;
			cycles = 3;
			break;
		}
		case 0x3e:
		{
			// CMP X,dp
			doCMPX(&apu::addrDP);
			regPC += 2;
			cycles = 3;
			break;
		}
		case 0x29:
		{
			// AND dp,dp

			unsigned short int adr0 = (this->*read8)(regPC + 1);
			if (flagP) adr0 |= 0x100;
			unsigned short int adr1 = (this->*read8)(regPC + 2);
			if (flagP) adr1 |= 0x100;

			unsigned char val0 = (this->*read8)(adr0);
			unsigned char val1 = (this->*read8)(adr1);

			unsigned char result = val0 & val1;
			(this->*write8)(adr1, result);
			doFlagsNZ(result);

			regPC += 3;
			cycles = 6;
			break;
		}
		case 0xfa:
		{
			// mov dd,ds
			unsigned short int adr0 = (this->*read8)(regPC + 1);
			if (flagP) adr0 |= 0x100;
			unsigned short int adr1 = (this->*read8)(regPC + 2);
			if (flagP) adr1 |= 0x100;

			unsigned char val1 = (this->*read8)(adr0);
			(this->*write8)(adr1,val1);

			regPC += 3;
			cycles = 5;
			break;
		}
		case 0xd8:
		{
			// mov d,x
			unsigned short int adr0 = (this->*read8)(regPC + 1);
			if (flagP) adr0 |= 0x100;

			(this->*write8)(adr0, regX);

			regPC += 2;
			cycles = 4;
			break;
		}
		case 0x88:
		{
			// ADC A,imm
			unsigned short int addr = addrModePC();
			unsigned char value = (this->*read8)(addr);
			int result = regA + value + (flagC?1:0);
			flagV = (regA & 0x80) == (value & 0x80) && (value & 0x80) != (result & 0x80);
			flagH = ((regA & 0xf) + (value & 0xf) + (flagC ? 1 : 0)) > 0xf;
			flagC = result > 0xff;
			regA = (unsigned char)result;
			doFlagsNZ(regA);
			regPC += 2;
			cycles = 2;
			break;
		}
		case 0x9b:
		{
			// dec d+X
			unsigned short int addr = addrDPX();
			unsigned char value = (this->*read8)(addr);
			value--;
			(this->*write8)(addr, value);
			doFlagsNZ(value);
			regPC += 2;
			cycles = 5;
			break;
		}
		case 0xbb:
		{
			// inc d+X
			unsigned short int addr = addrDPX();
			unsigned char value = (this->*read8)(addr);
			value++;
			(this->*write8)(addr, value);
			doFlagsNZ(value);
			regPC += 2;
			cycles = 5;
			break;
		}
		case 0x85:
		{
			// ADC a,!d
			unsigned short int addr = addrAbs();
			unsigned char value = (this->*read8)(addr);
			int result = regA + value + (flagC ? 1 : 0);
			flagV = (regA & 0x80) == (value & 0x80) && (value & 0x80) != (result & 0x80);
			flagH = ((regA & 0xf) + (value & 0xf) + (flagC ? 1 : 0)) > 0xf;
			flagC = result > 0xff;
			regA = (unsigned char)result;
			doFlagsNZ(regA);
			regPC += 3;
			cycles = 4;
			break;
		}
		case 0x0b:
		{
			// ASL d
			unsigned short int addr = addrDP();
			unsigned char value = (this->*read8)(addr);
			flagC = value & 0x80;
			value <<= 1;
			doFlagsNZ(value);
			(this->*write8)(addr, value);
			regPC += 2;
			cycles = 4;
			break;
		}
		case 0xb6:
		{
			// SBC a,!a+y
			unsigned short int addr = addrAbsY();

			unsigned char value = (this->*read8)(addr) ^ 0xff;
			int result = regA + value + (flagC ? 1 : 0);
			flagV = (regA & 0x80) == (value & 0x80) && (value & 0x80) != (result & 0x80);
			flagH = ((regA & 0xf) + (value & 0xf) + (flagC ? 1 : 0)) > 0xf;
			flagC = result > 0xff;
			regA = (unsigned char)result;
			doFlagsNZ(regA);

			regPC += 3;
			cycles = 5;
			break;
		}
		case 0x96:
		{
			// ADC a,!a+y
			doAdc(&apu::addrAbsY);
			regPC += 3;
			cycles = 5;
			break;
		}
		case 0x03:
		case 0x23:
		case 0x43:
		case 0x63:
		case 0x83:
		case 0xa3:
		case 0xc3:
		case 0xe3:
		{
			// BBS d.n,r
			unsigned short int addr = addrDP();
			unsigned char val = (this->*read8)(addr);
			signed char offs = (this->*read8)(regPC + 2);
			cycles = doBranch(offs, (val & (1 << (nextOpcode >> 5))) != 0) + 3;
			regPC += 1;
			break;
		}
		case 0x40:
		{
			// setp
			flagP = true;
			cycles = 2;
			regPC += 1;
			break;
		}
		case 0x74:
		{
			// cmp a,d+x
			doCMPA(&apu::addrDPX);
			regPC += 2;
			cycles = 4;
			break;
		}
		case 0xfb:
		{
			// mov y,d+x
			doMoveToY(&apu::addrDPX);
			regPC += 2;
			cycles = 4;
			break;
		}
		case 0x95:
		{
			// ADC a,!a+x
			doAdc(&apu::addrAbsX);
			regPC += 3;
			cycles = 5;
			break;
		}
		case 0xb5:
		{
			// SBC a,!a+x
			unsigned short int addr = addrAbsX();

			unsigned char value = (this->*read8)(addr) ^ 0xff;
			int result = regA + value + (flagC ? 1 : 0);
			flagV = (regA & 0x80) == (value & 0x80) && (value & 0x80) != (result & 0x80);
			flagH = ((regA & 0xf) + (value & 0xf) + (flagC ? 1 : 0)) > 0xf;
			flagC = result > 0xff;
			regA = (unsigned char)result;
			doFlagsNZ(regA);

			regPC += 3;
			cycles = 5;
			break;
		}
		case 0xc7:
		{
			// mov [d+X],A
			doMoveWithRead(&apu::addrIndirectX, regA);
			regPC += 2;
			cycles = 7;
			break;
		}
		case 0x98:
		{
			// ADC d,#i
			unsigned char value = (this->*read8)(regPC + 1);
			unsigned short int adrDst = (this->*read8)(regPC + 2);
			if (flagP) adrDst |= 0x100;

			int fc = flagC ? 1 : 0;
			unsigned char applyOn = (this->*read8)(adrDst);
			int result = applyOn + value + fc;
			flagV = (applyOn & 0x80) == (value & 0x80) && (value & 0x80) != (result & 0x80);
			flagH = ((applyOn & 0xf) + (value & 0xf) + fc) > 0xf;
			flagC = result > 0xff;

			(this->*write8)(adrDst, (unsigned char)result);
			doFlagsNZ((unsigned char)result);

			regPC += 3;
			cycles = 5;
			break;
		}
		case 0x2e:
		{
			// CBNE d,offs
			unsigned short int addr = apu::addrDP();
			unsigned char val = (this->*read8)(addr) ^ 0xff;
			unsigned char result = regA + val + 1;
			signed char offs = (this->*read8)(regPC + 2);
			cycles = doBranch(offs, result != 0) + 3;
			regPC += 1;
			break;
		}
		case 0x97:
		{
			// ADC a,[dp]+x
			doAdc(&apu::addrIndirectY);
			regPC += 2;
			cycles = 6;
			break;
		}
		case 0xa4:
		{
			// sbc a,dp
			unsigned short int addr = addrDP();

			unsigned char value = (this->*read8)(addr) ^ 0xff;
			int result = regA + value + (flagC ? 1 : 0);
			flagV = (regA & 0x80) == (value & 0x80) && (value & 0x80) != (result & 0x80);
			flagH = ((regA & 0xf) + (value & 0xf) + (flagC ? 1 : 0)) > 0xf;
			flagC = result > 0xff;
			regA = (unsigned char)result;
			doFlagsNZ(regA);

			regPC += 2;
			cycles = 3;
			break;
		}
		default:
		{
			// unknown opcode
			//std::stringstream strr;
			//strr << std::hex << std::setw(2) << std::setfill('0') << (int)nextOpcode;
			//std::stringstream staddr;
			//staddr << std::hex << std::setw(4) << std::setfill('0') << regPC;
			//glbTheLogger.logMsg("Unknown SPC700 opcode [" + strr.str() + "] at 0x" + staddr.str());
			return -1;
		}
	}

	return cycles;
}

apu::~apu()
{
	delete(spc700ram);
}
