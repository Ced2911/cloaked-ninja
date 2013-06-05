#include <xtl.h>
#include <xaudio2.h>
#include <algorithm>
#include "stdafx.h"
#define _IN_OSS
#include "externals.h"

#define STREAMING_BUFFER_SIZE 65536
#define MAX_BUFFER_COUNT 3

unsigned int write_buffer = 0;
unsigned char mixer_playbuf[MAX_BUFFER_COUNT][ SOUNDSIZE ];

static IXAudio2 *lpXAudio2 = NULL;
static IXAudio2MasteringVoice *lpMasterVoice = NULL;
static IXAudio2SourceVoice *lpSourceVoice = NULL;

int nbChannel = 1;
int output_channels = 2;
int output_samplesize = 4;


WAVEFORMATEX wfx;

static volatile unsigned int stream_running = 0;

struct StreamingVoiceContext : public IXAudio2VoiceCallback
{
    void OnVoiceProcessingPassStart( UINT32 BytesRequired ){}
    void OnVoiceProcessingPassEnd(){}
    void OnStreamEnd(){}
    void OnBufferStart( void* ){ stream_running = 1; SetEvent( hBufferEndEvent );}
    //void OnBufferEnd( void* ){ SetEvent( hBufferEndEvent ); }
	void OnBufferEnd( void* ){ stream_running = 0; }
    void OnLoopEnd( void* ){}
    void OnVoiceError( void*, HRESULT ){}

    HANDLE hBufferEndEvent;

    StreamingVoiceContext(): hBufferEndEvent( CreateEvent( NULL, FALSE, FALSE, NULL ) ){}
    virtual ~StreamingVoiceContext(){ CloseHandle( hBufferEndEvent ); }
};

static StreamingVoiceContext voiceContext;

extern "C" void RemoveSound()
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
}

extern "C" void SetupSound(void)
{
	int channels, blockalign;
	XAUDIO2_BUFFER xaudio2_buf;

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

		RemoveSound();
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

		RemoveSound();
		return;
	}

	memset( mixer_playbuf, 0, sizeof(mixer_playbuf) );	

	lpSourceVoice->FlushSourceBuffers();
	lpSourceVoice->Start( 0, 0 );
}

extern "C" unsigned long SoundGetBytesBuffered(void)
{
	int size;
	int sample_played;

	size = 0;

	XAUDIO2_VOICE_STATE state;
	lpSourceVoice->GetState( &state );
	sample_played =  state.SamplesPlayed;

	size = sample_played * (wfx.wBitsPerSample/8);

	//return size;

	return 0;
}

extern "C" void SoundFeedStreamData( unsigned char* pSound, long lBytes )
{	
#ifdef _DEBUG
	return;
#endif
	if(lBytes<0) {
		return;
	}

	XAUDIO2_VOICE_STATE state;
	int size = lBytes;
	unsigned char* pData = pSound;
	unsigned int start = 0;
	unsigned int chunck_size = 0;
	while(size > 0) {
		for(;;)
		{
			
			lpSourceVoice->GetState( &state, XAUDIO2_VOICE_NOSAMPLESPLAYED );
			
			if ( state.BuffersQueued < MAX_BUFFER_COUNT - 1 )
				break;
			// Audio sync !!
			WaitForSingleObject( voiceContext.hBufferEndEvent, INFINITE );
		}
		/*
		while(stream_running) {
			// wait 
		}
		*/

		//WaitForSingleObject( voiceContext.hBufferEndEvent, INFINITE );
		chunck_size = min(SOUNDSIZE, size);

		memcpy(mixer_playbuf[write_buffer], pData + start, chunck_size);

		XAUDIO2_BUFFER buf = {0};
		buf.AudioBytes = chunck_size;
		buf.pAudioData = mixer_playbuf[write_buffer];

		lpSourceVoice->SubmitSourceBuffer( &buf );

		size -= SOUNDSIZE;
		start += SOUNDSIZE;

		write_buffer = (write_buffer + 1) & (MAX_BUFFER_COUNT - 1);
	}
}


