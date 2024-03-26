
#include "audioSystem.h"

extern logger glbTheLogger;

static std::deque<float> audioQueue;

//

DWORD CALLBACK StreamProc(HSTREAM handle, float* buffer, DWORD length, void* user)
{
	user = user;
	handle = handle;

	float* paudioInc = (float*)user;

	const unsigned int avsize = (unsigned int)audioQueue.size();

	if (length == 0) return 0;

	if (avsize < 2)
	{
		for (unsigned int pos = 0;pos < length / sizeof(float);pos ++)
		{
			buffer[pos] = 0;
		}
			
		return length;
	}

	float nsamplesAva = avsize;
	//glbTheLogger.logMsg("samples available:"+std::to_string(avsize));
	//glbTheLogger.logMsg("audio inc:"+std::to_string((*paudioInc)));
	
	if (nsamplesAva > 15000) (*paudioInc) -= 0.001f;
	else (*paudioInc) += 0.001f;
	
	for (unsigned int pos = 0;pos < length / sizeof(float);pos += 2)
	{
		if (audioQueue.size()>=2)
		{
			buffer[pos] = audioQueue[0];
			audioQueue.pop_front();
			buffer[pos + 1] = audioQueue[0];
			audioQueue.pop_front();
		}
		else
		{
			buffer[pos] = 0;
			buffer[pos + 1] = 0;
			//glbTheLogger.logMsg("not enough samples to fill audio buffer");
		}
	}

	return length;
}

audioSystem::audioSystem()
{
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

	stream = BASS_StreamCreate(info.freq, 2, BASS_SAMPLE_FLOAT, (STREAMPROC*)StreamProc, (void*)&internalAudioInc);
	BASS_ChannelSetAttribute(stream, BASS_ATTRIB_BUFFER, 0.25);
	BASS_ChannelPlay(stream, FALSE);

	//stream = BASS_StreamCreate(info.freq, 2, BASS_SAMPLE_FLOAT, STREAMPROC_PUSH, NULL);
	//BASS_ChannelSetAttribute(stream, BASS_ATTRIB_BUFFER, 0.45);
	//BASS_ChannelPlay(stream, FALSE);

	audioSystemInited = true;
	glbTheLogger.logMsg("Audio system inited; sample rate: "+std::to_string((int)sampleRate));
}

void audioSystem::feedAudiobuf(float l, float r)
{
	audioQueue.push_back(l);
	audioQueue.push_back(r);
}

audioSystem::~audioSystem()
{
	BASS_Free();
}
