/***************************************************************************
cfg.c  -  description
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
// 2003/06/07 - Pete
// - added Linux NOTHREADLIB define
//
// 2003/02/28 - Pete
// - added option for kode54's interpolation and linuzappz's mono mode
//
// 2003/01/19 - Pete
// - added Neill's reverb
//
// 2002/08/04 - Pete
// - small linux bug fix: now the cfg file can be in the main emu directory as well
//
// 2002/06/08 - linuzappz
// - Added combo str for SPUasync, and MAXMODE is now defined as 2
//
// 2002/05/15 - Pete
// - generic cleanup for the Peops release
//
//*************************************************************************//

#include "stdafx.h"

#define _IN_CFG

#include "externals.h"

extern int iZincEmu;


////////////////////////////////////////////////////////////////////////
// WINDOWS CONFIG/ABOUT HANDLING
////////////////////////////////////////////////////////////////////////

#if defined(_WINDOWS) || defined(_XBOX)

#include "resource.h"

#include <stdio.h>

////////////////////////////////////////////////////////////////////////
// simple about dlg handler
////////////////////////////////////////////////////////////////////////

BOOL CALLBACK AboutDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifndef _XBOX
	switch(uMsg)
  {
	case WM_COMMAND:
    {
			switch(LOWORD(wParam))
      {
				case IDOK:
					EndDialog(hW,TRUE);
					return TRUE;
			}
    }
  }
	return FALSE;
#else
	return true;
#endif
}

////////////////////////////////////////////////////////////////////////
// READ CONFIG: from win registry
////////////////////////////////////////////////////////////////////////

#define INI_VERSION "110"
#define INI_VERSION_EXPERT "110_2"


void ReadConfig(void)
{
	FILE *fp;
	int reset;
	char ini_version[256];
	

	reset = 1;


	fp = fopen( "plugins\\spuPeopsSound.ini", "r" );
	if( !fp )	fp = fopen( "spuPeopsSound.ini", "r" );


	if( fp ) {
		fscanf( fp, "Version = %s\n", &ini_version );
		
		if( strcmp( ini_version, INI_VERSION ) == 0 ) {
			fscanf( fp, "SoundDriver = %d\n", &iOutputDriver );
			fscanf( fp, "Timer = %d\n", &iUseTimer );
			fscanf( fp, "Volume = %d\n", &iVolume );
			fscanf( fp, "Interpolation = %d\n", &iUseInterpolation );
			fscanf( fp, "Reverb = %d\n", &iUseReverb );
			fscanf( fp, "Mono = %d\n", &iDisStereo );
			fscanf( fp, "UseXA = %d\n", &iUseXA );
			fscanf( fp, "XAPitch = %d\n", &iXAPitch );
			fscanf( fp, "IRQWait = %d\n", &iSPUIRQWait );
			fscanf( fp, "Debugger = %d\n", &iDebugMode );
			fscanf( fp, "Recorder = %d\n", &iRecordMode );
			fscanf( fp, "EmuType = %d\n", &iEmuType );
			fscanf( fp, "ReverbBoost = %d\n", &iReverbBoost );

			fscanf( fp, "Latency = %d\n", &iLatency );
			fscanf( fp, "XAFilter = %d\n", &iXAInterp );
			fscanf( fp, "CDDAFilter = %d\n", &iCDDAInterp );
			fscanf( fp, "Output1Filter = %d\n", &iOutputInterp1 );
			fscanf( fp, "Output2Filter = %d\n", &iOutputInterp2 );
			fscanf( fp, "XAVol = %d\n", &iVolXA );
			fscanf( fp, "CDDAVol = %d\n", &iVolCDDA );
			fscanf( fp, "VoiceVol = %d\n", &iVolVoices );
			fscanf( fp, "XAStrength = %d\n", &iXAStrength );
			fscanf( fp, "CDDAStrength = %d\n", &iCDDAStrength );
			fscanf( fp, "Output2Strength = %d\n", &iOutput2Strength );


			reset = 0;
		}

		fclose(fp);
	}

	
	if( reset ) {
		// Defaults
		iUseXA=1;
		iVolume=10;
		iXAPitch=0;
		// iUseTimer=3; // XBOX
		iUseTimer = 0;

		iSPUIRQWait=0;
		iDebugMode=0;
		iRecordMode=0;
		iUseReverb=2;
		iUseInterpolation=2;
		iDisStereo=0;
		iEmuType=0;
		iXAStrength=0;
		iCDDAStrength=0;
		iOutput2Strength=0;
		iReleaseIrq = 0;
		iLatency = 8;
		
		iXAInterp = 2;
		iCDDAInterp = 1;
		iOutputInterp1 = 0;
		iOutputInterp2 = 0;
		iVolCDDA = 10;
		iVolXA = 10;
		iVolVoices = 10;
	

		fp = fopen( "plugins\\spuPeopsSound.ini", "w" );
		if( !fp ) fp = fopen( "spuPeopsSound.ini", "w" );


		if( fp ) {
			fprintf( fp, "Version = %s\n", INI_VERSION );
			fprintf( fp, "SoundDriver = %d\n", iOutputDriver );
			fprintf( fp, "Timer = %d\n", iUseTimer );
			fprintf( fp, "Volume = %d\n", iVolume );
			fprintf( fp, "Interpolation = %d\n", iUseInterpolation );
			fprintf( fp, "Reverb = %d\n", iUseReverb );
			fprintf( fp, "Mono = %d\n", iDisStereo );
			fprintf( fp, "UseXA = %d\n", iUseXA );
			fprintf( fp, "XAPitch = %d\n", iXAPitch );
			fprintf( fp, "IRQWait = %d\n", iSPUIRQWait );
			fprintf( fp, "Debugger = %d\n", iDebugMode );
			fprintf( fp, "Recorder = %d\n", iRecordMode );
			fprintf( fp, "EmuType = %d\n", iEmuType );
			fprintf( fp, "ReverbBoost = %d\n", iReverbBoost );

			fprintf( fp, "Latency = %d\n", iLatency );
			fprintf( fp, "XAFilter = %d\n", iXAInterp );
			fprintf( fp, "CDDAFilter = %d\n", iCDDAInterp );
			fprintf( fp, "Output1Filter = %d\n", iOutputInterp1 );
			fprintf( fp, "Output2Filter = %d\n", iOutputInterp2 );
			fprintf( fp, "XAVol = %d\n", iVolXA );
			fprintf( fp, "CDDAVol = %d\n", iVolCDDA );
			fprintf( fp, "VoiceVol = %d\n", iVolVoices );
			fprintf( fp, "XAStrength = %d\n", iXAStrength );
			fprintf( fp, "CDDAStrength = %d\n", iCDDAStrength );
			fprintf( fp, "Output2Strength = %d\n", iOutput2Strength );

			fclose(fp);
		}
	}


	latency_target = (iLatency+1) * 10;

	// =================================
	// =================================
	// =================================

	// set defaults here
	reverb_target = 33;
	if( iReverbBoost ) reverb_target = 38;

	latency_restart = 15;
	sound_stretcher = 0;

	debug_sound_buffer = 0;
	debug_cdxa_buffer = 0;

	phantom_padder = 0;
	phantom_pad_size = 50;
	phantom_post_pad = 500;
	APU_run = 45;

	upload_timer = 25;
	upload_low_reset = 2;
	upload_high_full = 30;
	upload_high_reset = 60;

	async_wait_block = 13;
	async_ondemand_block = 8;


	reset = 1;


	fp = fopen( "plugins\\spuPeopsSound_expert.ini", "r" );
	if( !fp ) fp = fopen( "spuPeopsSound_expert.ini", "r" );


	if( fp ) {
		int temp;

		fscanf( fp, "Version = %s\n", &ini_version );

		if( strcmp( ini_version,INI_VERSION_EXPERT ) == 0 ) {
			fscanf( fp, "LatencyTarget = %d\n", &temp );
			if( temp != -1 ) latency_target = temp;

			fscanf( fp, "UploadTimer = %d\n", &upload_timer );
			fscanf( fp, "SoundStretcher = %d\n", &sound_stretcher );
			fscanf( fp, "\n" );
			
			fscanf( fp, "APUcycles = %d\n", &APU_run );
			fscanf( fp, "PhantomPad = %d\n", &phantom_padder );
			fscanf( fp, "PhantomPadSize = %d\n", &phantom_pad_size );
			fscanf( fp, "PhantomPostPad = %d\n", &phantom_post_pad );
			fscanf( fp, "LatencyRestart = %d\n", &latency_restart );
			fscanf( fp, "UploadLowReset = %d\n", &upload_low_reset );
			fscanf( fp, "UploadHighFull = %d\n", &upload_high_full );
			fscanf( fp, "UploadHighReset = %d\n", &upload_high_reset );

			fscanf( fp, "ReverbTarget = %d\n", &temp );
			if( temp != -1 ) reverb_target = temp;

			fscanf( fp, "AsyncWaitBlocker = %d\n", &async_wait_block );
			fscanf( fp, "AsyncOndemandBlocker = %d\n", &async_ondemand_block );
			fscanf( fp, "DebugSoundBuffer = %d\n", &debug_sound_buffer );
			fscanf( fp, "DebugCdxaBuffer = %d\n", &debug_cdxa_buffer );


			reset = 0;
		}
		fclose(fp);
	}


	if( reset ) {
		// Defaults
		fp = fopen( "plugins\\spuPeopsSound_expert.ini", "w" );
		if( !fp ) fp = fopen( "spuPeopsSound_expert.ini", "w" );


		if( fp ) {
			fprintf( fp, "Version = %s\n", INI_VERSION_EXPERT );
			fprintf( fp, "LatencyTarget = %d\n", -1 );
			fprintf( fp, "UploadTimer = %d\n", upload_timer );
			fprintf( fp, "SoundStretcher = %d\n", sound_stretcher );
			fprintf( fp, "\n" );

			fprintf( fp, "APUcycles = %d\n", APU_run );
			fprintf( fp, "PhantomPad = %d\n", phantom_padder );
			fprintf( fp, "PhantomPadSize = %d\n", phantom_pad_size );
			fprintf( fp, "PhantomPostPad = %d\n", phantom_post_pad );
			fprintf( fp, "LatencyRestart = %d\n", latency_restart );
			fprintf( fp, "UploadLowReset = %d\n", upload_low_reset );
			fprintf( fp, "UploadHighFull = %d\n", upload_high_full );
			fprintf( fp, "UploadHighReset = %d\n", upload_high_reset );
			fprintf( fp, "ReverbTarget = %d\n", -1 );
			fprintf( fp, "AsyncWaitBlocker = %d\n", async_wait_block );
			fprintf( fp, "AsyncOndemandBlocker = %d\n", async_ondemand_block );
			fprintf( fp, "DebugSoundBuffer = %d\n", debug_sound_buffer );
			fprintf( fp, "DebugCdxaBuffer = %d\n", debug_cdxa_buffer );

			fclose(fp);
		}
	}
	

	// clip limit
	if( APU_run > 50 ) APU_run = 50;

	// =================================
	// =================================
	// =================================

	if(iZincEmu) 
  {
		// don't do this - creates problems
		//iVolume=1;    // with ZINC, max volume is needed (or qsound will be too loud)
		iUseTimer=1;		// with ZINC, only timer mode is possible
		iDebugMode=0;		// with ZINC, no debug mode possible (we don't get SPUasyncs)
		iDisStereo=0;		// with ZINC, no mono possible (or qsound mixing troubles)
		iRecordMode=0;	// with ZINC, no debug mode possible (we don't get SPUasyncs)
		phantom_padder = 0;
  }



	if(iUseTimer>4) iUseTimer=4;              // some checks
	if(iVolume<0) iVolume=0;
	if(iVolume>20) iVolume=20;
	
	
	switch( iEmuType ) {
		// Generic NTSC
		case 0:
			cpu_clock = 33868800;
			break;
			
			
			// Generic PAL
		case 1:
			cpu_clock = 33864900;
			break;
	}
	
	
	// fast async drain problem
	if( iUseTimer >= 2 ) {
		/*
		Novastorm - lowest value allowed
		- keeps intro cutscene in sync (see video playback)
		
			10ms run speed (ePSXe 170 shark)
		*/
		
		// need drainage for async methods
		cpu_clock -= 200000;
	}
	
	
	// always enable
	iUseDBufIrq = 1;
}

////////////////////////////////////////////////////////////////////////
// WRITE CONFIG: in win registry
////////////////////////////////////////////////////////////////////////

void WriteConfig(void)
{
	FILE *fp;
	

	fp = fopen( "plugins\\spuPeopsSound.ini", "w" );
	if( !fp ) fp = fopen( "spuPeopsSound.ini", "w" );

	if( fp ) {
		fprintf( fp, "Version = %s\n", INI_VERSION );
		fprintf( fp, "SoundDriver = %d\n", iOutputDriver );
		fprintf( fp, "Timer = %d\n", iUseTimer );
		fprintf( fp, "Volume = %d\n", iVolume );
		fprintf( fp, "Interpolation = %d\n", iUseInterpolation );
		fprintf( fp, "Reverb = %d\n", iUseReverb );
		fprintf( fp, "Mono = %d\n", iDisStereo );
		fprintf( fp, "UseXA = %d\n", iUseXA );
		fprintf( fp, "XAPitch = %d\n", iXAPitch );
		fprintf( fp, "IRQWait = %d\n", iSPUIRQWait );
		fprintf( fp, "Debugger = %d\n", iDebugMode );
		fprintf( fp, "Recorder = %d\n", iRecordMode );
		fprintf( fp, "EmuType = %d\n", iEmuType );
		fprintf( fp, "ReverbBoost = %d\n", iReverbBoost );

		fprintf( fp, "Latency = %d\n", iLatency );
		fprintf( fp, "XAFilter = %d\n", iXAInterp );
		fprintf( fp, "CDDAFilter = %d\n", iCDDAInterp );
		fprintf( fp, "Output1Filter = %d\n", iOutputInterp1 );
		fprintf( fp, "Output2Filter = %d\n", iOutputInterp2 );
		fprintf( fp, "XAVol = %d\n", iVolXA );
		fprintf( fp, "CDDAVol = %d\n", iVolCDDA );
		fprintf( fp, "VoiceVol = %d\n", iVolVoices );
		fprintf( fp, "XAStrength = %d\n", iXAStrength );
		fprintf( fp, "CDDAStrength = %d\n", iCDDAStrength );
		fprintf( fp, "Output2Strength = %d\n", iOutput2Strength );

		fclose(fp);
	}
}


void WriteConfigAdvanced(void)
{
}

#ifndef _XBOX

////////////////////////////////////////////////////////////////////////
// INIT WIN CFG DIALOG
////////////////////////////////////////////////////////////////////////

BOOL OnInitDSoundDialog(HWND hW) 
{
	HWND hWC;
	
	ReadConfig();
	
	if(iUseXA)      CheckDlgButton(hW,IDC_ENABXA,TRUE);
	if(iXAPitch)    CheckDlgButton(hW,IDC_XAPITCH,TRUE);
	
	hWC=GetDlgItem(hW,IDC_VOLUME);
	ComboBox_AddString(hWC, "0: none");
	ComboBox_AddString(hWC, "1: 0.10x");
	ComboBox_AddString(hWC, "2: 0.20x");
	ComboBox_AddString(hWC, "3: 0.30x");
	ComboBox_AddString(hWC, "4: 0.40x");
	ComboBox_AddString(hWC, "5: 0.50x - soft");
	ComboBox_AddString(hWC, "6: 0.60x");
	ComboBox_AddString(hWC, "7: 0.70x");
	ComboBox_AddString(hWC, "8: 0.80x");
	ComboBox_AddString(hWC, "9: 0.90x");
	ComboBox_AddString(hWC, "10: 1.00x - normal");
	ComboBox_AddString(hWC, "11: 1.10x");
	ComboBox_AddString(hWC, "12: 1.20x");
	ComboBox_AddString(hWC, "13: 1.30x");
	ComboBox_AddString(hWC, "14: 1.40x");
	ComboBox_AddString(hWC, "15: 1.50x - loud");
	ComboBox_AddString(hWC, "16: 1.60x");
	ComboBox_AddString(hWC, "17: 1.70x");
	ComboBox_AddString(hWC, "18: 1.80x");
	ComboBox_AddString(hWC, "19: 1.90x");
	ComboBox_AddString(hWC, "20: 2.00x - boost");
	ComboBox_SetCurSel(hWC,iVolume);
	
	if(iSPUIRQWait) CheckDlgButton(hW,IDC_IRQWAIT,TRUE);
	if(iDebugMode)  CheckDlgButton(hW,IDC_DEBUGMODE,TRUE);
	if(iRecordMode) CheckDlgButton(hW,IDC_RECORDMODE,TRUE);
	if(iDisStereo)  CheckDlgButton(hW,IDC_DISSTEREO,TRUE);
	if(iReverbBoost) CheckDlgButton(hW,IDC_REVERB_BOOST,TRUE);
	
	hWC=GetDlgItem(hW,IDC_USETIMER);
	ComboBox_AddString(hWC, "0: Thread (self-timer)");
	ComboBox_AddString(hWC, "1: Timer (system real-time clock)");
	ComboBox_AddString(hWC, "2: Async simple (fast-forward, skip timing)");
	ComboBox_AddString(hWC, "3: Async wait (cpu sleep, lockstep timing)");
	ComboBox_AddString(hWC, "4: Async on-demand (best quality, heavy system)");
	ComboBox_SetCurSel(hWC,iUseTimer);
	
	hWC=GetDlgItem(hW,IDC_USEREVERB);
	ComboBox_AddString(hWC, "0: No reverb (fastest)");
	ComboBox_AddString(hWC, "1: Simple reverb (fakes common effects)");
	ComboBox_AddString(hWC, "2: PSX reverb (best quality)");
	ComboBox_SetCurSel(hWC,iUseReverb);
	
	hWC=GetDlgItem(hW,IDC_INTERPOL);
	ComboBox_AddString(hWC, "0: None (fastest)");
	ComboBox_AddString(hWC, "1: Simple interpolation");
	ComboBox_AddString(hWC, "2: Gaussian interpolation (psx quality)");
	ComboBox_AddString(hWC, "3: 4-tap FIR");
	ComboBox_AddString(hWC, "4: Cubic interpolation (better treble)");
	ComboBox_AddString(hWC, "5: Hermite");
	ComboBox_AddString(hWC, "6: Catmull-Rom");
	ComboBox_AddString(hWC, "7: Catmull-Rom (desharp)");
	ComboBox_SetCurSel(hWC,iUseInterpolation);
	
	hWC=GetDlgItem(hW,IDC_EMULATOR);
	ComboBox_AddString(hWC, "0: NTSC");
	ComboBox_AddString(hWC, "1: PAL");
	ComboBox_SetCurSel(hWC,iEmuType);
	
	hWC=GetDlgItem(hW,IDC_OUTPUT_DRIVER);
	ComboBox_AddString(hWC, "0: DirectSound (XP recommended)");
	ComboBox_AddString(hWC, "1: XAudio 2 - stereo (XP, Vista, Win 7)");
	ComboBox_AddString(hWC, "2: XAudio 2 - 5.1 upmix (XP, Vista, Win 7)");
	ComboBox_AddString(hWC, "3: XAudio 2 - 7.1 upmix (XP, Vista, Win 7)");
	//ComboBox_AddString(hWC, "4: OpenAL (All OS)");
	ComboBox_SetCurSel(hWC,iOutputDriver);
	
	return TRUE;	                
}


BOOL OnInitDSoundAdvancedDialog(HWND hW) 
{
	HWND hWC;
	
	hWC=GetDlgItem(hW,IDC_ADVANCED_LATENCY);
	ComboBox_AddString(hWC, "0: 10-25 ms");
	ComboBox_AddString(hWC, "1: 20-35 ms");
	ComboBox_AddString(hWC, "2: 30-45 ms");
	ComboBox_AddString(hWC, "3: 40-55 ms");
	ComboBox_AddString(hWC, "4: 50-65 ms");
	ComboBox_AddString(hWC, "5: 60-75 ms");
	ComboBox_AddString(hWC, "6: 70-85 ms");
	ComboBox_AddString(hWC, "7: 80-95 ms");
	ComboBox_AddString(hWC, "8: 90-105 ms");
	ComboBox_SetCurSel(hWC,iLatency);
	
	hWC=GetDlgItem(hW,IDC_ADVANCED_XA);
	ComboBox_AddString(hWC, "0: None");
	ComboBox_AddString(hWC, "1: Simple");
	ComboBox_AddString(hWC, "2: Gaussian");
	ComboBox_AddString(hWC, "3: 4-tap FIR");
	ComboBox_AddString(hWC, "4: Cubic");
	ComboBox_AddString(hWC, "5: Hermite");
	ComboBox_AddString(hWC, "6: Catmull-Rom");
	ComboBox_AddString(hWC, "7: Catmull-Rom (desharp)");
	ComboBox_SetCurSel(hWC,iXAInterp);
	
	hWC=GetDlgItem(hW,IDC_ADVANCED_CDDA);
	ComboBox_AddString(hWC, "0: None");
	ComboBox_AddString(hWC, "1: Simple");
	ComboBox_AddString(hWC, "2: Gaussian");
	ComboBox_AddString(hWC, "3: 4-tap FIR");
	ComboBox_AddString(hWC, "4: Cubic");
	ComboBox_AddString(hWC, "5: Hermite");
	ComboBox_AddString(hWC, "6: Catmull-Rom");
	ComboBox_AddString(hWC, "7: Catmull-Rom (desharp)");
	ComboBox_SetCurSel(hWC,iCDDAInterp);
	
	hWC=GetDlgItem(hW,IDC_ADVANCED_OUTPUT1);
	ComboBox_AddString(hWC, "0: Normal");
	ComboBox_AddString(hWC, "1: Frequency Response");
	ComboBox_SetCurSel(hWC,iOutputInterp1);
	
	hWC=GetDlgItem(hW,IDC_ADVANCED_OUTPUT2);
	ComboBox_AddString(hWC, "0: None");
	ComboBox_AddString(hWC, "1: Simple");
	ComboBox_AddString(hWC, "2: Gaussian");
	ComboBox_AddString(hWC, "3: 4-tap FIR");
	ComboBox_AddString(hWC, "4: Cubic");
	ComboBox_AddString(hWC, "5: Hermite");
	ComboBox_AddString(hWC, "6: Catmull-Rom");
	ComboBox_AddString(hWC, "7: Catmull-Rom (desharp)");
	ComboBox_SetCurSel(hWC,iOutputInterp2);
	
	hWC=GetDlgItem(hW,IDC_ADVANCED_VOLUME_CDDA);
	ComboBox_AddString(hWC, "0: none");
	ComboBox_AddString(hWC, "1: 0.10x");
	ComboBox_AddString(hWC, "2: 0.20x");
	ComboBox_AddString(hWC, "3: 0.30x");
	ComboBox_AddString(hWC, "4: 0.40x");
	ComboBox_AddString(hWC, "5: 0.50x - soft");
	ComboBox_AddString(hWC, "6: 0.60x");
	ComboBox_AddString(hWC, "7: 0.70x");
	ComboBox_AddString(hWC, "8: 0.80x");
	ComboBox_AddString(hWC, "9: 0.90x");
	ComboBox_AddString(hWC, "10: 1.00x - normal");
	ComboBox_AddString(hWC, "11: 1.10x");
	ComboBox_AddString(hWC, "12: 1.20x");
	ComboBox_AddString(hWC, "13: 1.30x");
	ComboBox_AddString(hWC, "14: 1.40x");
	ComboBox_AddString(hWC, "15: 1.50x - loud");
	ComboBox_AddString(hWC, "16: 1.60x");
	ComboBox_AddString(hWC, "17: 1.70x");
	ComboBox_AddString(hWC, "18: 1.80x");
	ComboBox_AddString(hWC, "19: 1.90x");
	ComboBox_AddString(hWC, "20: 2.00x - boost");
	ComboBox_SetCurSel(hWC,iVolCDDA);
	
	hWC=GetDlgItem(hW,IDC_ADVANCED_VOLUME_XA);
	ComboBox_AddString(hWC, "0: none");
	ComboBox_AddString(hWC, "1: 0.10x");
	ComboBox_AddString(hWC, "2: 0.20x");
	ComboBox_AddString(hWC, "3: 0.30x");
	ComboBox_AddString(hWC, "4: 0.40x");
	ComboBox_AddString(hWC, "5: 0.50x - soft");
	ComboBox_AddString(hWC, "6: 0.60x");
	ComboBox_AddString(hWC, "7: 0.70x");
	ComboBox_AddString(hWC, "8: 0.80x");
	ComboBox_AddString(hWC, "9: 0.90x");
	ComboBox_AddString(hWC, "10: 1.00x - normal");
	ComboBox_AddString(hWC, "11: 1.10x");
	ComboBox_AddString(hWC, "12: 1.20x");
	ComboBox_AddString(hWC, "13: 1.30x");
	ComboBox_AddString(hWC, "14: 1.40x");
	ComboBox_AddString(hWC, "15: 1.50x - loud");
	ComboBox_AddString(hWC, "16: 1.60x");
	ComboBox_AddString(hWC, "17: 1.70x");
	ComboBox_AddString(hWC, "18: 1.80x");
	ComboBox_AddString(hWC, "19: 1.90x");
	ComboBox_AddString(hWC, "20: 2.00x - boost");
	ComboBox_SetCurSel(hWC,iVolXA);
	
	hWC=GetDlgItem(hW,IDC_ADVANCED_VOLUME_VOICES);
	ComboBox_AddString(hWC, "0: none");
	ComboBox_AddString(hWC, "1: 0.10x");
	ComboBox_AddString(hWC, "2: 0.20x");
	ComboBox_AddString(hWC, "3: 0.30x");
	ComboBox_AddString(hWC, "4: 0.40x");
	ComboBox_AddString(hWC, "5: 0.50x - soft");
	ComboBox_AddString(hWC, "6: 0.60x");
	ComboBox_AddString(hWC, "7: 0.70x");
	ComboBox_AddString(hWC, "8: 0.80x");
	ComboBox_AddString(hWC, "9: 0.90x");
	ComboBox_AddString(hWC, "10: 1.00x - normal");
	ComboBox_AddString(hWC, "11: 1.10x");
	ComboBox_AddString(hWC, "12: 1.20x");
	ComboBox_AddString(hWC, "13: 1.30x");
	ComboBox_AddString(hWC, "14: 1.40x");
	ComboBox_AddString(hWC, "15: 1.50x - loud");
	ComboBox_AddString(hWC, "16: 1.60x");
	ComboBox_AddString(hWC, "17: 1.70x");
	ComboBox_AddString(hWC, "18: 1.80x");
	ComboBox_AddString(hWC, "19: 1.90x");
	ComboBox_AddString(hWC, "20: 2.00x - boost");
	ComboBox_SetCurSel(hWC,iVolVoices);
	
	hWC=GetDlgItem(hW,IDC_ADVANCED_XA_STRENGTH);
	ComboBox_AddString(hWC, "0: basic");
	ComboBox_AddString(hWC, "1: 0.10x");
	ComboBox_AddString(hWC, "2: 0.20x");
	ComboBox_AddString(hWC, "3: 0.30x");
	ComboBox_AddString(hWC, "4: 0.40x");
	ComboBox_AddString(hWC, "5: 0.50x");
	ComboBox_AddString(hWC, "6: 0.60x");
	ComboBox_AddString(hWC, "7: 0.70x");
	ComboBox_AddString(hWC, "8: 0.80x");
	ComboBox_AddString(hWC, "9: 0.90x");
	ComboBox_AddString(hWC, "10: 1.00x");
	ComboBox_SetCurSel(hWC,iXAStrength);
	
	hWC=GetDlgItem(hW,IDC_ADVANCED_CDDA_STRENGTH);
	ComboBox_AddString(hWC, "0: basic");
	ComboBox_AddString(hWC, "1: 0.10x");
	ComboBox_AddString(hWC, "2: 0.20x");
	ComboBox_AddString(hWC, "3: 0.30x");
	ComboBox_AddString(hWC, "4: 0.40x");
	ComboBox_AddString(hWC, "5: 0.50x");
	ComboBox_AddString(hWC, "6: 0.60x");
	ComboBox_AddString(hWC, "7: 0.70x");
	ComboBox_AddString(hWC, "8: 0.80x");
	ComboBox_AddString(hWC, "9: 0.90x");
	ComboBox_AddString(hWC, "10: 1.00x");
	ComboBox_SetCurSel(hWC,iCDDAStrength);
	
	hWC=GetDlgItem(hW,IDC_ADVANCED_OUTPUT2_STRENGTH);
	ComboBox_AddString(hWC, "0: basic");
	ComboBox_AddString(hWC, "1: 0.10x");
	ComboBox_AddString(hWC, "2: 0.20x");
	ComboBox_AddString(hWC, "3: 0.30x");
	ComboBox_AddString(hWC, "4: 0.40x");
	ComboBox_AddString(hWC, "5: 0.50x");
	ComboBox_AddString(hWC, "6: 0.60x");
	ComboBox_AddString(hWC, "7: 0.70x");
	ComboBox_AddString(hWC, "8: 0.80x");
	ComboBox_AddString(hWC, "9: 0.90x");
	ComboBox_AddString(hWC, "10: 1.00x");
	ComboBox_SetCurSel(hWC,iOutput2Strength);
	
	return TRUE;	                
}

////////////////////////////////////////////////////////////////////////
// WIN CFG DLG OK
////////////////////////////////////////////////////////////////////////

void OnDSoundOK(HWND hW) 
{
	HWND hWC;
	
	if(IsDlgButtonChecked(hW,IDC_ENABXA))
		iUseXA=1; else iUseXA=0;
	
	if(IsDlgButtonChecked(hW,IDC_XAPITCH))
		iXAPitch=1; else iXAPitch=0;
	
	hWC=GetDlgItem(hW,IDC_VOLUME);
	iVolume=ComboBox_GetCurSel(hWC);
	
	hWC=GetDlgItem(hW,IDC_USETIMER);
	iUseTimer=ComboBox_GetCurSel(hWC);
	
	hWC=GetDlgItem(hW,IDC_USEREVERB);
	iUseReverb=ComboBox_GetCurSel(hWC);
	
	hWC=GetDlgItem(hW,IDC_INTERPOL);
	iUseInterpolation=ComboBox_GetCurSel(hWC);
	
	hWC=GetDlgItem(hW,IDC_EMULATOR);
	iEmuType=ComboBox_GetCurSel(hWC);
	
	hWC=GetDlgItem(hW,IDC_OUTPUT_DRIVER);
	iOutputDriver=ComboBox_GetCurSel(hWC);
	
	
	if(IsDlgButtonChecked(hW,IDC_IRQWAIT))
		iSPUIRQWait=1; else iSPUIRQWait=0;
	
	if(IsDlgButtonChecked(hW,IDC_DEBUGMODE))
		iDebugMode=1; else iDebugMode=0;
	
	if(IsDlgButtonChecked(hW,IDC_RECORDMODE))
		iRecordMode=1; else iRecordMode=0;
	
	if(IsDlgButtonChecked(hW,IDC_DISSTEREO))
		iDisStereo=1; else iDisStereo=0;
	
	if(IsDlgButtonChecked(hW,IDC_REVERB_BOOST))
		iReverbBoost=1; else iReverbBoost=0;
	
	WriteConfig();                                        // write registry
	
	EndDialog(hW,TRUE);
}


void OnDSoundAdvancedOK(HWND hW) 
{
	HWND hWC;
	
	hWC=GetDlgItem(hW,IDC_ADVANCED_LATENCY);
	iLatency=ComboBox_GetCurSel(hWC);
	
	hWC=GetDlgItem(hW,IDC_ADVANCED_XA);
	iXAInterp=ComboBox_GetCurSel(hWC);
	
	hWC=GetDlgItem(hW,IDC_ADVANCED_CDDA);
	iCDDAInterp=ComboBox_GetCurSel(hWC);
	
	hWC=GetDlgItem(hW,IDC_ADVANCED_OUTPUT1);
	iOutputInterp1=ComboBox_GetCurSel(hWC);
	
	hWC=GetDlgItem(hW,IDC_ADVANCED_OUTPUT2);
	iOutputInterp2=ComboBox_GetCurSel(hWC);
	
	hWC=GetDlgItem(hW,IDC_ADVANCED_VOLUME_CDDA);
	iVolCDDA=ComboBox_GetCurSel(hWC);
	
	hWC=GetDlgItem(hW,IDC_ADVANCED_VOLUME_XA);
	iVolXA=ComboBox_GetCurSel(hWC);
	
	hWC=GetDlgItem(hW,IDC_ADVANCED_VOLUME_VOICES);
	iVolVoices=ComboBox_GetCurSel(hWC);
	
	
	
	hWC=GetDlgItem(hW,IDC_ADVANCED_XA_STRENGTH);
	iXAStrength=ComboBox_GetCurSel(hWC);
	hWC=GetDlgItem(hW,IDC_ADVANCED_CDDA_STRENGTH);
	iCDDAStrength=ComboBox_GetCurSel(hWC);
	hWC=GetDlgItem(hW,IDC_ADVANCED_OUTPUT2_STRENGTH);
	iOutput2Strength=ComboBox_GetCurSel(hWC);
	
	
	WriteConfigAdvanced();                                        // write registry
	
	EndDialog(hW,TRUE);
}

////////////////////////////////////////////////////////////////////////
// WIN CFG DLG CANCEL
////////////////////////////////////////////////////////////////////////

void OnDSoundCancel(HWND hW) 
{
	EndDialog(hW,FALSE);
}


void OnDSoundAdvancedCancel(HWND hW) 
{
	EndDialog(hW,FALSE);
}

////////////////////////////////////////////////////////////////////////
// WIN CFG PROC
////////////////////////////////////////////////////////////////////////

BOOL CALLBACK DSoundAdvancedProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
  {
	case WM_INITDIALOG:
		return OnInitDSoundAdvancedDialog(hW);
		
	case WM_COMMAND:
    {
			switch(LOWORD(wParam))
      {
			case ID_ADVANCED_CANCEL:     OnDSoundAdvancedCancel(hW);return TRUE;
			case ID_ADVANCED_OK:         OnDSoundAdvancedOK(hW);   return TRUE;
      }
    }
  }
	return FALSE;
}



BOOL CALLBACK DSoundDlgProc(HWND hW, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
  {
	case WM_INITDIALOG:
		return OnInitDSoundDialog(hW);
		
	case WM_COMMAND:
    {
			switch(LOWORD(wParam))
      {
			case IDCANCEL:				OnDSoundCancel(hW);return TRUE;
			case IDOK:						OnDSoundOK(hW);   return TRUE;
			case IDC_ADVANCED:
				DialogBox(hInst,MAKEINTRESOURCE(IDD_ADVANCED),
          GetActiveWindow(),(DLGPROC)DSoundAdvancedProc);
				return TRUE;
      }
    }
  }
	return FALSE;
}

#endif

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// LINUX CONFIG/ABOUT HANDLING
////////////////////////////////////////////////////////////////////////

#else

char * pConfigFile=NULL;

#include <unistd.h>

////////////////////////////////////////////////////////////////////////
// START EXTERNAL CFG TOOL
////////////////////////////////////////////////////////////////////////

void StartCfgTool(char * pCmdLine)
{
	FILE * cf;char filename[255],t[255];
	
	strcpy(filename,"cfg/cfgPeopsOSS");
	cf=fopen(filename,"rb");
	if(cf!=NULL)
  {
		fclose(cf);
		getcwd(t,255);
		chdir("cfg");
		sprintf(filename,"./cfgPeopsOSS %s",pCmdLine);
		system(filename);
		chdir(t);
  }
	else
  {
		strcpy(filename,"cfgPeopsOSS");
		cf=fopen(filename,"rb");
		if(cf!=NULL)
    {
			fclose(cf);
			sprintf(filename,"./cfgPeopsOSS %s",pCmdLine);
			system(filename);
    }
		else
    {
			sprintf(filename,"%s/cfgPeopsOSS",getenv("HOME"));
			cf=fopen(filename,"rb");
			if(cf!=NULL)
      {
				fclose(cf);
				getcwd(t,255);
				chdir(getenv("HOME"));
				sprintf(filename,"./cfgPeopsOSS %s",pCmdLine);
				system(filename);
				chdir(t);
      }
			else printf("cfgPeopsOSS not found!\n");
    }
  }
}

/////////////////////////////////////////////////////////
// READ LINUX CONFIG FILE
/////////////////////////////////////////////////////////

void ReadConfigFile(void)
{
	FILE *in;char t[256];int len;
	char * pB, * p;
	
	if(pConfigFile)  
  {
		strcpy(t,pConfigFile); 
		in = fopen(t,"rb"); 
		if(!in) return;
  }
	else 
  {
		strcpy(t,"cfg/spuPeopsOSS.cfg");
		in = fopen(t,"rb"); 
		if(!in) 
    {
			strcpy(t,"spuPeopsOSS.cfg");
			in = fopen(t,"rb"); 
			if(!in) 
      {
				sprintf(t,"%s/spuPeopsOSS.cfg",getenv("HOME")); 
				in = fopen(t,"rb"); 
				if(!in) return;
      }
    }
  } 
	
	pB=(char *)malloc(32767);
	memset(pB,0,32767);
	
	len = fread(pB, 1, 32767, in);
	fclose(in);
	
	strcpy(t,"\nVolume");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
	if(p) iVolume=atoi(p+len);
	if(iVolume<0) iVolume=0;
	if(iVolume>5) iVolume=5;
	
	strcpy(t,"\nUseXA");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
	if(p) iUseXA=atoi(p+len);
	if(iUseXA<0) iUseXA=0;
	if(iUseXA>1) iUseXA=1;
	
	strcpy(t,"\nXAPitch");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;}
	if(p) iXAPitch=atoi(p+len);
	if(iXAPitch<0) iXAPitch=0;
	if(iXAPitch>1) iXAPitch=1;
	
	strcpy(t,"\nHighCompMode");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;} 
	if(p)  iUseTimer=atoi(p+len); 
	if(iUseTimer<0) iUseTimer=0; 
	// note: timer mode 1 (win time events) is not supported
	// in linux. But timer mode 2 (spuupdate) is safe to use.
	if(iUseTimer)   iUseTimer=2; 
	
#ifdef NOTHREADLIB
	iUseTimer=2; 
#endif
	
	strcpy(t,"\nSPUIRQWait");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;} 
	if(p)  iSPUIRQWait=atoi(p+len); 
	if(iSPUIRQWait<0) iSPUIRQWait=0; 
	if(iSPUIRQWait>1) iSPUIRQWait=1; 
	
	strcpy(t,"\nUseReverb");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;} 
	if(p)  iUseReverb=atoi(p+len); 
	if(iUseReverb<0) iUseReverb=0; 
	if(iUseReverb>2) iUseReverb=2; 
	
	strcpy(t,"\nUseInterpolation");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;} 
	if(p)  iUseInterpolation=atoi(p+len); 
	if(iUseInterpolation<0) iUseInterpolation=0; 
	if(iUseInterpolation>4) iUseInterpolation=4; 
	
	strcpy(t,"\nDisStereo");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;} 
	if(p)  iDisStereo=atoi(p+len); 
	if(iDisStereo<0) iDisStereo=0; 
	if(iDisStereo>1) iDisStereo=1; 
	
	strcpy(t,"\nDecodedBufIRQ");p=strstr(pB,t);if(p) {p=strstr(p,"=");len=1;} 
	if(p)  iUseDBufIrq=atoi(p+len); 
	if(iUseDBufIrq<0) iUseDBufIrq=0; 
	if(iUseDBufIrq>1) iUseDBufIrq=1; 
	
	free(pB);
}

/////////////////////////////////////////////////////////
// READ CONFIG called by spu funcs
/////////////////////////////////////////////////////////

void ReadConfig(void)             
{
	iVolume=10; 
	iUseXA=1; 
	iXAPitch=0;
	iSPUIRQWait=1;  
	iUseTimer=2;
	iUseReverb=2;
	iUseInterpolation=2;
	iDisStereo=0;
	iUseDBufIrq=0;
	iFreqResponse=0;
	
	ReadConfigFile();
	
	if(iZincEmu) 
  {
		iVolume=1;    // with ZINC, max volume is needed (or qsound will be too loud)
		iDisStereo=0; // with ZINC, no mono possible (or qsound mixing troubles)
  }
	
}

#endif


