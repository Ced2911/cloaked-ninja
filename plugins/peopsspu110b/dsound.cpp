/***************************************************************************
dsound.c  -  description
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

//*************************************************************************//
// History of changes:
//
// 2003/01/12 - Pete
// - added recording funcs
//
// 2002/05/15 - Pete
// - generic cleanup for the Peops release
//
//*************************************************************************//

#include "stdafx.h"

#define _IN_DSOUND

#ifdef _WINDOWS

#include <dsound.h>

////////////////////////////////////////////////////////////////////////
// dsound globals
////////////////////////////////////////////////////////////////////////

LPDIRECTSOUND lpDS;
LPDIRECTSOUNDBUFFER lpDSBP = NULL;
LPDIRECTSOUNDBUFFER lpDSB = NULL;
DSBUFFERDESC        dsbd;
DSBUFFERDESC        dsbdesc;
DSCAPS              dscaps;
DSBCAPS             dsbcaps;


int dsound_valid = 0;
unsigned long LastPad;

////////////////////////////////////////////////////////////////////////
// SETUP SOUND
////////////////////////////////////////////////////////////////////////

void DSound_SetupSound(void)
{
	HRESULT dsval;WAVEFORMATEX pcmwf;
	
	dsval = DirectSoundCreate(NULL,&lpDS,NULL);
	if(dsval!=DS_OK) 
  {
		MessageBox(hWMain,"DirectSoundCreate!","Error",MB_OK);
		return;
  }
	
	if(DS_OK!=IDirectSound_SetCooperativeLevel(lpDS,hWMain, DSSCL_PRIORITY))
  {
		if(DS_OK!=IDirectSound_SetCooperativeLevel(lpDS,hWMain, DSSCL_NORMAL))
    {
			MessageBox(hWMain,"SetCooperativeLevel!","Error",MB_OK);
			return;
    }
  }
	
	memset(&dsbd,0,sizeof(DSBUFFERDESC));
	dsbd.dwSize = 20;                                     // NT4 hack! sizeof(dsbd);
	dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER;                 
	dsbd.dwBufferBytes = 0; 
	dsbd.lpwfxFormat = NULL;
	
	dsval=IDirectSound_CreateSoundBuffer(lpDS,&dsbd,&lpDSBP,NULL);
	if(dsval!=DS_OK) 
  {
		MessageBox(hWMain, "CreateSoundBuffer (Primary)", "Error",MB_OK);
		return;
  }
	
	memset(&pcmwf, 0, sizeof(WAVEFORMATEX));
	pcmwf.wFormatTag = WAVE_FORMAT_PCM;
	
	
	
	if(iDisStereo) {pcmwf.nChannels = 1; pcmwf.nBlockAlign = 2;}
	else           {pcmwf.nChannels = 2; pcmwf.nBlockAlign = 4;}
	
	output_channels = pcmwf.nChannels;
	output_samplesize = pcmwf.nBlockAlign;


	pcmwf.nSamplesPerSec = 44100;
	
	pcmwf.nAvgBytesPerSec = pcmwf.nSamplesPerSec * pcmwf.nBlockAlign;
	pcmwf.wBitsPerSample = 16;
	
	dsval=IDirectSoundBuffer_SetFormat(lpDSBP,&pcmwf);
	if(dsval!=DS_OK) 
  {
		MessageBox(hWMain, "SetFormat!", "Error",MB_OK);
		return;
  }
	
	dscaps.dwSize = sizeof(DSCAPS);
	dsbcaps.dwSize = sizeof(DSBCAPS);
	IDirectSound_GetCaps(lpDS,&dscaps);
	IDirectSoundBuffer_GetCaps(lpDSBP,&dsbcaps);
	
	memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
	dsbdesc.dwSize = 20;                                  // NT4 hack! sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_LOCSOFTWARE | DSBCAPS_STICKYFOCUS | DSBCAPS_GETCURRENTPOSITION2;
	dsbdesc.dwBufferBytes = SOUNDSIZE;
	dsbdesc.lpwfxFormat = (LPWAVEFORMATEX)&pcmwf;
	
	dsval=IDirectSound_CreateSoundBuffer(lpDS,&dsbdesc,&lpDSB,NULL);
	if(dsval!=DS_OK) 
  {
		MessageBox(hWMain,"CreateSoundBuffer (Secondary)", "Error",MB_OK);
		return;
  }
	
	dsval=IDirectSoundBuffer_Play(lpDSBP,0,0,DSBPLAY_LOOPING);
	if(dsval!=DS_OK) 
  {
		MessageBox(hWMain,"Play (Primary)","Error",MB_OK);
		return;
  }
	
	dsval=IDirectSoundBuffer_Play(lpDSB,0,0,DSBPLAY_LOOPING);
	if(dsval!=DS_OK) 
  {
		MessageBox(hWMain,"Play (Secondary)","Error",MB_OK);
		return;
  }
	
	
	
	dsound_valid = 1;
}

////////////////////////////////////////////////////////////////////////
// REMOVE SOUND
////////////////////////////////////////////////////////////////////////

void DSound_RemoveSound(void)
{ 
	int iRes;
	
	if(lpDSB!=NULL) 
  {
		IDirectSoundBuffer_Stop(lpDSB);
		iRes=IDirectSoundBuffer_Release(lpDSB);
		// FF says such a loop is bad... Demo says it's good... Pete doesn't care
		while(iRes!=0) iRes=IDirectSoundBuffer_Release(lpDSB);
		lpDSB=NULL;
  }                            
	
	if(lpDSBP!=NULL) 
  {
		IDirectSoundBuffer_Stop(lpDSBP);
		iRes=IDirectSoundBuffer_Release(lpDSBP);
		// FF says such a loop is bad... Demo says it's good... Pete doesn't care
		while(iRes!=0) iRes=IDirectSoundBuffer_Release(lpDSBP);
		lpDSBP=NULL;
  }
	
	if(lpDS!=NULL) 
  {
		iRes=IDirectSound_Release(lpDS);
		// FF says such a loop is bad... Demo says it's good... Pete doesn't care
		while(iRes!=0) iRes=IDirectSound_Release(lpDS);
		lpDS=NULL;
  }
	
	
	LastWrite=0xffffffff;
	LastPad = 0xffffffff;
}

////////////////////////////////////////////////////////////////////////
// GET BYTES BUFFERED
////////////////////////////////////////////////////////////////////////

unsigned long DSound_SoundGetBytesBuffered(void)
{
	unsigned long cplay,cwrite;
	int size;
	
	
	if( dsound_valid == 0 ) return 0;
	
	
	
	if(LastWrite==0xffffffff) return 0;
	
	IDirectSoundBuffer_GetCurrentPosition(lpDSB,&cplay,&cwrite);
	


	if(cplay>SOUNDSIZE) return SOUNDSIZE;
	
	if(cplay <= LastWrite) size = LastWrite-cplay;
	else size = (SOUNDSIZE-cplay)+LastWrite;
	


	// play cursor too fast - feed underrun
	if( size < TESTMIN) {
		size = 0;
	}
	if( size > FULLMAX) {
		size = 0;
	}
	

	// force sound restart + phantom pad reset
	if( size == 0 ) {
		LastWrite = 0xffffffff;
	}


	return size;
}

////////////////////////////////////////////////////////////////////////
// FEED SOUND DATA
////////////////////////////////////////////////////////////////////////

void DSound_SoundPhantomPad()
{
	LPVOID lpvPtr1, lpvPtr2;
	unsigned long dwBytes1,dwBytes2; 
	unsigned short *lpSS, *lpSD;
	unsigned long dw,cplay,cwrite;
	HRESULT hr;
	unsigned long status;
	int pad, post_pad;
	int dst_flip;
	int lBytes;
	
	
	if( dsound_valid == 0 ) return;
	
	
	if( phantom_padder == 0 ) return;


	IDirectSoundBuffer_GetStatus(lpDSB,&status);
	if(status&DSBSTATUS_BUFFERLOST)
  {
		if(IDirectSoundBuffer_Restore(lpDSB)!=DS_OK) return;
		IDirectSoundBuffer_Play(lpDSB,0,0,DSBPLAY_LOOPING);
  }

	IDirectSoundBuffer_GetCurrentPosition(lpDSB,&cplay,&cwrite);


	// every 1ms timer - at least 15 works, be safe though
	lBytes = SOUNDLEN( phantom_pad_size );



	if( debug_sound_buffer ) {
		extern FILE *fp_spu_log;

		if( fp_spu_log == 0 ) {
			fp_spu_log = fopen( "logg.txt", "w" );
		}

		fprintf( fp_spu_log, "Phantom pad\n" );
	}

			
			
	// restart pad - no sound left
	if( LastPad==0xffffffff ||
		( DSound_SoundGetBytesBuffered()==0))
	{
		if( debug_sound_buffer ) {
			extern FILE *fp_spu_log;

			if( fp_spu_log == 0 ) {
				fp_spu_log = fopen( "logg.txt", "w" );
			}

			fprintf( fp_spu_log, "Phantom restart\n" );
		}

			
		LastPad = cwrite;
	}



	// safety check
	{
		int size;

		if( LastPad >= cplay ) size = LastPad - cplay;
		else size = (SOUNDSIZE - cplay) + LastPad;


		// stay max 1sec ahead
		if( size >= SOUNDLEN(1000) ) return;
	}



	hr=IDirectSoundBuffer_Lock(lpDSB,LastPad, SOUNDLEN( 5000 ),
		&lpvPtr1, &dwBytes1, 
		&lpvPtr2, &dwBytes2,
		0);
	
	
	if(hr!=DS_OK) {
		if( debug_sound_buffer ) {
			extern FILE *fp_spu_log;

			if( fp_spu_log == 0 ) {
				fp_spu_log = fopen( "logg.txt", "w" );
			}

			fprintf( fp_spu_log, "Phantom lock fail\n" );
		}

	

		LastWrite=0xffffffff;
		LastPad=0xffffffff;
		return;
	}


	// dummy data
	LastPad += lBytes;
	if(LastPad>=SOUNDSIZE) LastPad-=SOUNDSIZE;

		

	if( dwBytes1 > 0 ) {
		int size;


		lpSD=(unsigned short *)lpvPtr1;


		size = (lBytes <= dwBytes1) ? lBytes : dwBytes1;
		memset( lpSD, 0, size );


		lBytes -= size;
	}


	if( lBytes > 0 && lpvPtr2 ) {
		int size;


		lpSD=(unsigned short *)lpvPtr2;


		size = (lBytes <= dwBytes2) ? lBytes : dwBytes2;
		memset( lpSD, 0, size );
	}

	
	IDirectSoundBuffer_Unlock(lpDSB,lpvPtr1,dwBytes1,lpvPtr2,dwBytes2);
}


void DSound_SoundFeedStreamData(unsigned char* pSound,long lBytes)
{
	LPVOID lpvPtr1, lpvPtr2;
	unsigned long dwBytes1,dwBytes2; 
	unsigned short *lpSS, *lpSD;
	unsigned long dw,cplay,cwrite;
	HRESULT hr;
	unsigned long status;
	int pad, post_pad;
	int dst_flip;
	
	
	if( dsound_valid == 0 ) return;
	
	
	
	
	IDirectSoundBuffer_GetStatus(lpDSB,&status);
	if(status&DSBSTATUS_BUFFERLOST)
  {
		if(IDirectSoundBuffer_Restore(lpDSB)!=DS_OK) return;
		IDirectSoundBuffer_Play(lpDSB,0,0,DSBPLAY_LOOPING);
  }
	
	IDirectSoundBuffer_GetCurrentPosition(lpDSB,&cplay,&cwrite);


	// Normal audio stream
	if( debug_sound_buffer ) {
		extern FILE *fp_spu_log;

		if( fp_spu_log == 0 ) {
			fp_spu_log = fopen( "logg.txt", "w" );
		}

		fprintf( fp_spu_log, "Stream data = %d\n", lBytes );
	}

			
		
	pad = 0;
	post_pad = phantom_post_pad;
	if(LastWrite==0xffffffff ||
		(DSound_SoundGetBytesBuffered()==0))
	{
		if( debug_sound_buffer ) {
			extern FILE *fp_spu_log;

			if( fp_spu_log == 0 ) {
				fp_spu_log = fopen( "logg.txt", "w" );
			}

			fprintf( fp_spu_log, "Stream reset\n" );
		}

			
			
		LastWrite=cwrite;

		// insert latency - slower buffer stretching
		pad = SOUNDLEN(LATENCY) + SOUNDLEN( latency_restart );
	}

		

	hr=IDirectSoundBuffer_Lock(lpDSB,LastWrite, SOUNDLEN( 5000 ),
		&lpvPtr1, &dwBytes1, 
		&lpvPtr2, &dwBytes2,
		0);
		
	if(hr!=DS_OK) {
		if( debug_sound_buffer ) {
			extern FILE *fp_spu_log;

			if( fp_spu_log == 0 ) {
				fp_spu_log = fopen( "logg.txt", "w" );
			}

			fprintf( fp_spu_log, "Stream fail\n" );
		}

			
		LastWrite=0xffffffff;
		LastPad=0xffffffff;
		return;
	}


	LastWrite += pad;
	LastWrite += lBytes;

	if(LastWrite>=SOUNDSIZE) LastWrite-=SOUNDSIZE;

	LastPlay=cplay;
		


	lpSD=(unsigned short *)lpvPtr1;
	dw=dwBytes1>>1;
	dst_flip = 0;

	lpSS=(unsigned short *)pSound;



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


		if( pad > 0 && lpvPtr2 ) {
			int size;


			if( dst_flip == 0 ) {
				lpSD=(unsigned short *)lpvPtr2;
				dst_flip = 1;
			}


			size = (pad <= dwBytes2) ? pad : dwBytes2;
			memset( lpSD, 0, size );


			dwBytes2 -= size;
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


		if( lBytes > 0 && lpvPtr2 ) {
			int size;


			if( dst_flip == 0 ) {
				lpSD=(unsigned short *)lpvPtr2;
				dst_flip = 1;
			}


			size = (lBytes <= dwBytes2) ? lBytes : dwBytes2;
			memcpy( lpSD, lpSS, size );


			dwBytes2 -= size;
			lBytes -= size;

			lpSS += (size / 2);
			lpSD += (size / 2);
		}
	}


	if( post_pad > 0 )
	{
		if( dwBytes1 > 0 ) {
			int size;

			size = (post_pad <= dwBytes1) ? post_pad : dwBytes1;
			memset( lpSD, 0, size );


			dwBytes1 -= size;
			post_pad -= size;

			lpSD += (size / 2);
		}


		if( post_pad > 0 && lpvPtr2 ) {
			int size;


			if( dst_flip == 0 ) {
				lpSD=(unsigned short *)lpvPtr2;
				dst_flip = 1;
			}


			size = (post_pad <= dwBytes2) ? post_pad : dwBytes2;
			memset( lpSD, 0, size );


			dwBytes2 -= size;
			post_pad -= size;

			lpSD += (size / 2);
		}
	}



#if 0
	while(dw) {
		if( pad > 0 ) { *lpSD++=0; pad -= 2; }
		else if( lBytes > 0 ) { *lpSD++=*lpSS++; lBytes -= 2; }
		else if( post_pad > 0 ) { *lpSD++=0; post_pad -= 2; }
		dw--;
	}
	
	if(lpvPtr2)
  {
		lpSD=(unsigned short *)lpvPtr2;
		dw=dwBytes2>>1;
		while(dw) {
			if( pad > 0 ) { *lpSD++=0; pad -= 2; }
			else if( lBytes > 0 ) { *lpSD++=*lpSS++; lBytes -= 2; }
			else if( post_pad > 0 ) { *lpSD++=0; post_pad -= 2; }
			dw--;
		}
  }
#endif


	
	IDirectSoundBuffer_Unlock(lpDSB,lpvPtr1,dwBytes1,lpvPtr2,dwBytes2);
}


#endif



