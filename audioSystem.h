
#ifndef AUDIOSYSTEM_H
#define AUDIOSYSTEM_H

#include <vector>
#include <iomanip>
#include <sstream>
#include "bass/bass.h"
#include "apu.h"
#include "logger.h"
#include "include/AudioFile.h"

class apu;

struct spcInstrument
{
	float* data;
	unsigned int len = 0;
};

class audioSystem
{
private:

	unsigned int bufPos = 0;
	float audioBuf[1024];
	std::vector<float> audioStream;

	spcInstrument insArray[0x14];

	void loadTestSamples();

public:

	bool audioSystemInited = false;

	audioSystem();
	void updateStream(apu& theAPU);
	~audioSystem();
};

#endif
