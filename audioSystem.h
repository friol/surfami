
#ifndef AUDIOSYSTEM_H
#define AUDIOSYSTEM_H

#include <deque>
#include <vector>
#include <iomanip>
#include <sstream>
#include "bass/bass.h"
#include "apu.h"
#include "logger.h"

class apu;

class audioSystem
{
private:

	bool outputAudio = true;
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
	void setAudioOutput(bool v) { outputAudio = v; }
	void feedAudiobuf(float l, float r);
	~audioSystem();
};

#endif
