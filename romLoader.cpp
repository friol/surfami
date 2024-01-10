
#include "romLoader.h"

romLoader::romLoader()
{
}

std::vector<unsigned char> romLoader::readFile(std::string& filename)
{
	std::ifstream file(filename, std::ios::binary);

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

	std::vector<unsigned char> vec;
	vec.reserve(fileSize);

	vec.insert(vec.begin(),std::istream_iterator<unsigned char>(file),std::istream_iterator<unsigned char>());

	return vec;
}

int romLoader::loadRom(std::string& romPath,mmu& theMMU,std::vector<std::string>& loadLog)
{
	std::vector<unsigned char> romContents;

	loadLog.push_back("Loading rom [" + romPath+"]");
	romContents = readFile(romPath);

	loadLog.push_back("ROM was read correctly. Size is " + std::to_string(romContents.size()/1024) + "kb.");

	unsigned char* pSnesRAM = theMMU.getInternalRAMPtr();
	unsigned short int filesizeInKb = (unsigned short int)(romContents.size() / 1024);

	//	map rom to memory
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
				pSnesRAM[((bank + (i / 0x8000) + (mirror * chunks)) << 16) | 0x8000 + (i % 0x8000)]=romContents[i];
			}

			//	(Q1-Q2) shadowing to banks 0x00-0x7d, except WRAM (bank 0x7e/0x7f)
			if ((shadow_bank + (i / 0x8000) + (mirror * chunks)) < 0x7e) 
			{
				pSnesRAM[((shadow_bank + (i / 0x8000) + (mirror * chunks)) << 16) | 0x8000 + (i % 0x8000)]=romContents[i];
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
