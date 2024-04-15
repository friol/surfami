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
		glbTheLogger.logMsg("Unable to open roms/ipl.rom (SPC700 rom). Bailing out");
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

	counter = 0;
	dspDIR = 0;
	evenCycle = true;

	dspReset=false;
	mute=false;
	echoWrites=false;
	noiseRate=0;
	noiseSample = 0x4000;
	
	// init DSP channels
	for (int i = 0;i < 8;i++)
	{
		channels[i].keyOn = false;
		channels[i].keyOff= false;
		channels[i].leftVol = 0;
		channels[i].rightVol = 0;
		channels[i].samplePitch = 0;
		channels[i].sampleSourceEntry = 0;
		channels[i].startDelay = 0;
		channels[i].adsrState = 0;
		channels[i].brrHeader = 0;
		channels[i].blockOffset = 0;
		channels[i].decodeOffset = 0;
		channels[i].bufferOffset = 0;
		channels[i].gain = 0;
		channels[i].useNoise = false;

		memset(channels[i].adsrRates, 0, sizeof(channels[i].adsrRates));
		channels[i].sustainLevel = 0;
		channels[i].gainSustainLevel = 0;
		channels[i].useGain = false;
		channels[i].gainMode = 0;
		channels[i].directGain = false;
		channels[i].gainValue = 0;
		channels[i].preclampGain = 0;

		memset(channels[i].decodeBuffer, 0, sizeof(channels[i].decodeBuffer));
	}

	memset(dspRam, 0, 0x80);
	dspRam[0x7c] = 0xff;
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
	//std::stringstream strstrVal;
	//strstrVal << std::hex << std::setw(2) << std::setfill('0') << (int)val;

	//std::stringstream sDspReg;
	//sDspReg << std::hex << std::setw(2) << std::setfill('0') << (int)dspSelectedRegister;

	if (dspSelectedRegister == 0x0c)
	{
		//glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to MVOL (L)");
		mainVolLeft = (signed char)val;
	}
	else if (dspSelectedRegister == 0x1c)
	{
		//glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to MVOL (R)");
		mainVolRight = (signed char)val;
	}
	else if (dspSelectedRegister == 0x2c)
	{
		//glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to EVOL (L)");
		echoVolLeft = (signed char)val;
	}
	else if (dspSelectedRegister == 0x3c)
	{
		//glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to EVOL (R)");
		echoVolRight = (signed char)val;
	}
	else if (dspSelectedRegister == 0x4c)
	{
		//glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to KEYON");
		for (int i = 0; i < 8; i++)
		{
			if (val & (1 << i))
			{
				channels[i].keyOn = val & (1 << i);
			}
		}
	}
	else if (dspSelectedRegister == 0x5c)
	{
		//glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to KEYOFF");
		for (int i = 0; i < 8; i++) 
		{
			channels[i].keyOff = val & (1 << i);
		}
	}
	else if (dspSelectedRegister == 0x4d)
	{
		// For each channel, sends to the echo unit.
		for (int i = 0; i < 8; i++) 
		{
			channels[i].echoEnable = val & (1 << i);
		}
	}
	else if (dspSelectedRegister == 0x5d)
	{
		//glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to DIR 5d sample source directory page");
		dspDIR = (int)val<<8;
	}
	else if (dspSelectedRegister == 0x6d)
	{
		//glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to ESA 6d start echo mem reg");
		echoBufferAdr = val << 8;
	}
	else if (dspSelectedRegister == 0x7d)
	{
		//glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to 7d echo delay time");
		echoDelay = (val & 0xf) * 512; // 2048-byte steps, stereo sample is 4 bytes
	}
	else if ((dspSelectedRegister & 0x0f) == 0)
	{
		// left channel volume for voice
		int voiceNum = (dspSelectedRegister & 0xf0) >> 4;
		//glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] VOL (L) voice "+std::to_string(voiceNum));
		channels[voiceNum].leftVol = val;
	}
	else if ((dspSelectedRegister & 0x0f) == 1)
	{
		// right channel volume for voice
		int voiceNum = (dspSelectedRegister & 0xf0) >> 4;
		//glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] VOL (R) voice " + std::to_string(voiceNum));
		channels[voiceNum].rightVol = val;
	}
	else if ((dspSelectedRegister & 0x0f) == 2)
	{
		// low 8 bits of sample pitch
		int voiceNum = (dspSelectedRegister & 0xf0) >> 4;
		//glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] PITCH (L) voice " + std::to_string(voiceNum));
		//channels[voiceNum].samplePitch |= val;
		channels[voiceNum].samplePitch= (channels[voiceNum].samplePitch & 0x3f00) | val;;
	}
	else if ((dspSelectedRegister & 0x0f) == 3)
	{
		// high 6 bits of sample pitch
		int voiceNum = (dspSelectedRegister & 0xf0) >> 4;
		//glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] PITCH (H) voice " + std::to_string(voiceNum));
		//channels[voiceNum].samplePitch |= (val<<8);
		channels[voiceNum].samplePitch= ((channels[voiceNum].samplePitch & 0x00ff) | (val << 8)) & 0x3fff;
	}
	else if ((dspSelectedRegister & 0x0f) == 4)
	{
		// sample source entry from DIR
		int voiceNum = (dspSelectedRegister & 0xf0) >> 4;
		//glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] SCRN voice " + std::to_string(voiceNum));
		channels[voiceNum].sampleSourceEntry = val;
		//calcBRRSampleStart(voiceNum);
	}
	else if ((dspSelectedRegister & 0x0f) == 0x0f)
	{
		int voiceNum = (dspSelectedRegister & 0xf0) >> 4;
		firValues[voiceNum] = val;
	}
	else if (dspSelectedRegister==0x6c) 
	{
		dspReset = val & 0x80;
		mute = val & 0x40;
		echoWrites = (val & 0x20) == 0;
		noiseRate = val & 0x1f;
	}
	else if (dspSelectedRegister == 0x7c)
	{
		// any write clears ENDx
		val = 0;
	}
	else if ((dspSelectedRegister & 0x0f) == 5)
	{
		int voiceNum = (dspSelectedRegister & 0xf0) >> 4;
		channels[voiceNum].adsrRates[0] = (val & 0xf) * 2 + 1;
		channels[voiceNum].adsrRates[1] = ((val & 0x70) >> 4) * 2 + 16;
		channels[voiceNum].useGain = (val & 0x80) == 0;
	}
	else if ((dspSelectedRegister & 0x0f) == 6)
	{
		int voiceNum = (dspSelectedRegister & 0xf0) >> 4;
		channels[voiceNum].adsrRates[2] = val & 0x1f;
		channels[voiceNum].sustainLevel = (val & 0xe0) >> 5;
	}
	else if ((dspSelectedRegister & 0x0f) == 7)
	{
		int voiceNum = (dspSelectedRegister & 0xf0) >> 4;
		channels[voiceNum].directGain = (val & 0x80) == 0;
		channels[voiceNum].gainMode = (val & 0x60) >> 5;
		channels[voiceNum].adsrRates[3] = val & 0x1f;
		channels[voiceNum].gainValue = (val & 0x7f) * 16;
		channels[voiceNum].gainSustainLevel = (val & 0xe0) >> 5;
	}
	else if (dspSelectedRegister==0x3d)
	{
		// For each channel, replaces the sample waveform with the noise generator output.
		for (int i = 0; i < 8; i++) 
		{
			channels[i].useNoise = val & (1 << i);
		}
	}
	else
	{
		//glbTheLogger.logMsg("apu::dsp::write [" + strstrVal.str() + "] to unmapped register "+ sDspReg.str());
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
	if ((address >= 0xf4) && (address <= 0xf7))
	{
		return portsFromCPU[address - 0xf4];
	}
	else if ((address == 0xf8) || (address == 0xf9))
	{
		return spc700ram[address];
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
		std::stringstream strstrAddr;
		strstrAddr << std::hex << std::setw(4) << std::setfill('0') << (int)address;
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
	else if ((address == 0xf8) || (address == 0xf9))
	{
		spc700ram[address] = val;
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

/* the code for DSP sound generation comes from https://github.com/angelo-wf/LakeSnes */

static const int gaussValues[512] = {
  0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,
  0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x002, 0x002, 0x002, 0x002, 0x002,
  0x002, 0x002, 0x003, 0x003, 0x003, 0x003, 0x003, 0x004, 0x004, 0x004, 0x004, 0x004, 0x005, 0x005, 0x005, 0x005,
  0x006, 0x006, 0x006, 0x006, 0x007, 0x007, 0x007, 0x008, 0x008, 0x008, 0x009, 0x009, 0x009, 0x00a, 0x00a, 0x00a,
  0x00b, 0x00b, 0x00b, 0x00c, 0x00c, 0x00d, 0x00d, 0x00e, 0x00e, 0x00f, 0x00f, 0x00f, 0x010, 0x010, 0x011, 0x011,
  0x012, 0x013, 0x013, 0x014, 0x014, 0x015, 0x015, 0x016, 0x017, 0x017, 0x018, 0x018, 0x019, 0x01a, 0x01b, 0x01b,
  0x01c, 0x01d, 0x01d, 0x01e, 0x01f, 0x020, 0x020, 0x021, 0x022, 0x023, 0x024, 0x024, 0x025, 0x026, 0x027, 0x028,
  0x029, 0x02a, 0x02b, 0x02c, 0x02d, 0x02e, 0x02f, 0x030, 0x031, 0x032, 0x033, 0x034, 0x035, 0x036, 0x037, 0x038,
  0x03a, 0x03b, 0x03c, 0x03d, 0x03e, 0x040, 0x041, 0x042, 0x043, 0x045, 0x046, 0x047, 0x049, 0x04a, 0x04c, 0x04d,
  0x04e, 0x050, 0x051, 0x053, 0x054, 0x056, 0x057, 0x059, 0x05a, 0x05c, 0x05e, 0x05f, 0x061, 0x063, 0x064, 0x066,
  0x068, 0x06a, 0x06b, 0x06d, 0x06f, 0x071, 0x073, 0x075, 0x076, 0x078, 0x07a, 0x07c, 0x07e, 0x080, 0x082, 0x084,
  0x086, 0x089, 0x08b, 0x08d, 0x08f, 0x091, 0x093, 0x096, 0x098, 0x09a, 0x09c, 0x09f, 0x0a1, 0x0a3, 0x0a6, 0x0a8,
  0x0ab, 0x0ad, 0x0af, 0x0b2, 0x0b4, 0x0b7, 0x0ba, 0x0bc, 0x0bf, 0x0c1, 0x0c4, 0x0c7, 0x0c9, 0x0cc, 0x0cf, 0x0d2,
  0x0d4, 0x0d7, 0x0da, 0x0dd, 0x0e0, 0x0e3, 0x0e6, 0x0e9, 0x0ec, 0x0ef, 0x0f2, 0x0f5, 0x0f8, 0x0fb, 0x0fe, 0x101,
  0x104, 0x107, 0x10b, 0x10e, 0x111, 0x114, 0x118, 0x11b, 0x11e, 0x122, 0x125, 0x129, 0x12c, 0x130, 0x133, 0x137,
  0x13a, 0x13e, 0x141, 0x145, 0x148, 0x14c, 0x150, 0x153, 0x157, 0x15b, 0x15f, 0x162, 0x166, 0x16a, 0x16e, 0x172,
  0x176, 0x17a, 0x17d, 0x181, 0x185, 0x189, 0x18d, 0x191, 0x195, 0x19a, 0x19e, 0x1a2, 0x1a6, 0x1aa, 0x1ae, 0x1b2,
  0x1b7, 0x1bb, 0x1bf, 0x1c3, 0x1c8, 0x1cc, 0x1d0, 0x1d5, 0x1d9, 0x1dd, 0x1e2, 0x1e6, 0x1eb, 0x1ef, 0x1f3, 0x1f8,
  0x1fc, 0x201, 0x205, 0x20a, 0x20f, 0x213, 0x218, 0x21c, 0x221, 0x226, 0x22a, 0x22f, 0x233, 0x238, 0x23d, 0x241,
  0x246, 0x24b, 0x250, 0x254, 0x259, 0x25e, 0x263, 0x267, 0x26c, 0x271, 0x276, 0x27b, 0x280, 0x284, 0x289, 0x28e,
  0x293, 0x298, 0x29d, 0x2a2, 0x2a6, 0x2ab, 0x2b0, 0x2b5, 0x2ba, 0x2bf, 0x2c4, 0x2c9, 0x2ce, 0x2d3, 0x2d8, 0x2dc,
  0x2e1, 0x2e6, 0x2eb, 0x2f0, 0x2f5, 0x2fa, 0x2ff, 0x304, 0x309, 0x30e, 0x313, 0x318, 0x31d, 0x322, 0x326, 0x32b,
  0x330, 0x335, 0x33a, 0x33f, 0x344, 0x349, 0x34e, 0x353, 0x357, 0x35c, 0x361, 0x366, 0x36b, 0x370, 0x374, 0x379,
  0x37e, 0x383, 0x388, 0x38c, 0x391, 0x396, 0x39b, 0x39f, 0x3a4, 0x3a9, 0x3ad, 0x3b2, 0x3b7, 0x3bb, 0x3c0, 0x3c5,
  0x3c9, 0x3ce, 0x3d2, 0x3d7, 0x3dc, 0x3e0, 0x3e5, 0x3e9, 0x3ed, 0x3f2, 0x3f6, 0x3fb, 0x3ff, 0x403, 0x408, 0x40c,
  0x410, 0x415, 0x419, 0x41d, 0x421, 0x425, 0x42a, 0x42e, 0x432, 0x436, 0x43a, 0x43e, 0x442, 0x446, 0x44a, 0x44e,
  0x452, 0x455, 0x459, 0x45d, 0x461, 0x465, 0x468, 0x46c, 0x470, 0x473, 0x477, 0x47a, 0x47e, 0x481, 0x485, 0x488,
  0x48c, 0x48f, 0x492, 0x496, 0x499, 0x49c, 0x49f, 0x4a2, 0x4a6, 0x4a9, 0x4ac, 0x4af, 0x4b2, 0x4b5, 0x4b7, 0x4ba,
  0x4bd, 0x4c0, 0x4c3, 0x4c5, 0x4c8, 0x4cb, 0x4cd, 0x4d0, 0x4d2, 0x4d5, 0x4d7, 0x4d9, 0x4dc, 0x4de, 0x4e0, 0x4e3,
  0x4e5, 0x4e7, 0x4e9, 0x4eb, 0x4ed, 0x4ef, 0x4f1, 0x4f3, 0x4f5, 0x4f6, 0x4f8, 0x4fa, 0x4fb, 0x4fd, 0x4ff, 0x500,
  0x502, 0x503, 0x504, 0x506, 0x507, 0x508, 0x50a, 0x50b, 0x50c, 0x50d, 0x50e, 0x50f, 0x510, 0x511, 0x511, 0x512,
  0x513, 0x514, 0x514, 0x515, 0x516, 0x516, 0x517, 0x517, 0x517, 0x518, 0x518, 0x518, 0x518, 0x518, 0x519, 0x519
};

static int clamp16(int val) 
{
	return val < -0x8000 ? -0x8000 : (val > 0x7fff ? 0x7fff : val);
}

static int clip16(int val) 
{
	return (signed short int)(val & 0xffff);
}

signed short int apu::dspGetSample(int ch)
{
	int pos = (channels[ch].pitchCounter >> 12) + channels[ch].bufferOffset;
	int offset = (channels[ch].pitchCounter >> 4) & 0xff;
	signed short int news = channels[ch].decodeBuffer[(pos + 3) % 12];
	signed short int olds = channels[ch].decodeBuffer[(pos + 2) % 12];
	signed short int olders = channels[ch].decodeBuffer[(pos + 1) % 12];
	signed short int oldests = channels[ch].decodeBuffer[pos % 12];
	int out = (gaussValues[0xff - offset] * oldests) >> 11;
	out += (gaussValues[0x1ff - offset] * olders) >> 11;
	out += (gaussValues[0x100 + offset] * olds) >> 11;
	out = clip16(out) + ((gaussValues[offset] * news) >> 11);
	return clamp16(out) & ~1;
}

void apu::dspDecodeBrr(int ch)
{
	int shift = channels[ch].brrHeader >> 4;
	int filter = (channels[ch].brrHeader & 0xc) >> 2;
	int bOff = channels[ch].bufferOffset;
	int old = channels[ch].decodeBuffer[bOff == 0 ? 11 : bOff - 1] >> 1;
	int older = channels[ch].decodeBuffer[bOff == 0 ? 10 : bOff - 2] >> 1;
	unsigned char curByte = 0;
	for (int i = 0; i < 4; i++) {
		int s = 0;
		if (i & 1) 
		{
			s = curByte & 0xf;
		}
		else 
		{
			curByte = spc700ram[(channels[ch].decodeOffset + channels[ch].blockOffset + (i >> 1)) & 0xffff];
			s = curByte >> 4;
		}
		if (s > 7) s -= 16;
		if (shift <= 0xc) 
		{
			s = (s << shift) >> 1;
		}
		else 
		{
			s = (s >> 3) << 12;
		}
		switch (filter) 
		{
			case 1: s += old + (-old >> 4); break;
			case 2: s += 2 * old + ((3 * -old) >> 5) - older + (older >> 4); break;
			case 3: s += 2 * old + ((13 * -old) >> 6) - older + ((3 * older) >> 4); break;
		}

		channels[ch].decodeBuffer[bOff + i] = (signed short int)(clamp16(s) * 2);
		older = old;
		old = channels[ch].decodeBuffer[bOff + i] >> 1;
	}

	channels[ch].bufferOffset += 4;
	if (channels[ch].bufferOffset >= 12) channels[ch].bufferOffset = 0;
}

void apu::dspHandleNoise()
{
	if (dspCheckCounter(noiseRate))
	{
		int bit = (noiseSample & 1) ^ ((noiseSample >> 1) & 1);
		noiseSample = ((noiseSample >> 1) & 0x3fff) | (bit << 14);
	}
}

void apu::dspHandleGain(int ch)
{
	int newGain = channels[ch].gain;
	int rate = 0;

	// handle gain mode
	if (channels[ch].adsrState == 3) 
	{ 
		// release
		rate = 31;
		newGain -= 8;
	}
	else 
	{
		if (!channels[ch].useGain) 
		{
			rate = channels[ch].adsrRates[channels[ch].adsrState];

			switch (channels[ch].adsrState) 
			{
				case 0: newGain += rate == 31 ? 1024 : 32; break; // attack
				case 1: newGain -= ((newGain - 1) >> 8) + 1; break; // decay
				case 2: newGain -= ((newGain - 1) >> 8) + 1; break; // sustain
			}
		}
		else 
		{
			if (!channels[ch].directGain) 
			{
				rate = channels[ch].adsrRates[3];
				switch (channels[ch].gainMode) 
				{
					case 0: newGain -= 32; break; // linear decrease
					case 1: newGain -= ((newGain - 1) >> 8) + 1; break; // exponential decrease
					case 2: newGain += 32; break; // linear increase
					case 3: newGain += (channels[ch].preclampGain < 0x600) ? 32 : 8; break; // bent increase
				}
			}
			else 
			{ 
				// direct gain
				rate = 31;
				newGain = channels[ch].gainValue;
			}
		}
	}
	
	// use sustain level according to mode
	int sustainLevel = channels[ch].useGain ? channels[ch].gainSustainLevel : channels[ch].sustainLevel;
	if (channels[ch].adsrState == 1 && (newGain >> 8) == sustainLevel) 
	{
		channels[ch].adsrState = 2; // go to sustain
	}
	
	// store pre-clamped gain (for bent increase)
	channels[ch].preclampGain = newGain & 0xffff;
	// clamp gain
	if (newGain < 0 || newGain > 0x7ff) {
		newGain = newGain < 0 ? 0 : 0x7ff;
		if (channels[ch].adsrState == 0) 
		{
			channels[ch].adsrState = 1; // go to decay
		}
	}
	
	// store new value
	if (dspCheckCounter(rate)) channels[ch].gain = (unsigned short int)newGain;
}

void apu::dspHandleEcho(signed short int& sampleOutL,signed short int& sampleOutR)
{
	// increment fir buffer index
	firBufferIndex++;
	firBufferIndex &= 0x7;

	// get value out of ram
	unsigned short int adr = echoBufferAdr + echoBufferIndex;
	signed short int ramSample = spc700ram[adr] | (spc700ram[(adr + 1) & 0xffff] << 8);

	firBufferL[firBufferIndex] = ramSample >> 1;
	ramSample = spc700ram[(adr + 2) & 0xffff] | (spc700ram[(adr + 3) & 0xffff] << 8);
	firBufferR[firBufferIndex] = ramSample >> 1;

	// calculate FIR-sum
	int sumL = 0, sumR = 0;
	for (int i = 0; i < 8; i++) 
	{
		sumL += (firBufferL[(firBufferIndex + i + 1) & 0x7] * firValues[i]) >> 6;
		sumR += (firBufferR[(firBufferIndex + i + 1) & 0x7] * firValues[i]) >> 6;
		if (i == 6) 
		{
			// clip to 16-bit before last addition
			sumL = clip16(sumL);
			sumR = clip16(sumR);
		}
	}
	sumL = clamp16(sumL) & ~1;
	sumR = clamp16(sumR) & ~1;

	// apply master volume and modify output with sum
	sampleOutL = (signed short int)clamp16(((sampleOutL * mainVolLeft) >> 7) + ((sumL * echoVolLeft) >> 7));
	sampleOutR = (signed short int)clamp16(((sampleOutR * mainVolRight) >> 7) + ((sumR * echoVolRight) >> 7));

	// get echo value
	int echoL = clamp16(echoOutL + clip16((sumL * feedbackVolume) >> 7)) & ~1;
	int echoR = clamp16(echoOutR + clip16((sumR * feedbackVolume) >> 7)) & ~1;

	// write it to ram
	if (echoWrites) 
	{
		spc700ram[adr] = echoL & 0xff;
		spc700ram[(adr + 1) & 0xffff] = (unsigned char)(echoL >> 8);
		spc700ram[(adr + 2) & 0xffff] = echoR & 0xff;
		spc700ram[(adr + 3) & 0xffff] = (unsigned char)(echoR >> 8);
	}
	
	// handle indexes
	if (echoBufferIndex == 0) 
	{
		echoLength = echoDelay * 4;
	}
	
	echoBufferIndex += 4;
	if (echoBufferIndex >= echoLength) 
	{
		echoBufferIndex = 0;
	}
}


//

static const int rateValues[32] = {
  0, 2048, 1536, 1280, 1024, 768, 640, 512,
  384, 320, 256, 192, 160, 128, 96, 80,
  64, 48, 40, 32, 24, 20, 16, 12,
  10, 8, 6, 5, 4, 3, 2, 1
};

static const int rateOffsets[32] = {
  0, 0, 1040, 536, 0, 1040, 536, 0, 1040, 536, 0, 1040, 536, 0, 1040, 536,
  0, 1040, 536, 0, 1040, 536, 0, 1040, 536, 0, 1040, 536, 0, 1040, 536, 0
};

bool apu::dspCheckCounter(int rate)
{
	if (rate == 0) return false;
	return ((counter + rateOffsets[rate]) % rateValues[rate]) == 0;
}

void apu::dspCycle(signed short int& sampleOutL, signed short int& sampleOutR, float sampleRate)
{
	echoOutL = 0;
	echoOutR = 0;

	//unsigned int ch = 0;
	for (unsigned int ch = 0;ch < 8;ch++)
	{
		//int pitch = channels[ch].samplePitch;
		int pitch = (int)((float)channels[ch].samplePitch*(snesNativeSampleRate/sampleRate));

		//if (ch > 0 && dsp->channel[ch].pitchModulation) {
		//	pitch += ((dsp->channel[ch - 1].sampleOut >> 5) * pitch) >> 10;
		//}

		channels[ch].brrHeader = spc700ram[channels[ch].decodeOffset];

		unsigned short int samplePointer = dspDIR + (4 * channels[ch].sampleSourceEntry);
		if (channels[ch].startDelay == 0) samplePointer += 2;
		
		unsigned short int sampleAdr = spc700ram[samplePointer] | (spc700ram[(samplePointer + 1) & 0xffff] << 8);
		
		// handle starting of sample
		if (channels[ch].startDelay > 0) 
		{
			if (channels[ch].startDelay == 5) 
			{
				// first keyed on
				channels[ch].decodeOffset = sampleAdr;
				channels[ch].blockOffset = 1;
				channels[ch].bufferOffset = 0;
				channels[ch].brrHeader = 0;
				dspRam[0x7c] &= ~(1 << ch); // clear ENDx
			}
			channels[ch].gain = 0;
			channels[ch].startDelay--;
			channels[ch].pitchCounter = 0;
			if (channels[ch].startDelay > 0 && channels[ch].startDelay < 4) 
			{
				channels[ch].pitchCounter = 0x4000;
			}
			pitch = 0;
		}

		// get sample
		int sample = 0;
		if (channels[ch].useNoise) 
		{
			sample = clip16(noiseSample * 2);
		}
		else 
		{
			sample = dspGetSample(ch);
		}

		sample = ((sample * channels[ch].gain) >> 11) & ~1;
		
		// handle reset and release
		if (dspReset || ((channels[ch].brrHeader & 0x03) == 1) ) 
		{
			channels[ch].adsrState = 3; // go to release
			channels[ch].gain = 0;
		}
		
		// handle keyon/keyoff
		if (evenCycle) 
		{
			if (channels[ch].keyOff) 
			{
				channels[ch].adsrState = 3; // go to release
			}
			if (channels[ch].keyOn) 
			{
				channels[ch].startDelay = 5;
				channels[ch].adsrState = 0; // go to attack
				channels[ch].keyOn = false;
			}
		}
		
		// handle envelope
		if (channels[ch].startDelay == 0) 
		{
			dspHandleGain(ch);
		}
		
		// decode new brr samples if needed and update offsets
		if (channels[ch].pitchCounter >= 0x4000) 
		{
			dspDecodeBrr(ch);
			if (channels[ch].blockOffset >= 7) 
			{
				if (channels[ch].brrHeader & 0x1) 
				{
					channels[ch].decodeOffset = sampleAdr;
					dspRam[0x7c] |= 1 << ch; // set ENDx
				}
				else 
				{
					channels[ch].decodeOffset += 9;
				}
				channels[ch].blockOffset = 1;
			}
			else 
			{
				channels[ch].blockOffset += 2;
			}
		}

		// update pitch counter
		channels[ch].pitchCounter &= 0x3fff;
		channels[ch].pitchCounter += (unsigned short)pitch;
		if (channels[ch].pitchCounter > 0x7fff) channels[ch].pitchCounter = 0x7fff;
		
		// set outputs
		dspRam[(ch << 4) | 8] = (unsigned char)(channels[ch].gain >> 4);
		dspRam[(ch << 4) | 9] = (unsigned char)(sample >> 8);
		
		sampleOutL = (signed short)clamp16(sampleOutL + ((sample * channels[ch].leftVol) >> 7));
		sampleOutR = (signed short)clamp16(sampleOutR + ((sample * channels[ch].rightVol) >> 7));

		if (channels[ch].echoEnable) 
		{
			echoOutL = clamp16(echoOutL + ((sample * channels[ch].leftVol) >> 7));
			echoOutR = clamp16(echoOutR + ((sample * channels[ch].rightVol) >> 7));
		}
	}

	dspHandleEcho(sampleOutL,sampleOutR);

	counter = counter == 0 ? 30720 : counter - 1;
	dspHandleNoise();
	evenCycle = !evenCycle;

	if (mute)
	{
		sampleOutL = 0;
		sampleOutR = 0;
	}
}

void apu::step(audioSystem& theAudioSys)
{
	if (apufcounter>=9.4)
	//if ((apuCycles%8) == 0) 
	{
		apufcounter = 0.0;

		signed short int l = 0;
		signed short int r = 0;
		
		dspCycle(l, r, theAudioSys.sampleRate);

		float lf = (float)l / (0x8000);
		float rf = (float)r / (0x8000);

		theAudioSys.feedAudiobuf(lf, rf);
	}

	for (int k = 0;k < 2;k++)
	{
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
	}

	apuCycles++;
	apufcounter += theAudioSys.internalAudioInc;
}

//
// SPC700
//

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

unsigned short int apu::addrIndP()
{
	return regX++ | ((flagP ? 1 : 0) << 8);
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
	unsigned char lowaddr = (this->*read8)((regPC + 1)&0xffff);
	unsigned char highaddr = (this->*read8)((regPC + 2)&0xffff);
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

unsigned char apu::addrAbsBit(unsigned short int* adr)
{
	unsigned char lowaddr = (this->*read8)(regPC + 1);
	unsigned char highaddr = (this->*read8)(regPC + 2);
	unsigned short int adrBit = lowaddr | (highaddr << 8);
	*adr = adrBit & 0x1fff;
	return adrBit >> 13;
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
		case 0x8b:
		{
			// DEC dp
			unsigned short int addr = addrDP();
			unsigned char val = (this->*read8)(addr);
			val--;
			(this->*write8)(addr, val);
			doFlagsNZ(val);
			regPC += 2;
			cycles = 4;
			break;
		}
		case 0x1a:
		{
			// DECW dp
			unsigned char adr = (this->*read8)(regPC + 1);
			unsigned short int low = adr;
			if (flagP) low |= 0x100;
			unsigned short int high = (adr + 1) & 0xff;
			if (flagP) high |= 0x100;
			unsigned char val1 = (this->*read8)(low);
			unsigned short int val = val1 | ((this->*read8)(high) << 8);

			val -= 1;

			(this->*write8)(low, val & 0xff);
			(this->*write8)(high, val >> 8);

			flagZ = val == 0;
			flagN = val & 0x8000;

			regPC += 2;
			cycles = 6;
			break;
		}
		case 0x5a:
		{
			// CMPW YA,dp

			unsigned char adr = (this->*read8)(regPC + 1);
			unsigned short int low = adr;
			if (flagP) low |= 0x100;
			unsigned short int high = (adr + 1) & 0xff;
			if (flagP) high |= 0x100;
			unsigned char val1 = (this->*read8)(low);
			unsigned short int val = val1 | ((this->*read8)(high) << 8);

			unsigned short int value = val ^ 0xffff;
			unsigned short int ya = regA | (regY << 8);
			int result = ya + value + 1;
			
			flagC = result > 0xffff;
			flagZ = (result & 0xffff) == 0;
			flagN = result & 0x8000;

			regPC += 2;
			cycles = 4;
			break;
		}
		case 0x94:
		{
			// ADC A,d+x
			doAdc(&apu::addrDPX);
			regPC += 2;
			cycles = 4;
			break;
		}
		case 0xe6:
		{
			// MOV A,(X)
			doMoveToA(&apu::addrModeX);
			regPC += 1;
			cycles = 3;
			break;
		}
		case 0x69:
		{
			// CMP dp,dp

			unsigned short int adr0 = (this->*read8)(regPC + 1);
			if (flagP) adr0 |= 0x100;
			unsigned short int adr1 = (this->*read8)(regPC + 2);
			if (flagP) adr1 |= 0x100;

			unsigned char val0 = (this->*read8)(adr0);
			unsigned char val1 = (this->*read8)(adr1);

			val0 ^= 0xff;
			int result = val1 + val0 + 1;
			flagC = result > 0xff;
			doFlagsNZ((unsigned char)result);

			regPC += 3;
			cycles = 6;
			break;
		}
		case 0x00:
		{
			// NOPPy
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0x3c:
		{
			// ROL A
			bool newC = regA & 0x80;
			regA = (regA << 1) | (flagC?1:0);
			flagC = newC;
			doFlagsNZ(regA);
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0x76:
		{
			// CMP A,!a+Y
			doCMPA(&apu::addrAbsY);
			regPC += 3;
			cycles = 5;
			break;
		}
		case 0x0e:
		{
			// TSET1 !a
			unsigned short int adr = addrAbs();
			unsigned char val = (this->*read8)(adr);

			unsigned char result = regA + (val ^ 0xff) + 1;
			doFlagsNZ(result);

			(this->*write8)(adr, val | regA);

			regPC += 3;
			cycles = 6;
			break;
		}
		case 0x44:
		{
			// EOR A,d
			doEor(&apu::addrDP);
			regPC += 2;
			cycles = 3;
			break;
		}
		case 0xed:
		{
			// NOTC
			flagC = !flagC;
			regPC += 1;
			cycles = 3;
			break;
		}
		case 0x6b:
		{
			// ROR d
			unsigned short int addr = addrDP();
			unsigned char val = (this->*read8)(addr);
			bool newC = val & 1;
			val = (val >> 1) | ((flagC?1:0) << 7);
			flagC = newC;
			(this->*write8)(addr, val);
			doFlagsNZ(val);
			regPC += 2;
			cycles = 4;
			break;
		}
		case 0x05:
		{
			// OR A,!a
			unsigned short int addr = addrAbs();
			unsigned char val = (this->*read8)(addr);
			regA |= val;
			doFlagsNZ(regA);
			regPC += 3;
			cycles = 4;
			break;
		}
		case 0x016:
		{
			// OR A,!a+Y
			unsigned short int addr = addrAbsY();
			unsigned char val = (this->*read8)(addr);
			regA |= val;
			doFlagsNZ(regA);
			regPC += 3;
			cycles = 5;
			break;
		}
		case 0xe9:
		{
			// MOV X,!a
			doMoveToX(&apu::addrAbs);
			regPC += 3;
			cycles = 4;
			break;
		}
		case 0x15:
		{
			// OR A,!a+X
			unsigned short int addr = addrAbsX();
			regA |= (this->*read8)(addr);
			doFlagsNZ(regA);
			regPC += 3;
			cycles = 5;
			break;
		}
		case 0x0d:
		{
			// PUSH PSW
			unsigned char flagz= (flagN?1:0) << 7;
			flagz |= (flagV ? 1 : 0) << 6;
			flagz |= (flagP ? 1 : 0) << 5;
			flagz |= (flagB ? 1 : 0) << 4;
			flagz |= (flagH ? 1 : 0) << 3;
			flagz |= (flagI ? 1 : 0) << 2;
			flagz |= (flagZ ? 1 : 0) << 1;
			flagz |= (flagC ? 1 : 0);

			(this->*write8)(0x100 | regSP, flagz);
			regSP--;
			regSP &= 0xff;

			regPC += 1;
			cycles = 4;
			break;
		}
		case 0x36:
		{
			// AND A,!a+Y
			unsigned short int addr = addrAbsY();
			regA &= (this->*read8)(addr);
			doFlagsNZ(regA);
			regPC += 3;
			cycles = 5;
			break;
		}
		case 0x38:
		{
			// AND d,#i
			unsigned char value = (this->*read8)(regPC + 1);
			unsigned short int addrdst = addrImmediate8();

			unsigned char result = (this->*read8)(addrdst) & value;
			(this->*write8)(addrdst,result);
			doFlagsNZ(result);

			regPC += 3;
			cycles = 5;
			break;
		}
		case 0x18:
		{
			// OR d,#i
			unsigned char value = (this->*read8)(regPC + 1);
			unsigned short int addrdst = addrImmediate8();

			unsigned char result = (this->*read8)(addrdst) | value;
			(this->*write8)(addrdst, result);
			doFlagsNZ(result);

			regPC += 3;
			cycles = 5;
			break;
		}
		case 0x2b:
		{
			// ROL d
			unsigned short int addr = addrDP();
			unsigned char val = (this->*read8)(addr);

			bool newC = val & 0x80;
			val = (val << 1) | (flagC ? 1 : 0);
			flagC = newC;
			doFlagsNZ(val);

			(this->*write8)(addr, val);

			regPC += 2;
			cycles = 4;
			break;
		}
		case 0x4e:
		{
			// TCLR1 !a
			unsigned short int adr = addrAbs();
			unsigned char val = (this->*read8)(adr);

			unsigned char result = regA + (val ^ 0xff) + 1;
			doFlagsNZ(result);

			(this->*write8)(adr, val & ~regA);

			regPC += 3;
			cycles = 6;
			break;
		}
		case 0xbf:
		{
			// MOV A,(X)+
			unsigned short int addr = addrIndP();
			regA = (this->*read8)(addr);
			doFlagsNZ(regA);

			regPC += 1;
			cycles = 4;
			break;
		}
		case 0xb8:
		{
			// SBC d,#imm
			unsigned char value = (this->*read8)(regPC + 1);
			unsigned short int adrDst = (this->*read8)(regPC + 2);
			if (flagP) adrDst |= 0x100;

			int fc = flagC ? 1 : 0;
			value ^= 0xff;
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
		case 0x89:
		{
			// ADC dd,ds
			unsigned short int adr0 = (this->*read8)(regPC + 1);
			if (flagP) adr0 |= 0x100;
			unsigned short int adr1 = (this->*read8)(regPC + 2);
			if (flagP) adr1 |= 0x100;

			unsigned char val0 = (this->*read8)(adr0);

			unsigned char applyOn = (this->*read8)(adr1);
			int result = applyOn + val0 + (flagC?1:0);
			flagV = (applyOn & 0x80) == (val0 & 0x80) && (val0 & 0x80) != (result & 0x80);
			flagH = ((applyOn & 0xf) + (val0 & 0xf) + (flagC ? 1 : 0)) > 0xf;
			flagC = result > 0xff;
			(this->*write8)(adr1,(unsigned char)result);
			doFlagsNZ((unsigned char)result);

			regPC += 3;
			cycles = 6;
			break;
		}
		case 0x14:
		{
			// OR A,d+X
			unsigned short int addr = addrDPX();
			unsigned char val = (this->*read8)(addr);
			regA |= val;
			doFlagsNZ(regA);
			regPC += 2;
			cycles = 4;
			break;
		}
		case 0x25:
		{
			// AND A,!a
			unsigned short int addr = addrAbs();
			unsigned char val = (this->*read8)(addr);
			regA &= val;
			doFlagsNZ(regA);
			regPC += 3;
			cycles = 4;
			break;
		}
		case 0x4c:
		{
			// LSR !a
			doLsr(&apu::addrAbs);
			regPC += 3;
			cycles = 5;
			break;
		}
		case 0x26:
		{
			// AND A,(X)
			unsigned short int addr = addrModeX();
			unsigned char val = (this->*read8)(addr);
			regA &= val;
			doFlagsNZ(regA);
			regPC += 1;
			cycles = 3;
			break;
		}
		case 0x0c:
		{
			// ASL !a
			unsigned short int addr = addrAbs();
			unsigned char val = (this->*read8)(addr);

			flagC = val & 0x80;
			val <<= 1;
			doFlagsNZ(val);

			(this->*write8)(addr, val);

			regPC += 3;
			cycles = 5;
			break;
		}
		case 0xa5:
		{
			// SBC A,!a
			unsigned short int addr = addrAbs();

			unsigned char value = (this->*read8)(addr) ^ 0xff;
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
		case 0x34:
		{
			// AND A,!a+X
			unsigned short int addr = addrDPX();
			unsigned char val = (this->*read8)(addr);
			regA &= val;
			doFlagsNZ(regA);
			regPC += 2;
			cycles = 4;
			break;
		}
		case 0x58:
		{
			// EOR d,#i
			unsigned char value = (this->*read8)(regPC + 1);
			unsigned short int addrdst = addrImmediate8();

			unsigned char result = (this->*read8)(addrdst) ^ value;

			(this->*write8)(addrdst, result);
			doFlagsNZ(result);

			regPC += 3;
			cycles = 5;
			break;
		}
		case 0x1e:
		{
			// CMP X,!a
			doCMPX(&apu::addrAbs);
			regPC += 3;
			cycles = 4;
			break;
		}
		case 0xb4:
		{
			// SBC A,!a+X
			unsigned short int addr = addrDPX();
			
			unsigned char value = (this->*read8)(addr) ^ 0xff;
			int result = regA + value + (flagC ? 1 : 0);
			flagV = (regA & 0x80) == (value & 0x80) && (value & 0x80) != (result & 0x80);
			flagH = ((regA & 0xf) + (value & 0xf) + (flagC ? 1 : 0)) > 0xf;
			flagC = result > 0xff;
			regA = (unsigned char)result;

			doFlagsNZ(regA);
			regPC += 2;
			cycles = 4;
			break;
		}
		case 0x77:
		{
			// CMP A,[d]+Y
			doCMPA(&apu::addrIndirectY);
			regPC += 2;
			cycles = 6;
			break;
		}
		case 0xb7:
		{
			// SBC A,[d]+Y
			unsigned short int addr = addrIndirectY();

			unsigned char value = (this->*read8)(addr) ^ 0xff;
			int result = regA + value + (flagC ? 1 : 0);
			flagV = (regA & 0x80) == (value & 0x80) && (value & 0x80) != (result & 0x80);
			flagH = ((regA & 0xf) + (value & 0xf) + (flagC ? 1 : 0)) > 0xf;
			flagC = result > 0xff;
			regA = (unsigned char)result;

			doFlagsNZ(regA);
			regPC += 2;
			cycles = 6;
			break;
		}
		case 0x49:
		{
			// EOR dd,ds
			unsigned short int adr0 = (this->*read8)(regPC + 1);
			if (flagP) adr0 |= 0x100;
			unsigned short int adr1 = (this->*read8)(regPC + 2);
			if (flagP) adr1 |= 0x100;

			unsigned char val0 = (this->*read8)(adr0);
			unsigned char val1 = (this->*read8)(adr1);

			unsigned char result = val0 ^ val1;
			(this->*write8)(adr1, result);
			doFlagsNZ(result);

			regPC += 3;
			cycles = 6;
			break;
		}
		case 0x70:
		{
			// BVS offs
			signed char offs = (this->*read8)(regPC + 1);
			cycles = doBranch(offs, flagV);
			break;
		}
		case 0x17:
		{
			// OR A,[d]+Y
			unsigned short int addr = addrIndirectY();
			unsigned char value = (this->*read8)(addr);

			regA |= value;
			doFlagsNZ(regA);

			regPC += 2;
			cycles = 6;
			break;
		}
		case 0x1b:
		{
			// ASL a+X
			unsigned short int addr = addrDPX();
			unsigned char value = (this->*read8)(addr);
			flagC = value & 0x80;
			value <<= 1;
			doFlagsNZ(value);
			(this->*write8)(addr, value);

			regPC += 2;
			cycles = 5;
			break;
		}
		case 0xaa:
		{
			// MOV1 abs.bit
			unsigned short int adr = 0;
			unsigned char bit = addrAbsBit(&adr);
			flagC = ((this->*read8)(adr) >> bit) & 1;

			regPC += 3;
			cycles = 4;
			break;
		}
		case 0x8a:
		{
			// EOR1 abs.bit
			unsigned short int adr = 0;
			unsigned char bit = addrAbsBit(&adr);
			flagC = flagC ^ (((this->*read8)(adr) >> bit) & 1);

			regPC += 3;
			cycles = 5;
			break;
		}
		case 0xa9:
		{
			// SBC dd,ds
			unsigned short int adr0 = (this->*read8)(regPC + 1);
			if (flagP) adr0 |= 0x100;
			unsigned short int adr1 = (this->*read8)(regPC + 2);
			if (flagP) adr1 |= 0x100;

			unsigned char val1 = (this->*read8)(adr1);

			unsigned char value = (this->*read8)(adr0) ^ 0xff;
			int result = val1 + value + (flagC ? 1 : 0);
			flagV = (val1 & 0x80) == (value & 0x80) && (value & 0x80) != (result & 0x80);
			flagH = ((val1 & 0xf) + (value & 0xf) + (flagC ? 1 : 0)) > 0xf;
			flagC = result > 0xff;
			val1 = (unsigned char)result;
			doFlagsNZ(val1);

			(this->*write8)(adr1, (unsigned char)result);

			regPC += 3;
			cycles = 6;
			break;
		}
		case 0x35:
		{
			// AND A,!a+X
			unsigned short int addr = addrAbsX();
			regA &= (this->*read8)(addr);
			doFlagsNZ(regA);
			regPC += 3;
			cycles = 5;
			break;
		}
		case 0x45:
		{
			// EOR A,!a
			doEor(&apu::addrAbs);
			regPC += 3;
			cycles = 4;
			break;
		}
		case 0x66:
		{
			// CMP A,(X)
			doCMPA(&apu::addrModeX);
			regPC += 1;
			cycles = 3;
			break;
		}
		case 0x56:
		{
			// EOR A,!a+Y
			doEor(&apu::addrAbsY);
			regPC += 3;
			cycles = 5;
			break;
		}
		case 0xea:
		{
			// NOT1 abs.bit
			unsigned short int adr = 0;
			unsigned char bit = addrAbsBit(&adr);

			unsigned char result = (this->*read8)(adr) ^ (1 << bit);
			(this->*write8)(adr, result);

			regPC += 3;
			cycles = 5;
			break;
		}
		case 0x54:
		{
			// EOR A,d+X
			doEor(&apu::addrDPX);
			regPC += 2;
			cycles = 4;
			break;
		}
		case 0xa0:
		{
			// EI
			flagI = true;
			regPC += 1;
			cycles = 3;
			break;
		}
		case 0x01:
		case 0x11:
		case 0x21:
		case 0x31:
		case 0x41:
		case 0x51:
		case 0x61:
		case 0x71:
		case 0x81:
		case 0x91:
		case 0xa1:
		case 0xb1:
		case 0xc1:
		case 0xd1:
		case 0xe1:
		case 0xf1:
		{
			// TCALL nn
			regPC += 1;
			(this->*write8)(0x100 | regSP, regPC>>8);
			regSP--;
			regSP &= 0xff;
			(this->*write8)(0x100 | regSP, regPC&0xff);
			regSP--;
			regSP &= 0xff;

			unsigned short int adr = 0xffde - (2 * (nextOpcode >> 4));

			unsigned char low = (this->*read8)(adr);
			unsigned char high = (this->*read8)(adr+1);
			unsigned short int newPC = low | (high<<8);

			regPC = newPC;

			cycles = 8;
			break;
		}
		case 0x9d:
		{
			// MOV X,SP
			regX = regSP & 0xff;
			doFlagsNZ(regX);
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0x5b:
		{
			// LSR d+X
			doLsr(&apu::addrDPX);
			regPC += 2;
			cycles = 5;
			break;
		}
		case 0x7b:
		{
			// ROR d+X
			unsigned short int addr = addrDPX();
			unsigned char val = (this->*read8)(addr);
			bool newC = val & 1;
			val = (val >> 1) | ((flagC ? 1 : 0) << 7);
			flagC = newC;
			doFlagsNZ(val);
			(this->*write8)(addr, val);

			regPC += 2;
			cycles = 5;
			break;
		}
		case 0x37:
		{
			// AND A, [d]+Y
			doAnd(&apu::addrIndirectY);
			regPC += 2;
			cycles = 6;
			break;
		}
		case 0x57:
		{
			// EOR A, [d]+Y
			doEor(&apu::addrIndirectY);
			regPC += 2;
			cycles = 6;
			break;
		}
		case 0xe0:
		{
			// CLRV
			flagV = false;
			flagH = false;
			regPC += 1;
			cycles = 2;
			break;
		}
		case 0x6c:
		{
			// ROR !a
			unsigned short int addr = addrAbs();
			unsigned char val = (this->*read8)(addr);

			bool newC = val & 1;
			val = (val >> 1) | ((flagC ? 1 : 0) << 7);
			flagC = newC;
			doFlagsNZ(val);

			(this->*write8)(addr, val);

			regPC += 3;
			cycles = 5;
			break;
		}
		case 0x4f:
		{
			// PCALL u
			signed char offs = (this->*read8)(regPC + 1);

			(this->*write8)(0x100 | regSP, (regPC+2) >> 8);
			regSP--;
			regSP &= 0xff;
			(this->*write8)(0x100 | regSP, (regPC+2) & 0xff);
			regSP--;
			regSP &= 0xff;

			regPC = 0xff00 | offs;

			cycles = 6;
			break;
		}
		case 0xd9:
		{
			// MOV d+Y,X
			doMoveWithRead(&apu::addrDPY, regX);
			regPC += 2;
			cycles = 5;
			break;
		}
		case 0x50:
		{
			// BVC offs
			signed char offs = (this->*read8)(regPC + 1);
			cycles = doBranch(offs, !flagV);
			break;
		}
		case 0xf9:
		{
			// MOV X,d+Y
			doMoveToX(&apu::addrDPY);
			regPC += 2;
			cycles = 4;
			break;
		}
		case 0x39:
		{
			// AND (X),(Y)
			unsigned char src = (this->*read8)(regY | ((flagP?1:0) << 8));
			unsigned char dst= (this->*read8)(regX | ((flagP ? 1 : 0) << 8));

			dst &= src;
			doFlagsNZ(dst);

			(this->*write8)(regX | ((flagP ? 1 : 0) << 8), dst);

			regPC += 1;
			cycles = 5;
			break;
		}
		case 0xca:
		{
			// MOV1 m.b,C
			unsigned short int adr = 0;
			unsigned char bit = addrAbsBit(&adr);
			unsigned char result = ((this->*read8)(adr) & (~(1 << bit))) | ((flagC?1:0) << bit);
			(this->*write8)(adr, result);

			regPC += 3;
			cycles = 6;
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
