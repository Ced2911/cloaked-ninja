/***************************************************************************
openal.c  -  description
-------------------
begin                : Wed May 15 2002
copyright            : (C) 2002 by Pete Bernert
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

#include <al.h>
#include <alc.h>


//#define DEBUG_AL


// NOTE: Unfinished
// - 4096 min buffer uploads
// - suggestion: see Dolphin OpenAL version



int openal_valid = 0;


#define NUM_AL_BUFFERS 2


ALCdevice *openal_device;
ALCcontext *al_context;

ALuint al_source, al_buffers[ NUM_AL_BUFFERS ];
ALenum al_format;
ALsizei al_frequency;


int OpenAL_CheckError()
{
	if( alGetError() != AL_NO_ERROR ) {
		MessageBox( NULL, "Unable to start OpenAL", "OpenAL", MB_OK );

		return 1;
	}

	return 0;
}


void OpenAL_RemoveSound()
{
	if( al_context ) {
		alSourceStop( al_source );
		
		alDeleteBuffers( NUM_AL_BUFFERS, al_buffers );
		alDeleteSources( 1, &al_source );
				
		alcMakeContextCurrent( NULL );
		alcDestroyContext( al_context );

		al_context = 0;
	}


	if( openal_device ) {
		alcCloseDevice( openal_device );

		openal_device = 0;
	}


	openal_valid = 0;
	
	LastWrite = 0xffffffff;
}


void OpenAL_SetupSound(void)
{
	int channels, blockalign;


#ifdef DEBUG_AL
	fp_xa2 = fopen( "log-al.txt","w" );
#endif


	openal_device = alcOpenDevice(NULL);
	if( openal_device == 0 ) {
		MessageBox( NULL, "Unable to start OpenAL", "OpenAL", MB_OK );

		return;
	}


	al_context = alcCreateContext( openal_device, NULL );
	alcMakeContextCurrent( al_context );
	if( al_context == 0 ) {
		MessageBox( NULL, "Unable to start OpenAL", "OpenAL", MB_OK );

		return;
	}

	
	// 16-bit samples
	channels = ( iDisStereo == 1 ) ? 1 : 2;
	blockalign = channels * 2;


	// default settings
	al_format = ( channels == 1 ) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
	al_frequency = 44100;


	output_channels = channels;
	output_samplesize = blockalign;



	LastPlayTotal = 0;
	LastPlay = 0;
	LastWrite = 0xffffffff;
	memset( mixer_playbuf, 0, sizeof(mixer_playbuf) );


	alGenSources( 1, &al_source );
	alGenBuffers( NUM_AL_BUFFERS, al_buffers );


	if( OpenAL_CheckError() ) return;


	openal_valid = 1;
}


int OpenAL_SoundBufferReady()
{
	ALint val;


	if( openal_valid == 0 ) return 1;


	// uninit
	if( LastWrite == 0xffffffff ) return NUM_AL_BUFFERS;


	// check # buffers drained (0-2)
	alGetSourcei( al_source, AL_BUFFERS_PROCESSED, &val );

	LastWrite = val;


#ifdef DEBUG_AL
	fprintf( fp_xa2, "Buffer rdy = %d\n", val );
#endif


	// no buffers ready
	if( LastWrite == 0 ) return 0;


	// ready to upload data
	return 1;
}


unsigned long OpenAL_SoundGetBytesBuffered(void)
{
	int size;
	unsigned int cplay;
	ALint alplay;


	if( openal_valid == 0 ) return 0;

	return 0;


	// get current play positions
	alGetSourcei( al_source, AL_BYTE_OFFSET, &alplay );
	cplay = alplay;
	LastPlay = cplay;


	if( cplay < LastWrite ) size = LastWrite - cplay;
	else size = ( SOUNDSIZE - cplay ) + LastWrite;


	// play cursor too fast - feed underrun
	if( size < TESTMIN ) {
		size = 0;
	}
	if( size > FULLMAX) {
		size = 0;
	}


	return size;
}


void OpenAL_SoundFeedStreamData( unsigned char* pSound, long lBytes )
{
	int pad;
	ALuint buffer;


	if( openal_valid == 0 ) return;


	pad = 0;


	//alGetSourcei( al_source, AL_SOURCE_STATE, &val );


	// full - skip
	//if( LastWrite == 0 ) return;
	if( OpenAL_SoundBufferReady() == 0 ) return;


	// empty buffer
	if( LastWrite == 0xffffffff ||
			LastWrite == NUM_AL_BUFFERS )
	//if( val != AL_PLAYING )
	//if( LastWrite==0xffffffff ||
		//	OpenAL_SoundGetBytesBuffered() == 0 )
	{
#ifdef DEBUG_AL
		fprintf( fp_xa2, "Empty refill\n" );
#endif


		// insert latency - slower buffer stretching
		pad = SOUNDLEN(LATENCY);


		//MessageBox(0,0,0,0);

		// rewind, copy buffer, play again
		// - OpenAL: reuse mixing buffer (always empty)
		alSourceStop( al_source );

		alBufferData( al_buffers[0], al_format, mixer_playbuf, pad, al_frequency );
		alBufferData( al_buffers[1], al_format, pSound, pad, al_frequency );

		alSourceQueueBuffers( al_source, NUM_AL_BUFFERS, al_buffers );
		alSourcePlay( al_source );


		// # buffers playing
		LastPlay = NUM_AL_BUFFERS;
		LastWrite = 0;
	}
	else {
#ifdef DEBUG_AL
		fprintf( fp_xa2, "Fill buffer = %d\n", LastWrite );
#endif


		//MessageBox(0,"Err",0,0);

		// put new buffer in (memcpy)
		alSourceUnqueueBuffers( al_source, 1, &buffer );
		alBufferData( buffer, al_format, pSound, lBytes, al_frequency );
		alSourceQueueBuffers( al_source, 1, &buffer );

		LastWrite--;
	}
}
