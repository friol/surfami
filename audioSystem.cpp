
#include "math.h"
#include "audioSystem.h"

extern logger glbTheLogger;

std::vector<float> audioVector;

DWORD CALLBACK StreamProc(HSTREAM handle, float* buffer, DWORD length, void* user)
{
	/*float* abuf = (float*)user;
	for (int pos = 0;pos < length / sizeof(float);pos+=2)
	{
		buffer[pos] = abuf[pos];
		buffer[pos + 1] = abuf[pos+1];
	}*/

	unsigned int avsize = audioVector.size();
	for (int pos = 0;pos < length / sizeof(float);pos += 2)
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

	//if (HIWORD(BASS_GetVersion()) != BASSVERSION) {
	//	printf("An incorrect version of BASS.DLL was loaded");
	//	return 0;
	//}

	for (int pos = 0;pos < audioBufLen*2;pos++)
	{
		audioBuf[pos] = 0;
	}

	if (!BASS_Init(-1, 44100, 0, 0, NULL))
	{
		return;
	}

	BASS_INFO info;
	BASS_GetInfo(&info);
	sampleRate = info.freq;

	stream = BASS_StreamCreate(info.freq, 2, BASS_SAMPLE_FLOAT, (STREAMPROC*)StreamProc, (void*)audioBuf);
	BASS_ChannelSetAttribute(stream, BASS_ATTRIB_BUFFER, 0.25);
	BASS_ChannelPlay(stream, FALSE);

	audioSystemInited = true;
	glbTheLogger.logMsg("Audio system inited; sample rate:"+std::to_string((int)sampleRate));
}

void audioSystem::feedAudiobuf(float l, float r)
{
	audioVector.push_back(l);
	audioVector.push_back(r);

	/*audioBuf[bufPos] = l;
	audioBuf[bufPos+1] = r;
	bufPos += 2;
	if (bufPos >= (audioBufLen*2)) bufPos = 0;*/
}

audioSystem::~audioSystem()
{
	BASS_Free();
}
