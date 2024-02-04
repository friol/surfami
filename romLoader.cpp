
#include "romLoader.h"

romLoader::romLoader()
{
}

std::vector<unsigned char> romLoader::readFile(std::string& filename,int& error)
{
	std::vector<unsigned char> vec;

	error = 0;
	std::ifstream file(filename, std::ios::binary);
	if (!file)
	{
		error = 1;
		return vec;
	}

	file.unsetf(std::ios::skipws);

	std::streampos fileSize;
	file.seekg(0, std::ios::end);
	fileSize = file.tellg();

	if (strstr(filename.c_str(), ".smc"))
	{
		file.seekg(512, std::ios::beg);
	}
	else
	{
		file.seekg(0, std::ios::beg);
	}

	vec.reserve(fileSize);

	vec.insert(vec.begin(),std::istream_iterator<unsigned char>(file),std::istream_iterator<unsigned char>());

	return vec;
}

int romLoader::loadRom(std::string& romPath,mmu& theMMU,std::vector<std::string>& loadLog,bool isHirom)
{
	std::vector<unsigned char> romContents;

	loadLog.push_back("Loading rom [" + romPath+"]");

	int error = 0;
	romContents = readFile(romPath,error);
	if (error != 0)
	{
		loadLog.push_back("Error loading rom.");
		return 1;
	}

	loadLog.push_back("ROM was read correctly. Size is " + std::to_string(romContents.size()/1024) + "kb. HiRom: "+std::to_string(isHirom));

	unsigned char* pSnesRAM = theMMU.getInternalRAMPtr();
	unsigned short int filesizeInKb = (unsigned short int)(romContents.size() / 1024);

	if (!isHirom)
	{
		unsigned char bank = 0x80;
		unsigned char shadow_bank = 0x00;
		unsigned char chunks = (unsigned char)(filesizeInKb / 32);
		for (unsigned long int i = 0; i < romContents.size(); i++)
		{
			//	write to all locations that mirror our ROM
			for (unsigned char mirror = 0; mirror < (0x80 / chunks); mirror++)
			{
				//	(Q3-Q4) 32k (0x8000) consecutive chunks to banks 0x80-0xff, upper halfs (0x8000-0xffff)
				if ((bank + (i / 0x8000) + (mirror * chunks)) < 0xff)
				{
					pSnesRAM[((bank + (i / 0x8000) + (mirror * chunks)) << 16) | 0x8000 + (i % 0x8000)] = romContents[i];
				}

				//	(Q1-Q2) shadowing to banks 0x00-0x7d, except WRAM (bank 0x7e/0x7f)
				if ((shadow_bank + (i / 0x8000) + (mirror * chunks)) < 0x7e)
				{
					pSnesRAM[((shadow_bank + (i / 0x8000) + (mirror * chunks)) << 16) | 0x8000 + (i % 0x8000)] = romContents[i];
				}
			}
		}
	}
	else
	{
		for (unsigned long int i = 0; i < romContents.size(); i++)
		{
			pSnesRAM[0xc00000|i]= romContents[i];
			/*The unused region in banks $40 - 7D will normally be a mirror of $C0 - FD, though there are minor variations.*/
			pSnesRAM[0x400000|i] = romContents[i];
		}

		unsigned int pos = 0;
		for (unsigned int addr = 0;addr < 0x400000;addr++)
		{
			if (pos >= 0x8000)
			{
				pSnesRAM[addr] = pSnesRAM[addr|0xc00000];
			}

			pos++;
			pos %= 0x10000;
		}

		pos = 0;
		for (unsigned int addr = 0x800000;addr < 0xc00000;addr++)
		{
			if (pos >= 0x8000)
			{
				pSnesRAM[addr] = pSnesRAM[addr | 0xc00000];
			}

			pos++;
			pos %= 0x10000;
		}
	}

	// load SRAM, if present
	int romBeg=(int)romPath.find_last_of("/\\")+1;
	std::string romName = romPath.substr(romBeg);
	int pointPos = (int)romName.find_last_of(".");
	std::string onlyRomName = romName.substr(0, pointPos);
	std::string sramFileName = "D:\\prova\\snes\\sram\\" + onlyRomName + ".srm";

	std::vector<unsigned char> sramData;
	std::ifstream file(sramFileName, std::ios::binary);
	if (file)
	{
		file.unsetf(std::ios::skipws);

		std::streampos fileSize;
		file.seekg(0, std::ios::end);
		fileSize = file.tellg();
		file.seekg(0, std::ios::beg);

		sramData.reserve(fileSize);
		sramData.insert(sramData.begin(), std::istream_iterator<unsigned char>(file), std::istream_iterator<unsigned char>());

		for (int pos = 0;pos < fileSize;pos++)
		{
			theMMU.write8(0x700000 + pos, sramData[pos]);
		}

		theMMU.hasSram(sramFileName);
	}

	// RESET (emulation)	0xFFFC / 0xFFFD
	int loReset = theMMU.read8(0xfffc);
	int hiReset = theMMU.read8(0xfffd);
	int resetVector = loReset | (hiReset << 8);

	std::stringstream strr;
	strr << std::hex << resetVector;
	std::string hexvec(strr.str());

	loadLog.push_back("Reset vector at 0x"+ hexvec);

	loadLog.push_back("Finished loading rom.");
	return 0;
}
