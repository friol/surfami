
#ifndef ROMLOADER_H
#define ROMLOADER_H

#include <string>
#include <fstream>
#include <vector>
#include <iomanip>
#include <sstream>

#include "mmu.h"

class romLoader
{
private:

	void checkRomType(std::vector<unsigned char>* romContents, bool& isHirom, int& standard, std::vector<std::string>& loadLog,unsigned int& sramSz);
	std::vector<unsigned char> readFile(std::string& filename,int& error);

public:

	romLoader();
	int loadRom(std::string& romPath,mmu& theMMU,std::vector<std::string>& loadLog, bool& isHirom, int& videoStandard);
};

#endif
