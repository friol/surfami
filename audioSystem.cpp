
#include "math.h"
#include "audioSystem.h"

extern logger glbTheLogger;

std::vector<float> audioVector;
static bool travasing = false;
float buffaPos = 0;


//

DWORD CALLBACK StreamProc(HSTREAM handle, float* buffer, DWORD length, void* user)
{
	user = user;
	handle = handle;

	float* paudioInc = (float*)user;

	travasing = true;
	unsigned int avsize = (unsigned int)audioVector.size();

	if (length == 0) return 0;

	if (avsize == 0)
	{
		for (unsigned int pos = 0;pos < length / sizeof(float);pos ++)
		{
			buffer[pos] = 0;
		}
			
		return length;
	}

	float nsamplesAva = avsize - buffaPos;
	//glbTheLogger.logMsg("samples available:"+std::to_string(avsize- buffaPos));
	
	if (nsamplesAva > 15000) (*paudioInc) -= 0.001f;
	else (*paudioInc) += 0.001f;
	
	//float inc = 1.0;
	//inc = (avsize - buffaPos) / (length / sizeof(float));
	//glbTheLogger.logMsg("inc is:" + std::to_string(inc));

	for (unsigned int pos = 0;pos < length / sizeof(float);pos += 2)
	{
		if (buffaPos < avsize)
		{
			buffer[pos] = audioVector[(unsigned int)buffaPos];
			buffer[pos + 1] = audioVector[(unsigned int)buffaPos + 1];
			buffaPos += 2;
		}
		else
		{
			buffer[pos] = 0;
			buffer[pos + 1] = 0;
			//glbTheLogger.logMsg("not enough samples to fill audio buffer");
		}
	}
	
	//audioVector.clear();
	travasing = false;

	return length;
	//return avsize*sizeof(float);
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
	audioVector.push_back(l);
	audioVector.push_back(r);

	/*float samplesPerFrame = (sampleRate / 60.0) * 2;
	DWORD status = BASS_ChannelGetData(stream, NULL, BASS_DATA_AVAILABLE);
	if ((status != -1) && (status < samplesPerFrame))
	{
		const int dim = audioVector.size();
		if (dim>=samplesPerFrame)
		{
			glbTheLogger.logMsg("Available samples:" + std::to_string(status));
			glbTheLogger.logMsg("Feeding:" + std::to_string(dim));
			for (unsigned int pos = 0;pos < dim;pos++)
			{
				tmpbuf[pos] = audioVector[pos];
			}

			BASS_StreamPutData(stream, (void*)tmpbuf, dim);
			audioVector.clear();
		}
	}*/
}

audioSystem::~audioSystem()
{
	BASS_Free();
}
