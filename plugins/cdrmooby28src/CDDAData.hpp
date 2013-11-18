/************************************************************************

Copyright mooby 2002

CDRMooby2 CDDAData.hpp
http://mooby.psxfanatics.com

  This file is protected by the GNU GPL which should be included with
  the source code distribution.

************************************************************************/



#ifndef CDDADATA_HPP
#define CDDADATA_HPP

#include "FileInterface.hpp"
#include "CDTime.hpp"
#include "TrackInfo.hpp"

// CDDA data virtual base class
class CDDAData
{
public:
   CDDAData() {}
   virtual ~CDDAData() {}
   virtual void openFile(const std::string& file) = 0;
   virtual int play(const CDTime& startTime) = 0;
   virtual int stop() = 0;
   virtual bool isPlaying(void) {return false;}
   virtual CDTime playTime(void) {return CDTime();}
};

// if there's no CDDA data, just return sensible values
class NoCDDAData : public CDDAData
{
public:
   NoCDDAData() {}
   virtual ~NoCDDAData() {}
   virtual void openFile(const std::string& file) {}
   virtual int play(const CDTime& startTime) {return 0;}
   virtual int stop() {return 0;}
};

// for really playing CDDA data.
class PlayCDDAData : public CDDAData
{
public:
		// ti is the list of track lengths.
      // sets the volume from whatever is stored in the preferences
   PlayCDDAData(const std::vector<TrackInfo> ti, CDTime gapLength);

		// cleans up
   virtual ~PlayCDDAData() {if (playing) stop(); delete theCD; /*Pa_Terminate();*/}

		// opens the file and readies the plugin for playing
   virtual void openFile(const std::string& file);
		// plays data starting at startTime
   virtual int play(const CDTime& startTime);
		// stops playing
   virtual int stop();

   virtual bool isPlaying(void) {return playing;}
   virtual CDTime playTime(void) {return CDDAPos;}

	// All the data members are public so they can be accessed by the PortAudio
	// callback

		// the PortAudio stream for playing
   /*PortAudioStream* stream;*/

      // the volume as set in the configuration window
   double volume;
		// the current position of playing
   CDTime CDDAPos;
		// the end time of this track
   CDTime CDDAEnd;
		// the start time taken from play()
   CDTime CDDAStart;
   CDTime InitialTime;
		// the offset into the frame that it's currently playing from
   unsigned long frameOffset;
		// a FileInterface for getting the data from
   FileInterface* theCD;
		// the track list of the CD for calculating times n such
   std::vector<TrackInfo> trackList;
		// if true, it's playing.
   bool playing;
      // a buffer of null audio if repeat is off
   char nullAudio[bytesPerFrame];
      // if true and repeating one track, this is at the end of the currently playing track
   bool endOfTrack;
      // length of the pregap between tracks 1 and 2
   CDTime pregapLength;
      // time of the pregap
   CDTime pregapTime;
};

#endif
