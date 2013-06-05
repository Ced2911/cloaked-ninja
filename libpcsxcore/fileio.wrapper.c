/***************************************************************************
 *   File io wrapper for pcsx		                                       *
 ***************************************************************************/
#include <stdio.h>
#include "fileio.wrapper.h"

FILE_IO * io_open ( const char * filename, const char * mode ) {
	FILE_IO * fd;
	HANDLE hFile;

	hFile = CreateFile(filename,               // file to open
		GENERIC_READ,          // open for reading
		FILE_SHARE_READ,       // share for reading
		NULL,                  // default security
		OPEN_EXISTING,         // existing file only
		FILE_FLAG_SEQUENTIAL_SCAN  , // normal file
		NULL);

	if (hFile == INVALID_HANDLE_VALUE)  {
		printf("io_open: failed to open %s\n", filename);
		return NULL;
	}

	fd = (FILE_IO*)malloc(sizeof(FILE_IO));
	fd->h = hFile;
	return fd;
}

int io_close ( FILE_IO * stream ) {
	if (stream) {
		CloseHandle(stream->h);
		free(stream);
	}
}

size_t io_read ( void * ptr, size_t size, size_t count, FILE_IO * stream ) {
	DWORD dwBytesRead;
	size_t to_read = size * count;
	if( FALSE == ReadFile(stream->h, ptr, to_read, &dwBytesRead, NULL) ) {
		printf("io_read: failed to read %08x\n", to_read);
		return 0;
	}
	return to_read;
}

int io_seek ( FILE_IO * stream, long int offset, int origin ) {
	DWORD dwMoveMethod;
	DWORD dwPtr;
	switch(origin) {
	case SEEK_SET:
		dwMoveMethod = FILE_BEGIN;
		break;
	case SEEK_END:
		dwMoveMethod = FILE_END;
		break;
	case SEEK_CUR:
		dwMoveMethod = FILE_CURRENT;
		break;
	}
	dwPtr = SetFilePointer(stream->h, offset, 0, dwMoveMethod);
	if (dwPtr == INVALID_SET_FILE_POINTER)
	{
		printf("io_seek: failed to seek %08x %08x\n", offset, dwMoveMethod);
		return 1;
	}
	return 0;
}

long int io_tell ( FILE_IO * stream ) {
	DWORD dwMoveMethod;
	DWORD dwPtr;
	dwMoveMethod = FILE_CURRENT;
	dwPtr = SetFilePointer(stream->h, 0, 0, dwMoveMethod);
	if (dwPtr == INVALID_SET_FILE_POINTER)
	{
		printf("io_tell: failed to tell %08x\n", dwPtr);
		return 1;
	}
	return dwPtr;
}

char io_getc(FILE_IO * stream) {
	char c;
	io_read(&c, 1, 1, stream);
	return c;
}

char * io_gets ( char * str, int num, FILE_IO * stream ) {
	char d;
	while( num -- > 0) {
		d = io_getc(stream);		
		*str++ = d;
		if (d == '\n') 
			break;
	}
	return str;
}