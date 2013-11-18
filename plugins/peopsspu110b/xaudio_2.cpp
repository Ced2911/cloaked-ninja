/***************************************************************************
zn.c  -  description
--------------------
begin                : Wed April 23 2004
copyright            : (C) 2004 by Pete Bernert
email                : BlackDove@addcom.de
***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version. See also the license.txt file for *
*   additional informations.                                              *
*                                                                         *
***************************************************************************/



// NOTE:
// - XP seems to update XAudio2 every 10 ms (441 samples, 44100 rate)



#include "xaudio2.h"

#define BUFFER_SIZE		22050

static short			*pSndBuffer = NULL;
static int				iBufSize = 0;
static volatile int	iReadPos = 0, iWritePos = 0;

static unsigned int write_buffer = 0;

static unsigned char xaudio_buffer[ SOUNDSIZE ];

static IXAudio2 *lpXAudio2 = NULL;
static IXAudio2MasteringVoice *lpMasterVoice = NULL;
static IXAudio2SourceVoice *lpSourceVoice = NULL;

static int sample_len;
static int nbChannel = 1;

void SOUND_FillAudio();
static WAVEFORMATEX wfx;


struct StreamingVoiceContext : public IXAudio2VoiceCallback
{
    void OnVoiceProcessingPassStart( UINT32 BytesRequired ){}
    void OnVoiceProcessingPassEnd(){}
    void OnStreamEnd(){}
    void OnBufferStart( void* ){ SetEvent( hBufferEndEvent );}
    //void OnBufferEnd( void* ){ SetEvent( hBufferEndEvent ); }
	void OnBufferEnd( void* ){ SOUND_FillAudio(); }
    void OnLoopEnd( void* ){}
    void OnVoiceError( void*, HRESULT ){}

    HANDLE hBufferEndEvent;

    StreamingVoiceContext(): hBufferEndEvent( CreateEvent( NULL, FALSE, FALSE, NULL ) ){}
    virtual ~StreamingVoiceContext(){ CloseHandle( hBufferEndEvent ); }
};

static StreamingVoiceContext voiceContext;

void XAudio2_RemoveSound()
{
	if( lpSourceVoice ) {
		lpSourceVoice->Stop(0);
		
		lpSourceVoice->DestroyVoice();
		lpSourceVoice = NULL;
	}
	
	if( lpMasterVoice ) {
		lpMasterVoice->DestroyVoice();
		lpMasterVoice = NULL;
	}
	
	if( lpXAudio2 ) {
		lpXAudio2->Release();
		lpXAudio2 = NULL;
	}
	
	
	LastWrite = 0xffffffff;
}


void XAudio2_SetupSound(void)
{
	int channels, blockalign;

	if( FAILED(XAudio2Create( &lpXAudio2, 0 , XAUDIO2_DEFAULT_PROCESSOR ) ) ) {
		DebugBreak();
		return;
	}	

	/*
	// normal XAudio 2 stereo
	switch( nbChannel ) {
		case 1: channels = ( iDisStereo == 1 ) ? 1 : 2; break;
		case 2: channels = 6; break;
		case 3: channels = 8; break;
	}
	*/
	channels = 2;


	// 16-bit samples
	blockalign = channels * 2;

	output_channels = channels;
	output_samplesize = blockalign;


	if ( FAILED( lpXAudio2->CreateMasteringVoice( &lpMasterVoice, channels, 44100, 0, 0, NULL ) )) {
		DebugBreak();

		XAudio2_RemoveSound();
		return;
	}	
	
	memset(&wfx, 0, sizeof(WAVEFORMATEX));
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nSamplesPerSec = 44100;
	wfx.nChannels = channels;
	wfx.wBitsPerSample = 16;
	wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	wfx.cbSize = 0;	
	
	// wave streaming
	/*
	if( FAILED(lpXAudio2->CreateSourceVoice( &lpSourceVoice, (WAVEFORMATEX*)&wfx,
		XAUDIO2_VOICE_NOSRC, XAUDIO2_DEFAULT_FREQ_RATIO, NULL, NULL, NULL ) ) ) {
	*/
	if( FAILED(lpXAudio2->CreateSourceVoice( &lpSourceVoice, (WAVEFORMATEX*)&wfx,
                0, XAUDIO2_DEFAULT_FREQ_RATIO, &voiceContext, NULL, NULL ) ) ) {
		DebugBreak();

		XAudio2_RemoveSound();
		return;
	}

	memset( xaudio_buffer, 0, sizeof(xaudio_buffer) );	

	lpSourceVoice->FlushSourceBuffers();
	lpSourceVoice->Start( 0, 0 );

	iBufSize = BUFFER_SIZE;
	//if (iDisStereo) iBufSize /= 2;

	if (pSndBuffer == NULL) {
		pSndBuffer = (short *)malloc(iBufSize * sizeof(short));
	}

	// submit fake buffer for starting
	XAUDIO2_BUFFER buf = {0};
	buf.AudioBytes = 2048;
	buf.pAudioData = xaudio_buffer;

	lpSourceVoice->SubmitSourceBuffer( &buf );


	LastPlayTotal = 0;
	LastPlay = 0;
	LastWrite = 0xffffffff;
}


static void SOUND_FillAudio() {
	int len = SOUNDSIZE;
	int byte_size = len = 2048;
	short *p = (short *)xaudio_buffer;

	XAUDIO2_BUFFER buf = {0};

	len /= sizeof(short);

	while (iReadPos != iWritePos && len > 0) {
		*p++ = pSndBuffer[iReadPos++];
		if (iReadPos >= iBufSize) iReadPos = 0;
		--len;
	}

	// Fill remaining space with zero
	while (len > 0) {
		*p++ = 0;
		--len;
	}

	// submit to xaudio
	buf.AudioBytes = byte_size;
	buf.pAudioData = xaudio_buffer;

	lpSourceVoice->SubmitSourceBuffer( &buf );
}

unsigned long XAudio2_SoundGetBytesBuffered(void)
{
	int size;

	if (pSndBuffer == NULL) return SOUNDSIZE;

	size = iReadPos - iWritePos;
	if (size <= 0) size += iBufSize;

	if (size < iBufSize / 2) return SOUNDSIZE;

	return 0;



	// update cursor
	LastPlay = iWritePos;
	LastWrite = iReadPos;

	return size;
}


void XAudio2_SoundFeedStreamData( unsigned char* pSound, long lBytes )
{
	short *p = (short *)pSound;

	if (pSndBuffer == NULL) return;

	while (lBytes > 0) {
		if (((iWritePos + 1) % iBufSize) == iReadPos) break;

		pSndBuffer[iWritePos] = *p++;

		++iWritePos;
		if (iWritePos >= iBufSize) iWritePos = 0;

		lBytes -= sizeof(short);
	}

}
