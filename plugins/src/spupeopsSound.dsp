# Microsoft Developer Studio Project File - Name="spupeopsSound" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=spupeopsSound - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "spupeopsSound.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "spupeopsSound.mak" CFG="spupeopsSound - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "spupeopsSound - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "spupeopsSound - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "spupeopsSound - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SPUPEOPSSOUND_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /Ob2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SPUPEOPSSOUND_EXPORTS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /fo"spuPeopsSound.res" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 dsound.lib winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386 /out:"spuPeopsSound_109a.dll"

!ELSEIF  "$(CFG)" == "spupeopsSound - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SPUPEOPSSOUND_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "SPUPEOPSSOUND_EXPORTS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 dsound.lib winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "spupeopsSound - Win32 Release"
# Name "spupeopsSound - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\adsr.cpp

!IF  "$(CFG)" == "spupeopsSound - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "spupeopsSound - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\alsa.cpp
# End Source File
# Begin Source File

SOURCE=.\cfg.cpp
# End Source File
# Begin Source File

SOURCE=.\debug.cpp
# End Source File
# Begin Source File

SOURCE=.\dma.cpp
# End Source File
# Begin Source File

SOURCE=.\dsound.cpp

!IF  "$(CFG)" == "spupeopsSound - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "spupeopsSound - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\freeze.cpp
# End Source File
# Begin Source File

SOURCE=.\openal.cpp

!IF  "$(CFG)" == "spupeopsSound - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "spupeopsSound - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\oss.cpp
# End Source File
# Begin Source File

SOURCE=.\output.cpp
# End Source File
# Begin Source File

SOURCE=.\psemu.cpp
# End Source File
# Begin Source File

SOURCE=.\record.cpp
# End Source File
# Begin Source File

SOURCE=.\registers.cpp
# End Source File
# Begin Source File

SOURCE=.\reverb.cpp

!IF  "$(CFG)" == "spupeopsSound - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "spupeopsSound - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\spu.cpp
# End Source File
# Begin Source File

SOURCE=.\spuPeopsSound.cpp
# End Source File
# Begin Source File

SOURCE=.\spuPeopsSound.def
# End Source File
# Begin Source File

SOURCE=.\stdafx.cpp
# End Source File
# Begin Source File

SOURCE=.\xa.cpp
# End Source File
# Begin Source File

SOURCE=.\xaudio_2.cpp
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\zn.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\adsr.h
# End Source File
# Begin Source File

SOURCE=.\afxres.h
# End Source File
# Begin Source File

SOURCE=.\alsa.h
# End Source File
# Begin Source File

SOURCE=.\cfg.h
# End Source File
# Begin Source File

SOURCE=.\debug.h
# End Source File
# Begin Source File

SOURCE=.\dma.h
# End Source File
# Begin Source File

SOURCE=.\dsoundoss.h
# End Source File
# Begin Source File

SOURCE=.\externals.h
# End Source File
# Begin Source File

SOURCE=.\freeze.h
# End Source File
# Begin Source File

SOURCE=.\gauss_i.h
# End Source File
# Begin Source File

SOURCE=.\oss.h
# End Source File
# Begin Source File

SOURCE=.\psemuxa.h
# End Source File
# Begin Source File

SOURCE=.\record.h
# End Source File
# Begin Source File

SOURCE=.\registers.h
# End Source File
# Begin Source File

SOURCE=.\regs.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\reverb.h
# End Source File
# Begin Source File

SOURCE=.\spu.h
# End Source File
# Begin Source File

SOURCE=.\stdafx.h
# End Source File
# Begin Source File

SOURCE=.\WINRES.H
# End Source File
# Begin Source File

SOURCE=.\xa.h
# End Source File
# Begin Source File

SOURCE=.\xaudio_2.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\bitmap1.bmp
# End Source File
# Begin Source File

SOURCE=.\bitmap2.bmp
# End Source File
# Begin Source File

SOURCE=.\bitmap3.bmp
# End Source File
# Begin Source File

SOURCE=.\bitmap4.bmp
# End Source File
# Begin Source File

SOURCE=.\bitmap5.bmp
# End Source File
# Begin Source File

SOURCE=.\spuPeopsSound.rc
# End Source File
# End Group
# End Target
# End Project
