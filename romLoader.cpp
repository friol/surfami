
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
	file.seekg(0, std::ios::beg);

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

	if (romContents.size() != 0x8000)
	{
		// other roms not supported for now
		return 1;
	}

	for (int i = 0;i < 0x8000;i++)
	{
		theMMU.write8(i + 0x8000, romContents[i]);
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
