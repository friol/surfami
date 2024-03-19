
#include "math.h"
#include "audioSystem.h"

extern logger glbTheLogger;

std::vector<float> audioVector;

DWORD CALLBACK StreamProc(HSTREAM handle, float* buffer, DWORD length, void* user)
{
	user = user;
	handle = handle;

	unsigned int avsize = (unsigned int)audioVector.size();
	for (unsigned int pos = 0;pos < length / sizeof(float);pos += 2)
	{
		if (pos<avsize)
		{
			buffer[pos] = audioVector[pos];
			buffer[pos + 1] = audioVector[pos + 1];
		}
		else
		{
			buffer[pos] = 0;
			buffer[pos + 1] = 0;
		}
	}

	audioVector.clear();

	return length;
}

audioSystem::audioSystem()
{
	HSTREAM stream;

	if (HIWORD(BASS_GetVersion()) != BASSVERSION) 
	{
		glbTheLogger.logMsg("Incorrect bass.dll version");
		return;
	}

	if (!BASS_Init(-1, 44100, 0, 0, NULL))
	{
		glbTheLogger.logMsg("Could not init BASS audio library");
		return;
	}

	BASS_INFO info;
	BASS_GetInfo(&info);
	sampleRate = (float)info.freq;

	stream = BASS_StreamCreate(info.freq, 2, BASS_SAMPLE_FLOAT, (STREAMPROC*)StreamProc, (void*)audioBuf);
	BASS_ChannelSetAttribute(stream, BASS_ATTRIB_BUFFER, 0.25);
	BASS_ChannelPlay(stream, FALSE);

	audioSystemInited = true;
	glbTheLogger.logMsg("Audio system inited; sample rate: "+std::to_string((int)sampleRate));
}

void audioSystem::feedAudiobuf(float l, float r)
{
	audioVector.push_back(l);
	audioVector.push_back(r);
}

audioSystem::~audioSystem()
{
	BASS_Free();
}
