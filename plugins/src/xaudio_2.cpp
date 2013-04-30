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
#include <xaudio2.h>



IXAudio2 *lpXAudio2 = NULL;
IXAudio2MasteringVoice *lpMasterVoice = NULL;
IXAudio2SourceVoice *lpSourceVoice = NULL;


int xaudio2_valid = 0;
int coinit_valid = 0;


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
	
	

	if( coinit_valid == 1 ) {
		//CoUninitialize();
	}

	
	coinit_valid = 0;
	xaudio2_valid = 0;
	
	LastWrite = 0xffffffff;
}


#if 0
// Only works on Vista / Win 7
void XAudio2_GetSpeakerCount()
{
	IMMDeviceEnumerator *pEnumerator = NULL;
	IMMDevice *pDevice = NULL;
	IAudioClient *pAudioClient = NULL;
	WAVEFORMATEX *pwfx = NULL;

		
	// get # speakers
	if( FAILED( CoCreateInstance(
						  CLSID_MMDeviceEnumerator, NULL,
						  CLSCTX_ALL, IID_IMMDeviceEnumerator,
							(void**)&pEnumerator) ) ) {
		output_driver = 1;

		return;
	}


	if( FAILED( pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice) ) ) {
		output_driver = 1;

		pEnumerator->Release();

		return;
	}


	if( FAILED( pDevice->Activate( IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient) ) ) {
		output_driver = 1;

		pDevice->Release();
		pEnumerator->Release();

		return;
	}


	if( FAILED( pAudioClient->GetMixFormat(&pwfx) ) ) {
		output_driver = 1;

		CoTaskMemFree(pwfx);
		pAudioClient->Release();
		pDevice->Release();
		pEnumerator->Release();

		return;
	}
}
#endif


void XAudio2_SetupSound(void)
{
	int channels, blockalign;
	WAVEFORMATEX wfx;
	XAUDIO2_BUFFER xaudio2_buf;


	/*
	// required on Win32
	if( FAILED( CoInitialize( NULL ) )) {
		//MessageBox( NULL, "Unable to start XAudio2", "XAudio2", MB_OK );

		return;
	}
	*/
	coinit_valid = 1;


	if( FAILED(XAudio2Create( &lpXAudio2, 0 , XAUDIO2_DEFAULT_PROCESSOR ) ) ) {
		//MessageBox( NULL, "Unable to start XAudio2\r- Try updating DirectX drivers", "XAudio2", MB_OK );

		return;
	}
	

	// normal XAudio 2 stereo
	switch( iOutputDriver ) {
		case 1: channels = ( iDisStereo == 1 ) ? 1 : 2; break;
		case 2: channels = 6; break;
		case 3: channels = 8; break;
	}

	channels = 2;

	// 16-bit samples
	blockalign = channels * 2;


	output_channels = channels;
	output_samplesize = blockalign;


	if ( FAILED( lpXAudio2->CreateMasteringVoice( &lpMasterVoice, channels, 44100, 0, 0, NULL ) )) {
		//MessageBox( NULL, "Unable to start XAudio2", "XAudio2", MB_OK );

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
	if( FAILED(lpXAudio2->CreateSourceVoice( &lpSourceVoice, (WAVEFORMATEX*)&wfx,
		0, XAUDIO2_DEFAULT_FREQ_RATIO, NULL, NULL, NULL ) ) ) {
		//MessageBox( NULL, "Unable to start XAudio2", "XAudio2", MB_OK );

		XAudio2_RemoveSound();
		return;
	}


	LastPlayTotal = 0;
	LastPlay = 0;
	LastWrite = 0xffffffff;
	memset( mixer_playbuf, 0, sizeof(mixer_playbuf) );



	lpSourceVoice->FlushSourceBuffers();
	lpSourceVoice->Start( 0, 0 );



	// looping buffer - own cursor maintained
	memset( &xaudio2_buf, 0, sizeof(xaudio2_buf) );
	xaudio2_buf.AudioBytes = SOUNDSIZE;
	xaudio2_buf.pAudioData = (BYTE *) &mixer_playbuf;
	xaudio2_buf.LoopCount = XAUDIO2_LOOP_INFINITE;
	lpSourceVoice->SubmitSourceBuffer( &xaudio2_buf );



	xaudio2_valid = 1;
}


unsigned long XAudio2_SoundGetBytesBuffered(void)
{
	int size, diff;
	unsigned int cplay;
	XAUDIO2_VOICE_STATE state;


	if( xaudio2_valid == 0 ) return 0;


	// get current play positions
	lpSourceVoice->GetState( &state );

	// cut down on 64-bit slow math
	cplay = state.SamplesPlayed & 0xffffffff;
	cplay *= output_samplesize;



	// update cursor
	LastPlay += ( cplay - LastPlayTotal );
	if( LastPlay >= SOUNDSIZE ) LastPlay -= SOUNDSIZE;

	diff = cplay - LastPlayTotal;
	LastPlayTotal = cplay;



	// edge boundaries
	if( LastWrite == 0xffffffff ) return 0;


	if( LastPlay < LastWrite ) size = LastWrite - LastPlay;
	else size = ( SOUNDSIZE - LastPlay ) + LastWrite;


/*
	if( !fp_xa2 ) fp_xa2 = fopen( "log-xa2.txt", "w" );
	fprintf( fp_xa2, "%d %d %d %d %d %d %d\n",
		size,
		LastPlay, LastWrite,
		state.SamplesPlayed, LastPlayTotal,
		diff);
*/



	// play cursor too fast - feed underrun
	if( size > FULLMAX ) {
		// assume underflow
		//fprintf( fp_xa2, "Under\n" );
		size = 0;
	}


	return size;
}


void XAudio2_SoundFeedStreamData( unsigned char* pSound, long lBytes )
{
	int pad, post_pad;
	unsigned short *lpSS, *lpSD;
	unsigned short *lpSD_start, *lpSD_end;
	int dwBytes1, dst_flip;


	if( xaudio2_valid == 0 ) return;


	pad = 0;
	post_pad = SOUNDLEN( phantom_post_pad );


	if( XAudio2_SoundGetBytesBuffered() == 0 )
	{
		// insert latency
		pad = SOUNDLEN(LATENCY);
		lBytes += pad;


		// update cursor + writepad
		LastWrite = LastPlay + output_samplesize * 2;
		if( LastWrite >= SOUNDSIZE ) LastWrite -= SOUNDSIZE;
	}


	lpSS = (unsigned short *) pSound;
	lpSD = (unsigned short *) (mixer_playbuf + LastWrite);
	lpSD_start = (unsigned short *) mixer_playbuf;
	lpSD_end = (unsigned short *) (mixer_playbuf + SOUNDSIZE);


	dwBytes1 = SOUNDSIZE - LastWrite;
	dst_flip = 0;

	
	LastWrite += lBytes;
	if( LastWrite >= SOUNDSIZE ) {
		LastWrite -= SOUNDSIZE;
	}



	// sound restart - pad first
	if( pad > 0 )
	{
		if( dwBytes1 > 0 ) {
			int size;

			size = (pad <= dwBytes1) ? pad : dwBytes1;
			memset( lpSD, 0, size );


			dwBytes1 -= size;
			pad -= size;

			lpSD += (size / 2);
		}


		if( pad > 0 ) {
			int size;


			if( dst_flip == 0 ) {
				lpSD=(unsigned short *)lpSD_start;
				dst_flip = 1;
			}


			size = pad;
			memset( lpSD, 0, size );


			pad -= size;

			lpSD += (size / 2);
		}
	}


	// now copy data - real audio
	if( lBytes > 0 )
	{
		// phantom pad start
		LastPad = LastWrite;


		if( dwBytes1 > 0 ) {
			int size;

			size = (lBytes <= dwBytes1) ? lBytes : dwBytes1;
			memcpy( lpSD, lpSS, size );


			dwBytes1 -= size;

			lBytes -= size;
			lpSS += (size / 2);
			lpSD += (size / 2);
		}


		if( lBytes > 0 ) {
			int size;


			if( dst_flip == 0 ) {
				lpSD=(unsigned short *)lpSD_start;
				dst_flip = 1;
			}


			size = lBytes;
			memcpy( lpSD, lpSS, size );


			lBytes -= size;

			lpSS += (size / 2);
			lpSD += (size / 2);
		}
	}


	// post-padding
	if( post_pad > 0 )
	{
		if( dwBytes1 > 0 ) {
			int size;

			size = (post_pad <= dwBytes1) ? post_pad : dwBytes1;
			memset( lpSD, 0, size );


			dwBytes1 -= size;
			pad -= size;

			lpSD += (size / 2);
		}


		if( post_pad > 0 ) {
			int size;


			if( dst_flip == 0 ) {
				lpSD=(unsigned short *)lpSD_start;
				dst_flip = 1;
			}


			size = post_pad;
			memset( lpSD, 0, size );


			post_pad -= size;

			lpSD += (size / 2);
		}
	}


#if 0
	// transfer data
	while( lBytes > 0 ) {
		if( pad > 0 ) { *lpSD++ = 0; pad -= 2; }
		else if( lBytes > 0 ) { *lpSD++ = *lpSS++; lBytes -= 2; }
		else if( post_pad > 0 ) { *lpSD++ = 0; post_pad -= 2; }

		if( lpSD >= lpSD_end ) lpSD = lpSD_start;
	}
#endif
}
