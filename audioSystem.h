
#ifndef AUDIOSYSTEM_H
#define AUDIOSYSTEM_H

#include <vector>
#include <iomanip>
#include <sstream>
#include "bass/bass.h"
#include "apu.h"
#include "logger.h"
//#include "include/AudioFile.h"

class apu;

class audioSystem
{
private:

	unsigned int bufPos = 0;
	
	const unsigned int audioBufLen = 1024;
	float audioBuf[1024*2];
	float tmpbuf[4096*4];
	
	HSTREAM stream;
	std::vector<float> audioStream;

public:

	bool audioSystemInited = false;
	float sampleRate = 0;
	float internalAudioInc = 1.0;

	audioSystem();
	void feedAudiobuf(float l, float r);
	~audioSystem();
};

#endif
