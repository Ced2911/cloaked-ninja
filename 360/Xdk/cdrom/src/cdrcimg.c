/*
 * (C) Gražvydas "notaz" Ignotas, 2010
 *
 * This work is licensed under the terms of any of these licenses
 * (at your option):
 *  - GNU GPL, version 2 or later.
 *  - GNU LGPL, version 2.1 or later.
 * See the COPYING file in the top-level directory.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zlib.h>
#include <bzlib.h>
#include <xtl.h>
#include "cdrcimg.h"

#define SWAP16(x) _byteswap_ushort(x)
#define SWAP32(x) _byteswap_ulong(x)

#define PFX "cdrcimg: "
#define err(f, ...) printf(PFX f, ##__VA_ARGS__)

#define CD_FRAMESIZE_RAW 2352

#define TR {printf("[Trace] in function %s, line %d, file %s\n",__FUNCTION__,__LINE__,__FILE__);}

enum {
    CDRC_ZLIB,
    CDRC_ZLIB2,
    CDRC_BZ,
};

static const char *cd_fname;
static unsigned int *cd_index_table;
static unsigned int cd_index_len;
static unsigned int cd_sectors_per_blk;
static int cd_compression;
static FILE *cd_file;

static struct {
    unsigned char raw[16][CD_FRAMESIZE_RAW];
    unsigned char compressed[CD_FRAMESIZE_RAW * 16 + 100];
} *cdbuffer;
static int current_block, current_sect_in_blk;

struct CdrStat;
extern long CDR__getStatus(struct CdrStat *stat);

struct CdrStat {
    unsigned long Type;
    unsigned long Status;
    unsigned char Time[3]; // current playing time
};

struct trackinfo {

    enum {
        DATA, CDDA
    } type;
    char start[3]; // MSF-format
    char length[3]; // MSF-format
};

#define MAXTRACKS 100 /* How many tracks can a CD hold? */

static int numtracks = 0;

#define btoi(b)           ((b) / 16 * 10 + (b) % 16) /* BCD to u_char */
#define MSF2SECT(m, s, f) (((m) * 60 + (s) - 2) * 75 + (f))

// return Starting and Ending Track
// buffer:
//  byte 0 - start track
//  byte 1 - end track

long CDRCIMGgetTN(unsigned char *buffer) {
    buffer[0] = 1;
    buffer[1] = numtracks > 0 ? numtracks : 1;

    return 0;
}

// return Track Time
// buffer:
//  byte 0 - frame
//  byte 1 - second
//  byte 2 - minute

long CDRCIMGgetTD(unsigned char track, unsigned char *buffer) {
    buffer[2] = 0;
    buffer[1] = 2;
    buffer[0] = 0;

    return 0;
}

int uncompress2(void *out, unsigned long *out_size, void *in, unsigned long in_size) {
    static z_stream z;
    int ret = 0;

    if (z.zalloc == NULL) {
        // XXX: one-time leak here..
        z.next_in = Z_NULL;
        z.avail_in = 0;
        z.zalloc = Z_NULL;
        z.zfree = Z_NULL;
        z.opaque = Z_NULL;
        ret = inflateInit2(&z, -15);
    } else
        ret = inflateReset(&z);
    if (ret != Z_OK)
        return ret;

    z.next_in = in;
    z.avail_in = in_size;
    z.next_out = out;
    z.avail_out = *out_size;

    ret = inflate(&z, Z_NO_FLUSH);
    //inflateEnd(&z);

    *out_size -= z.avail_out;
    return ret == 1 ? 0 : ret;
}

// read track
// time: byte 0 - minute; byte 1 - second; byte 2 - frame
// uses bcd format

long CDRCIMGreadTrack(unsigned char *time) {
    unsigned int start_byte, size;
    unsigned long cdbuffer_size;
    int ret, sector, block;

    if (cd_file == NULL)
        return -1;

    sector = MSF2SECT(btoi(time[0]), btoi(time[1]), btoi(time[2]));

    // avoid division if possible
    switch (cd_sectors_per_blk) {
        case 1:
            block = sector;
            current_sect_in_blk = 0;
            break;
        case 10:
            block = sector / 10;
            current_sect_in_blk = sector % 10;
            break;
        case 16:
            block = sector >> 4;
            current_sect_in_blk = sector & 15;
            break;
        default:
            err("unhandled cd_sectors_per_blk: %d\n", cd_sectors_per_blk);
            return -1;
    }

    if (block == current_block) {
        // it's already there, nothing to do
        //printf("hit sect %d\n", sector);
        return 0;
    }

    if (sector >= cd_index_len * cd_sectors_per_blk) {
        err("sector %d is past track end\n", sector);
        return -1;
    }

    start_byte = cd_index_table[block];
    if (fseek(cd_file, start_byte, SEEK_SET) != 0) {
        err("seek error for block %d at %x: ",
                block, start_byte);
        perror(NULL);
        return -1;
    }

    size = cd_index_table[block + 1] - start_byte;
    if (size > sizeof (cdbuffer->compressed)) {
        err("block %d is too large: %u\n", block, size);
        return -1;
    }

    if (fread(cdbuffer->compressed, 1, size, cd_file) != size) {
        err("read error for block %d at %x: ", block, start_byte);
        perror(NULL);
        return -1;
    }

    cdbuffer_size = sizeof (cdbuffer->raw[0]) * cd_sectors_per_blk;
    switch (cd_compression) {
        case CDRC_ZLIB:
            ret = uncompress(cdbuffer->raw[0], &cdbuffer_size, cdbuffer->compressed, size);
            break;
        case CDRC_ZLIB2:
            ret = uncompress2(cdbuffer->raw[0], &cdbuffer_size, cdbuffer->compressed, size);
            break;
        case CDRC_BZ:
            ret = BZ2_bzBuffToBuffDecompress((char *) cdbuffer->raw, (unsigned int *) &cdbuffer_size,
                    (char *) cdbuffer->compressed, size, 0, 0);
            break;
        default:
            err("bad cd_compression: %d\n", cd_compression);
            return -1;
    }

    if (ret != 0) {
        err("uncompress failed with %d for block %d, sector %d\n",
                ret, block, sector);
        return -1;
    }
    if (cdbuffer_size != sizeof (cdbuffer->raw[0]) * cd_sectors_per_blk)
        err("cdbuffer_size: %lu != %d, sector %d\n", cdbuffer_size,
            sizeof (cdbuffer->raw[0]) * cd_sectors_per_blk, sector);

    // done at last!
    current_block = block;
    return 0;
}

// return read track

unsigned char *CDRCIMGgetBuffer(void) {
    return cdbuffer->raw[current_sect_in_blk] + 12;
}

// plays cdda audio
// sector: byte 0 - minute; byte 1 - second; byte 2 - frame
// does NOT uses bcd format

long CDRCIMGplay(unsigned char *time) {
    return 0;
}

// stops cdda audio

long CDRCIMGstop(void) {
    return 0;
}

// gets subchannel data

unsigned char* CDRCIMGgetBufferSub(void) {
    return NULL;
}

long CDRCIMGgetStatus(struct CdrStat *stat) {
    CDR__getStatus(stat);

    stat->Type = 0x01;

    return 0;
}

long CDRCIMGclose(void) {
    if (cd_file != NULL) {
        fclose(cd_file);
        cd_file = NULL;
    }
    if (cd_index_table != NULL) {
        free(cd_index_table);
        cd_index_table = NULL;
    }
    return 0;
}

long CDRCIMGshutdown(void) {
    return CDRCIMGclose();
}

long CDRCIMGinit(void) {
    if (cdbuffer == NULL) {
        cdbuffer = malloc(sizeof (*cdbuffer));
        if (cdbuffer == NULL) {
            err("OOM\n");
            return -1;
        }
    }
    return 0;
}

// This function is invoked by the front-end when opening an ISO
// file for playback

long CDRCIMGopen(void) {

#pragma pack(1)
    union {

        struct {
            unsigned int offset;
            unsigned short size;
        }  ztab_entry;

        struct {
            unsigned int offset;
            unsigned short size;
            unsigned int dontcare;
        } znxtab_entry;
        unsigned int bztab_entry;
    } u;
#pragma pack(pop)

    int tabentry_size;
    char table_fname[256];
    long table_size;
    int i, ret;
    char *ext;
    FILE *f = NULL;

    if (cd_file != NULL)
        return 0; // it's already open

    numtracks = 0;
    current_block = -1;
    current_sect_in_blk = 0;

    if (cd_fname == NULL)
        return -1;

    ext = strrchr(cd_fname, '.');
    if (ext == NULL) {
        return -1;
    } // pocketiso stuff
    else if (stricmp(ext, ".z") == 0) {
        cd_compression = CDRC_ZLIB;
        tabentry_size = sizeof (u.ztab_entry);
        _snprintf(table_fname, sizeof (table_fname), "%s.table", cd_fname);
    } else if (stricmp(ext, ".znx") == 0) {
        cd_compression = CDRC_ZLIB;
        tabentry_size = sizeof (u.znxtab_entry);
        _snprintf(table_fname, sizeof (table_fname), "%s.table", cd_fname);
    } else if (stricmp(ext, ".bz") == 0) {
        cd_compression = CDRC_BZ;
        tabentry_size = sizeof (u.bztab_entry);
        _snprintf(table_fname, sizeof (table_fname), "%s.index", cd_fname);
    } else {
        err("unhandled extension: %s\n", ext);
        return -1;
    }

    f = fopen(table_fname, "rb");
    if (f == NULL) {
        err("missing file: %s: ", table_fname);
        perror(NULL);
        return -1;
    }

    ret = fseek(f, 0, SEEK_END);
    if (ret != 0) {
        err("failed to seek\n");
        goto fail_table_io;
    }
    table_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (table_size > 4 * 1024 * 1024) {
        err(".table too large - %d\n",table_size);
        goto fail_table_io;
    }

    cd_index_len = table_size / tabentry_size;

    cd_index_table = malloc((cd_index_len + 1) * sizeof (cd_index_table[0]));
    if (cd_index_table == NULL)
        goto fail_table_io;

    switch (cd_compression) {
        case CDRC_ZLIB:
            // a Z.table file is binary where each element represents
            // one compressed frame.  
            //    4 bytes: the offset of the frame in the .Z file
            //    2 bytes: the length of the compressed frame
            // .znx.table has 4 additional bytes (xa header??)
            u.znxtab_entry.dontcare = 0;
            
            // dump to usb
            //FILE * fdump = fopen("uda:/dump.tmp","wb");
            
            for (i = 0; i < cd_index_len; i++) {
                memset(&u,0,tabentry_size);
                ret = fread(&u, 1, tabentry_size, f);
                
                //fwrite(&u, 1, tabentry_size, fdump);
                //printf("tabentry_size : %d\r\n",tabentry_size);
                //buffer_dump(&u,tabentry_size);
                //exit(0);
                if (ret != tabentry_size) {
                    err(".table read failed on entry %d/%d\n", i, cd_index_len);
                    goto fail_table_io_read;
                }
                
                u.ztab_entry.offset = SWAP32(u.ztab_entry.offset);
                u.ztab_entry.size = SWAP16(u.ztab_entry.size);
                
                cd_index_table[i] = u.ztab_entry.offset;
                //if (u.znxtab_entry.dontcare != 0)
                //	printf("znx %08x!\n", u.znxtab_entry.dontcare);
                
            }
            //fclose(fdump);
            
            // fake entry, so that we know last compressed block size
            cd_index_table[i] = u.ztab_entry.offset + u.ztab_entry.size;
            cd_sectors_per_blk = 1;
            break;
        case CDRC_BZ:
            // the .BZ.table file is arranged so that one entry represents
            // 10 compressed frames. Each element is a 4 byte unsigned integer
            // representing the offset in the .BZ file. Last entry is the size
            // of the compressed file.
            for (i = 0; i < cd_index_len; i++) {
                ret = fread(&u.bztab_entry, 1, sizeof (u.bztab_entry), f);
                if (ret != sizeof (u.bztab_entry)) {
                    err(".table read failed on entry %d/%d\n", i, cd_index_len);
                    goto fail_table_io_read;
                }
                u.bztab_entry = SWAP32(u.bztab_entry);
                cd_index_table[i] = u.bztab_entry;
            }
            cd_sectors_per_blk = 10;
            break;
    }

    cd_file = fopen(cd_fname, "rb");
    if (cd_file == NULL) {
        err("failed to open: %s: ", table_fname);
        perror(NULL);
        goto fail_img;
    }
    fclose(f);

    printf(PFX "Loaded compressed CD Image: %s.\n", cd_fname);

    return 0;

fail_img:
    fail_table_io_read :
            free(cd_index_table);
    cd_index_table = NULL;
fail_table_io:
    fclose(f);
    return -1;
}

void cdrcimg_set_fname(const char *fname) {
    TR;
    cd_fname = fname;
}
