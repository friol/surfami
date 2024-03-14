
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
	
	const unsigned int audioBufLen = 1024;
	float audioBuf[1024*2];
	
	float* outwavbuf;
	signed long int outwavpos = 0;
	const unsigned long int outwavdim = 65536*2;
	
	std::vector<float> audioStream;

	spcInstrument insArray[0x14];

	void loadTestSamples();

public:

	bool audioSystemInited = false;

	audioSystem();
	void updateStream(apu& theAPU);
	void feedAudiobuf(float l, float r);
	~audioSystem();
};

#endif
