/***************************************************************************
 *   File io wrapper for pcsx		                                       *
 ***************************************************************************/
#include <xtl.h>

typedef struct _FILE_IO {
	HANDLE h;
} FILE_IO;

FILE_IO * io_open ( const char * filename, const char * mode );
int io_close ( FILE_IO * stream );
size_t io_read ( void * ptr, size_t size, size_t count, FILE_IO * stream );
int io_seek ( FILE_IO * stream, long int offset, int origin );
long int io_tell ( FILE_IO * stream );
char * io_gets ( char * str, int num, FILE_IO * stream );
char io_getc ( FILE_IO * stream );

#define fopen io_open
#define fclose io_close
#define fread io_read
#define fseek io_seek
#define ftell io_tell
#define fgets io_gets
#define fgetc io_getc
#define FILE FILE_IO
