
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

	if ((fileSize%1024)!=0)
	//if (strstr(filename.c_str(), ".smc"))
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

/*
Country (also implies PAL/NTSC) (FFD9h)
  00h -  International (eg. SGB)  (any)
  00h J  Japan                    (NTSC)
  01h E  USA and Canada           (NTSC)
  02h P  Europe, Oceania, Asia    (PAL)
  03h W  Sweden/Scandinavia       (PAL)
  04h -  Finland                  (PAL)
  05h -  Denmark                  (PAL)
  06h F  France                   (SECAM, PAL-like 50Hz)
  07h H  Holland                  (PAL)
  08h S  Spain                    (PAL)
  09h D  Germany, Austria, Switz  (PAL)
  0Ah I  Italy                    (PAL)
  0Bh C  China, Hong Kong         (PAL)
  0Ch -  Indonesia                (PAL)
  0Dh K  South Korea              (NTSC) (North Korea would be PAL)
  0Eh A  Common (?)               (?)
  0Fh N  Canada                   (NTSC)
  10h B  Brazil                   (PAL-M, NTSC-like 60Hz)
  11h U  Australia                (PAL)
*/

void romLoader::checkRomType(std::vector<unsigned char>* romContents, bool& isHirom, int& standard, std::vector<std::string>& loadLog, unsigned int& sramSz)
{
	if (romContents->size() <= 0x8000)
	{
		isHirom = false;
		standard = 0;
		return;
	}

	int romAdder = 0;
	const int listOfPALCountries[] = {2,3,4,5,7,8,9,0x0a,0x0b,0x0c}; //?

	// check header with sophisticated heuristics
	std::string romnameFromHeader;
	int numAlnumLorom = 0, numAlnumHirom = 0;
	std::string hiromTitle, loromTitle;
	for (unsigned int rampos = 0;rampos < 21;rampos++)
	{
		unsigned char charHi=(*romContents)[0xffc0 + rampos];
		unsigned char charLo=(*romContents)[0xffc0-0x8000 + rampos];
		if (std::isalnum(charHi))
		{
			numAlnumHirom += 1;
			hiromTitle += charHi;
		}

		if (std::isalnum(charLo))
		{
			numAlnumLorom += 1;
			loromTitle += charLo;
		}
	}

	loadLog.push_back("LoRom title:[" + loromTitle + "]");
	loadLog.push_back("HiRom title:["+hiromTitle+"]");

	if (numAlnumHirom > numAlnumLorom) 
	{
		loadLog.push_back("ROM seems HiRom");
		isHirom = true;
	}
	else
	{
		loadLog.push_back("ROM seems LoRom");
		romAdder = -0x8000;
	}

	// rom type from the header
	unsigned char headRomType = (*romContents)[0xffd5 + romAdder];

	std::stringstream strr;
	strr << std::hex << std::setw(2) << std::setfill('0') << (int)headRomType;
	std::string sRomType = strr.str();
	loadLog.push_back("Rom Type:"+sRomType);

	// 0 lorom, 1 hirom
	unsigned char speedMemMap = (*romContents)[0xffd5+ romAdder];
	if ((speedMemMap & 0x0f) == 0)
	{
		loadLog.push_back("Header says it's LoROM");
	}
	else
	{
		loadLog.push_back("Header says it's HiROM");
	}

	// SRAM size
	unsigned char sramSize = (*romContents)[0xffd8 + romAdder];
	int iSRAMsize = (1 << sramSize)*1024;
	loadLog.push_back("SRAM size:" + std::to_string(iSRAMsize));

	if (iSRAMsize > (1024 * 32)) iSRAMsize = 0;

	// 1024 is not considered sram?
	if (iSRAMsize > 1024)
	{
		sramSz = iSRAMsize;
	}
	else
	{
		sramSz = 0;
	}

	// rom country
	unsigned char romCountry = (*romContents)[0xffd9+ romAdder];
	loadLog.push_back("Standard/Country:" + std::to_string(romCountry));

	for (auto ccode: listOfPALCountries)
	{
		if (romCountry == ccode)
		{
			loadLog.push_back("ROM is PAL");
			standard = 1;
			return;
		}
	}

	loadLog.push_back("ROM is NTSC");
}

int romLoader::loadRom(std::string& romPath,mmu& theMMU,std::vector<std::string>& loadLog,bool& isHirom,int& videoStandard)
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

	unsigned int sramSize = 0;
	checkRomType(&romContents, isHirom, videoStandard, loadLog,sramSize);

	//

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
	std::string sramFileName = "sram\\" + onlyRomName + ".srm";

	bool hasSram = false;
	std::vector<unsigned char> sramData;
	std::ifstream file(sramFileName, std::ios::binary);
	if (file)
	{
		hasSram = true;
		loadLog.push_back("SRAM found, loading it.");

		file.unsetf(std::ios::skipws);

		std::streampos fileSize;
		file.seekg(0, std::ios::end);
		fileSize = file.tellg();
		file.seekg(0, std::ios::beg);

		sramData.reserve(fileSize);
		sramData.insert(sramData.begin(), std::istream_iterator<unsigned char>(file), std::istream_iterator<unsigned char>());
	}
	else
	{
		if (sramSize > 0)
		{
			loadLog.push_back("Allocating and saving new SRAM");
			hasSram = true;
			for (unsigned int b = 0;b < sramSize;b++)
			{
				sramData.push_back(0);
			}
		}
	}

	if (hasSram)
	{
		theMMU.hasSram(sramFileName, (unsigned int)sramData.size());
		if (isHirom) theMMU.setHiRom(true);

		for (int pos = 0;pos < sramData.size();pos++)
		{
			if (!isHirom)
			{
				theMMU.write8(0x700000 + pos, sramData[pos]);
			}
			else
			{
				theMMU.write8(0x206000 + pos, sramData[pos]);
			}
		}
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
