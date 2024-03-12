
#include "math.h"
#include "audioSystem.h"

extern logger glbTheLogger;

DWORD CALLBACK StreamProc(HSTREAM handle, float* buffer, DWORD length, void* user)
{
	float* abuf = (float*)user;
	for (int pos = 0;pos < length / sizeof(float);pos+=2)
	{
		buffer[pos] = abuf[pos/2];
		buffer[pos + 1] = abuf[pos/2];
	}

	return length;
}

audioSystem::audioSystem()
{
	HSTREAM stream;

	//if (HIWORD(BASS_GetVersion()) != BASSVERSION) {
	//	printf("An incorrect version of BASS.DLL was loaded");
	//	return 0;
	//}

	float ang = 0;
	for (int pos = 0;pos < 1024;pos++)
	{
		audioBuf[pos] = 0;//sin(ang);
		ang += 0.01;
	}

	if (!BASS_Init(-1, 44100, 0, 0, NULL))
	{
		return;
	}

	BASS_INFO info;
	BASS_GetInfo(&info);

	stream = BASS_StreamCreate(info.freq, 2, BASS_SAMPLE_FLOAT, (STREAMPROC*)StreamProc, (void*)audioBuf);
	BASS_ChannelSetAttribute(stream, BASS_ATTRIB_BUFFER, 0);
	BASS_ChannelPlay(stream, FALSE);

	loadTestSamples();

	audioSystemInited = true;
	glbTheLogger.logMsg("Audio system inited.");
}

void audioSystem::loadTestSamples()
{
	for (unsigned int ins = 0;ins < 0x14;ins++)
	{
		AudioFile<float> audioFile;
		std::string smpName = "../../spcsamples/02 Title_";

		std::stringstream strr;
		strr << std::hex << std::setw(2) << std::setfill('0') << ins;

		smpName += strr.str();
		smpName += ".wav";

		audioFile.load(smpName.c_str());
		int numSamples = audioFile.getNumSamplesPerChannel();

		glbTheLogger.logMsg("Loaded sample "+strr.str() + " of len " + std::to_string(numSamples));

		insArray[ins].len = numSamples;
		insArray[ins].data = new float[numSamples];
		for (unsigned int s = 0;s < numSamples;s++)
		{
			insArray[ins].data[s]= audioFile.samples[0][s];
		}
	}
}

void audioSystem::updateStream(apu& theAPU)
{
	float res = 0.0;
	for (unsigned int channel = 0;channel < 8;channel++)
	{
		if ((theAPU.channels[channel].keyOn)&& (!theAPU.channels[channel].keyOff))
		{
			int sampleNum = theAPU.channels[channel].sampleSourceEntry;
			if (sampleNum < 0x14)
			{
				if (theAPU.channels[channel].playingPos < insArray[sampleNum].len)
				{
					res += insArray[sampleNum].data[(unsigned int)theAPU.channels[channel].playingPos];

					float finc = ((float)theAPU.channels[channel].samplePitch) / (65536.0/8.0);

					theAPU.channels[channel].playingPos += finc;
				}
			}
		}
	}

	res /= 8.0;

	audioBuf[bufPos] = res;
	bufPos += 1;
	if (bufPos >= 1024) bufPos = 0;
}

audioSystem::~audioSystem()
{
	BASS_Free();
}
