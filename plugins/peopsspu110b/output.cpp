#include <xtl.h>

extern "C" {
#include "stdafx.h"
#include "record.h"
#include "externals.h"
}
#include <stdio.h>


extern "C" unsigned long LastWrite;
extern "C" unsigned long LastPlay;
extern "C" unsigned int LastPlayTotal;
extern "C" unsigned long LastPad;

extern "C" int output_channels;
extern "C" int output_samplesize;


unsigned char mixer_playbuf[ SOUNDSIZE ];


FILE *fp_xa2;

#include "xaudio_2.cpp"


/*
0 = DirectSound
1 = XAudio2 stereo
2 = XAudio2 5.1
3 = XAudio2 7.1
4 = OpenAL
*/


extern "C" void SetupSound(void)
{
	XAudio2_SetupSound();
}



extern "C" void RemoveSound(void)
{
	XAudio2_RemoveSound();
}



extern "C" int SoundBufferReady()
{

	return 1;
}



extern "C" int SoundGetBytesBuffered()
{
	XAudio2_SoundGetBytesBuffered();

	return 0;
}



extern "C" int SoundGetSamplesBuffered()
{
	return SoundGetBytesBuffered() / output_samplesize;
}



extern "C" void SoundFeedStreamData(unsigned char* pSound,long lBytes)
{
	XAudio2_SoundFeedStreamData( pSound, lBytes );
}



extern "C" void SoundPhantomPad()
{
}



extern "C" void SoundRecordStreamData(unsigned char* pSound,long lBytes)
{
}



extern "C" void ResetSound()
{
	// fast-forward lag?
	CDDAPlay  = CDDAStart;
	CDDAFeed  = CDDAStart;
	
	XAPlay  = XAStart;
	XAFeed  = XAStart;


	LastWrite = 0xffffffff;
}
