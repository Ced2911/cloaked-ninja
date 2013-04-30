#include "stdafx.h"
#include "record.h"
#include "externals.h"
#include <stdio.h>
#include <sys/time.h> 

unsigned long LastWrite = 0xffffffff;
unsigned long LastPlay = 0;
unsigned int LastPlayTotal;

unsigned long LastPad;


int output_channels = 2;
int output_samplesize = 4;


unsigned char mixer_playbuf[ SOUNDSIZE ];


FILE *fp_xa2;


#include "dsound.cpp"
#include "xaudio_2.cpp"
#include "openal.cpp"


unsigned long timeGetTime()
{
	return __mftb();
}

/*
0 = DirectSound
1 = XAudio2 stereo
2 = XAudio2 5.1
3 = XAudio2 7.1
4 = OpenAL
*/


void SetupSound(void)
{
	XAudio2_SetupSound();
}


void RemoveSound(void)
{
	XAudio2_RemoveSound();
}


int SoundBufferReady()
{
	return 1;
}


int SoundGetBytesBuffered()
{
	 XAudio2_SoundGetBytesBuffered();

	return 0;
}


int SoundGetSamplesBuffered()
{
	return SoundGetBytesBuffered() / output_samplesize;
}


void SoundFeedStreamData(unsigned char* pSound,long lBytes)
{
	XAudio2_SoundFeedStreamData( pSound, lBytes );
}


void SoundPhantomPad()
{
	
}


void SoundRecordStreamData(unsigned char* pSound,long lBytes)
{
}


void ResetSound()
{
	// fast-forward lag?
	CDDAPlay  = CDDAStart;
	CDDAFeed  = CDDAStart;
	
	XAPlay  = XAStart;
	XAFeed  = XAStart;


	LastWrite = 0xffffffff;
}
