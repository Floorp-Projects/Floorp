/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/*--------------------------------------------------------------------
 *  NSDiff
 *
 *  A binary file diffing utility that creates diffs in GDIFF format
 *
 *       nsdiff [-b"blocksize"] [-o"outfile"] oldfile newfile
 *
 *  Blocksize defaults to 32.
 *  The outfile defaults to "newfile" with a .GDF extension
 *
 *------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "gdiff.h"

/*--------------------------------------
 *  constants
 *------------------------------------*/

#define IGNORE_SIZE         7
#define DEFAULT_BLOCKSIZE   32
#define FILENAMESIZE        513
#define OLDBUFSIZE          0x1FFF
#define MAXBUFSIZE          0xFFFF  /* more requires long adds, wasting space */
#define CHKMOD              65536
#define HTBLSIZE            65521

#define destroyFdata(fd)  { if ((fd)->data != NULL) free((fd)->data); }


/*--------------------------------------
 *  types
 *------------------------------------*/

typedef unsigned long   uint32;
typedef long            int32;
typedef unsigned short  uint16;
typedef short           int16;
typedef unsigned char   uchar;
typedef unsigned char   BOOL;

typedef struct hnode {
    uint32          chksum;  /* true checksum of the block */
    uint32          offset;  /* offset into old file */
    struct hnode    *next;   /* next node in collision list */
} HNODE, *HPTR;

typedef struct filedata {
    uint32  filepos;        /* fileposition of start of buffer */
    int32   offset;         /* current position in the buffer */
    int32   unwritten;      /* the start of the unwritten buffer */
    uint32  datalen;        /* actual amount of data in buffer */
    uint32  bufsize;        /* the physical size of the memory buffer */
    uchar * data;           /* a "bufsize" memory block for data */
    FILE  * file;           /* file handle */
    BOOL    bDoAdds;        /* If true check for "add" data before reading */
} FILEDATA;

/*--------------------------------------
 *  prototypes
 *------------------------------------*/

void    add( uchar *data, uint32 count, FILE *outf );
void    adddata( FILEDATA *fd );
void    copy( uint32 offset, uint32 count, FILE *outf );
uint32  checksum( uchar *buffer, uint32 offset, uint32 count );
HPTR    *chunkOldFile( FILE *fh, uint32 blocksize );
void    cleanup( void );
uint32  compareFdata( FILEDATA *oldfd, uint32 oldpos, FILEDATA *newfd );
void    diff( FILE *oldf, FILE *newf, FILE *outf );
uint32  getChecksum( FILEDATA *fd );
BOOL    getFdata( FILEDATA *fdata, uint32 position );
BOOL    initFdata( FILEDATA *fdata, FILE *file, uint32 bufsize, BOOL bDoAdds );
BOOL    moreFdata( FILEDATA *fdata );
int     openFiles( void );
void    readFdata( FILEDATA *fdata, uint32 position );
void    usage( void );
BOOL    validateArgs( int argc, char *argv[] );
void    writeHeader( FILE *outf );


/*--------------------------------------
 *  Global data
 *------------------------------------*/

char    *oldname = NULL,
        *newname = NULL,
        *outname = NULL;

FILE    *oldfile,
        *newfile,
        *outfile;

uint32  blocksize = DEFAULT_BLOCKSIZE;

HPTR    *htbl;

  /* for hashing stats */
uint32  slotsused = 0,
        blockshashed = 0,
        blocksfound = 0,
        hashhits = 0,
        partialmatch = 0,
        cmpfailed = 0;


/*--------------------------------------
 *  main
 *------------------------------------*/

int main( int argc, char *argv[] )
{
    int err;

    /* Parse command line */

    if ( !validateArgs( argc, argv ) ) {
        err = ERR_ARGS;
    }
    else {
        err = openFiles();
    }


    /* Calculate block checksums in old file */

    if ( err == ERR_OK ) {
        htbl = chunkOldFile( oldfile, blocksize );
        if ( htbl == NULL )
            err = ERR_MEM;
    }


    /* Do the diff */

    if ( err == ERR_OK ) {
        writeHeader( outfile );
        diff( oldfile, newfile, outfile );
    }


    /* Cleanup */

    cleanup();


    /* Report status */

    if ( err == ERR_OK ) {
        fprintf( stderr, "\nHashed %ld blocks into %ld slots (out of %ld)\n",
            blockshashed, slotsused, HTBLSIZE );
        fprintf( stderr, "Blocks found: %ld\n", blocksfound );
        fprintf( stderr, "Hashtable hits: %ld\n", hashhits );
        fprintf( stderr, "memcmps failed: %ld\n", cmpfailed );
        fprintf( stderr, "partial matches: %ld\n", partialmatch );
    }
    else {
        switch (err) {
            case ERR_ARGS:
                fprintf( stderr, "Invalid arguments\n" );
                usage();
                break;

            case ERR_ACCESS:
                fprintf( stderr, "Error opening file\n" );
                break;

            case ERR_MEM:
                fprintf( stderr, "Insufficient memory\n" );
                break;

            default:
                fprintf( stderr, "Unknown error %d\n", err );
                break;
        }
    }

    return (err);
}


/*--------------------------------------
 *  add
 *------------------------------------*/

void add( uchar *data, uint32 count, FILE *outf )
{
    uchar numbuf[5];

#ifdef DEBUG
    fprintf( stderr, "Adding %ld bytes\n", count );
#endif

    if ( count == 0 )
        return;

    if ( count <= ADD8MAX ) {
        numbuf[0] = (uchar)count;
        fwrite( numbuf, 1, 1, outf );
    }
    else if ( count <= 0xFFFF ) {
        numbuf[0] = ADD16;
        numbuf[1] = (uchar)( count >> 8 );
        numbuf[2] = (uchar)( count & 0x00FF );
        fwrite( numbuf, 1, 3, outf );
    }
    else {
        numbuf[0] = ADD32;
        numbuf[1] = (uchar)( count >> 24 );
        numbuf[2] = (uchar)( ( count >> 16 ) & 0x00FF );
        numbuf[3] = (uchar)( ( count >> 8 ) & 0x00FF );
        numbuf[4] = (uchar)( count & 0x00FF );
        fwrite( numbuf, 1, 5, outf );
    }

    fwrite( data, 1, count, outf );
}


/*--------------------------------------
 *  copy
 *------------------------------------*/

void copy( uint32 offset, uint32 count, FILE *outf )
{
    uchar numbuf[9];
#ifdef DEBUG
    fprintf( stderr, "Copying %ld bytes from offset %ld\n", count, offset );
#endif

    if ( count == 0 )
        return;

    if ( offset <= 0xFFFF ) {
        numbuf[1] = (uchar)( offset >> 8 );
        numbuf[2] = (uchar)( offset & 0x00FF );

        if ( count <= 0xFF ) {
            numbuf[0] = COPY16BYTE;
            numbuf[3] = (uchar)( count & 0xFF );
            fwrite( numbuf, 1, 4, outf );
        }
        else if ( count <= 0xFFFF ) {
            numbuf[0] = COPY16SHORT;
            numbuf[3] = (uchar)( count >> 8 );
            numbuf[4] = (uchar)( count & 0x00FF );
            fwrite( numbuf, 1, 5, outf );
        }
        else {
            numbuf[0] = COPY16LONG;
            numbuf[3] = (uchar)( count >> 24 );
            numbuf[4] = (uchar)( ( count >> 16 ) & 0x00FF );
            numbuf[5] = (uchar)( ( count >> 8 ) & 0x00FF );
            numbuf[6] = (uchar)( count & 0x00FF );
            fwrite( numbuf, 1, 7, outf );
        }
    }
    else {
        numbuf[1] = (uchar)( offset >> 24 );
        numbuf[2] = (uchar)( ( offset >> 16 ) & 0x00FF );
        numbuf[3] = (uchar)( ( offset >> 8 ) & 0x00FF );
        numbuf[4] = (uchar)( offset & 0x00FF );

        if ( count <= 0xFF ) {
            numbuf[0] = COPY32BYTE;
            numbuf[5] = (uchar)( count & 0xFF );
            fwrite( numbuf, 1, 6, outf );
        }
        else if ( count <= 0xFFFF ) {
            numbuf[0] = COPY32SHORT;
            numbuf[5] = (uchar)( count >> 8 );
            numbuf[6] = (uchar)( count & 0x00FF );
            fwrite( numbuf, 1, 7, outf );
        }
        else {
            numbuf[0] = COPY32LONG;
            numbuf[5] = (uchar)( count >> 24 );
            numbuf[6] = (uchar)( ( count >> 16 ) & 0x00FF );
            numbuf[7] = (uchar)( ( count >> 8 ) & 0x00FF );
            numbuf[8] = (uchar)( count & 0x00FF );
            fwrite( numbuf, 1, 9, outf );
        }
    }
}


/*--------------------------------------
 *  checksum
 *------------------------------------*/

uint32 checksum( uchar *buffer, uint32 offset, uint32 count )
{
    uint32  i,
            s1 = 0,
            s2 = 0;

    for ( i=0; i < count; i++ ) {
        s1 = ( s1 + buffer[ offset+i ] ) % CHKMOD ;
        s2 = ( s2 + s1 ) % CHKMOD ;
    }

    return ( (s2 << 16) + s1 );
}


/*--------------------------------------
 *  chunkOldFile
 *------------------------------------*/

HPTR *chunkOldFile( FILE *fh, uint32 blocksize )
{
    HPTR    *table;
    HPTR    node;
    HPTR    tmp;
    uchar   *buffer;
    uint32  filepos;
    uint32  bufsize;
    uint32  bytesRead;
    uint32  chksum;
    uint32  hash;
    uint32  i;

    bufsize = ( MAXBUFSIZE / blocksize ) * blocksize ;
#ifdef DEBUG
    fprintf( stderr, "bufsize: %ld, tblsize: %ld\n", bufsize, HTBLSIZE*sizeof(HPTR) );
#endif

    table = (HPTR*)malloc( HTBLSIZE * sizeof(HPTR) );
    buffer = (uchar*)malloc( bufsize );

    if ( table == NULL || buffer == NULL) {
        fprintf( stderr, "Out of memory\n" );

        if ( table != NULL )
            free( table );
    }
    else {
        memset( table, 0, HTBLSIZE * sizeof(HPTR) );

        filepos = 0;
        do {
            bytesRead = fread( buffer, 1, bufsize, fh );
#ifdef DEBUG
            fprintf( stderr, "read %ld bytes\n", bytesRead );
#endif

            i = 0;
            while ( i < bytesRead ) {
                blockshashed++;

                node = (HPTR)malloc( sizeof(HNODE) );
                if ( node != NULL ) {

                    if ( blocksize <= bytesRead-i )
                        chksum = checksum( buffer, i, blocksize );
                    else
                        chksum = checksum( buffer, i, bytesRead-i );

                    hash = chksum % HTBLSIZE;

                    node->chksum = chksum;
                    node->offset = filepos + i;
                    node->next   = NULL;

                    tmp = table[hash];
                    if (tmp == NULL) {
                        slotsused++;
                        table[hash] = node;
                    }
                    else {
#ifdef FRONTHASH
                        node->next   = tmp;
                        table[hash]  = node;
#else
                        while (tmp->next != NULL)
                            tmp = tmp->next;
                        tmp->next = node;
#endif
                    }

                }
                else {
                    fprintf( stderr, "Out of memory\n" );
                }

                i += blocksize;
            }

            filepos += bytesRead;

        } while ( bytesRead > 0 );

    }

    /* reset file to beginning for comparison */
    fseek( fh, 0, SEEK_SET );

    if ( buffer != NULL )
        free(buffer);

    return (table);
}


/*--------------------------------------
 *  cleanup
 *------------------------------------*/

void cleanup()
{
    long i;
    HPTR tmp;

    if ( oldfile != NULL )
        fclose( oldfile );

    if ( newfile != NULL )
        fclose( newfile );

    if ( outfile != NULL )
        fclose( outfile );

    if ( htbl != NULL ) {
        for ( i=HTBLSIZE-1; i >= 0; i-- ) {
            while ( htbl[i] != NULL ) {
                tmp = htbl[i]->next;
                free( htbl[i] );
                htbl[i] = tmp;
            }
        }

        free( htbl );
    }
}


#ifndef OLDDIFF
/*--------------------------------------
 *  diff
 *------------------------------------*/

void diff( FILE *oldf, FILE *newf, FILE *outf )
{
    FILEDATA    oldfd,
                newfd;

    uint32  rChk,
            match,
            tmpfilepos,
            tmpoffset,
            hash;

    HPTR    node;

    if ( !initFdata( &oldfd, oldf, OLDBUFSIZE, FALSE ) ||
         !initFdata( &newfd, newf, MAXBUFSIZE, TRUE ) )
    {
        fprintf( stderr, "Out of memory!\n" );
        goto bail;
    }

outerloop:
    while ( moreFdata( &newfd ) ) {
        rChk = getChecksum( &newfd );
        hash = rChk % HTBLSIZE;

        node = htbl[hash];
        while( node != NULL ) {
            hashhits++;

            /* compare checksums to see if this might be a match */
            if ( rChk == node->chksum ) {
                /* might be a match, compare actual bits */
                tmpfilepos = newfd.filepos;
                tmpoffset  = newfd.offset;
                match = compareFdata( &oldfd, node->offset, &newfd );

                if ( match > IGNORE_SIZE ) {
                    blocksfound++;
                    if ( match < blocksize )
                        partialmatch++;

                    /* add any unmatched bytes from new file */
                    if ( newfd.offset > newfd.unwritten )
                        adddata( &newfd );

                    /* copy the matched block from old file */
                    copy( node->offset, match, outf );

                    /* skip the copied bytes */
                    newfd.offset = tmpfilepos+tmpoffset + match - newfd.filepos;
                    newfd.unwritten = newfd.offset;

                    /* don't increment offset in loop, this is the new start */
                    goto outerloop;  /* "continue outerloop" */
                }
                else {
                    /* match failed */
                    cmpfailed++;
                    node = node->next;

                    if ( tmpfilepos != newfd.filepos ) {
                        /* file buffer was moved during failed compare */
                        readFdata( &newfd, tmpfilepos+tmpoffset );
                    }
                }
            }
            else {
                /* can't be a match */
                node = node->next;
            }

        } /* while (node!=NULL) */

        /* examine next byte */
        newfd.offset++;

    } /* while( modeFdata() ) */


    /* check for final unmatched data */

    if ( newfd.offset > newfd.unwritten ) {
        assert( 0 ); /* this should be taken care of in moreFdata now */
        adddata( &newfd );
    }


    /* Terminate the GDIFF file */

    fwrite( GDIFF_EOF, 1, 1, outf );


bail:
    destroyFdata( &oldfd );
    destroyFdata( &newfd );
}


BOOL initFdata( FILEDATA *fdata, FILE *file, uint32 bufsize, BOOL bDoAdds )
{
    fdata->filepos  = 0;
    fdata->offset   = 0;
    fdata->unwritten= 0;
    fdata->datalen  = 0;
    fdata->data     = (uchar*)malloc(bufsize);
    fdata->bufsize  = bufsize;
    fdata->file     = file;
    fdata->bDoAdds  = bDoAdds;

    if ( fdata->data == NULL )
        return (FALSE);

    readFdata( fdata, 0 );

    return (TRUE);
}

BOOL moreFdata( FILEDATA *fd )
{
    if ( fd->offset < fd->datalen ) {
        /* pointer still in buffer */
        return (TRUE);
    }
    else {
        readFdata( fd, (fd->filepos + fd->offset) );

        if (fd->offset >= fd->datalen) {
            /* no more data */
            return (FALSE);
        }
        else
            return (TRUE);
    }
}

BOOL getFdata( FILEDATA *fd, uint32 position )
{
    if ( (position >= fd->filepos) && (position < (fd->filepos+fd->datalen)) ) {
        /* the position is available in the current buffer */
        return (TRUE);
    }
    else {
        /* try to get the position into the buffer */
        readFdata( fd, position );
    }

    if ( position >= (fd->filepos + fd->datalen) ) {
        /* position still not in buffer: not enough data */
        return (FALSE);
    }

    return (TRUE);
}

void readFdata( FILEDATA *fd, uint32 position )
{
    if ( fd->bDoAdds && (fd->offset > fd->unwritten) ) {
        add( fd->data + fd->unwritten, fd->offset - fd->unwritten, outfile );
    }

    if ( position != (fd->filepos + fd->datalen) ) {
        /* it's not just the next read so we must seek to it */
        fseek( fd->file, position, SEEK_SET );
        /* assert that in newfile we move byte by byte */
        assert( !fd->bDoAdds || position == fd->filepos+fd->offset );
    }

    fd->datalen     = fread( fd->data, 1, fd->bufsize, fd->file );
    fd->filepos     = position;
    fd->offset      = 0;
    fd->unwritten   = 0;
#ifdef DEBUG
    fprintf(stderr,"Read %ld %s bytes\n",fd->datalen, fd->bDoAdds?"NEW":"old");
#endif
}

void adddata( FILEDATA *fd )
{
    uint32 chunksize;

    assert( fd->offset <= fd->datalen ); /* must be in the buffer */

    chunksize = fd->offset - fd->unwritten;

    if ( chunksize > 0 ) {
        add( fd->data + fd->unwritten, chunksize, outfile );
        fd->unwritten = fd->offset;
    }
}

uint32 compareFdata( FILEDATA *oldfd, uint32 oldpos, FILEDATA *newfd )
{
    int32  oldoffset, newoffset;
    uint32 count = 0;

    if ( getFdata( oldfd, oldpos ) && (newfd->offset < newfd->datalen) ) {
        oldoffset = oldpos - oldfd->filepos;
        newoffset = newfd->offset;

        while ( *(oldfd->data + oldoffset) == *(newfd->data + newoffset) ) {
            oldoffset++;
            newoffset++;
            count++;

            if ( oldoffset >= oldfd->datalen ) {
                readFdata( oldfd, oldfd->filepos + oldoffset );
                oldoffset = 0;
                if ( oldfd->datalen == 0 ) {
                    /* out of data */
                    break;
                }
            }

            if ( newoffset >= newfd->datalen ) {
                if ( newfd->datalen != newfd->bufsize ) {
                    /* we're at EOF, don't read more */
                    break;
                }
                else {
                    readFdata( newfd, newfd->filepos + newoffset );
                    newoffset = 0;
                    if ( newfd->datalen == 0 ) {
                        /* Hm, at EOF after all */
                        break;
                    }
                }
            }
        }
    }

    return count;
}

uint32  getChecksum( FILEDATA *fd )
{
    /* XXX Need to implement a rolling checksum */
    uint32 remaining;

    assert( fd->offset < fd->datalen ); /* must be in buffer */

    remaining = fd->datalen - fd->offset;

    if ( remaining >= blocksize ) {
        /* plenty of room to calculate */
        return checksum( fd->data, fd->offset, blocksize );
    }
    else {
        /* fewer than 'blocksize' bytes remain in buffer */

        if ( fd->datalen == fd->bufsize ) {
            /* there should be more file to read */
            readFdata( fd, fd->filepos + fd->offset );
            remaining = fd->datalen;
        }

        if ( remaining < blocksize )
            return checksum( fd->data, fd->offset, remaining );
        else
            return checksum( fd->data, fd->offset, blocksize );
    }
}


#else  /* !NEWDIFF, i.e. old buggy diff */
/*--------------------------------------
 *  diff
 *------------------------------------*/

void diff( FILE *oldf, FILE *newf, FILE *outf )
{
    uchar   *olddata,
            *newdata;

    uint32  rChk,
            i,
            unwritten,
            hash,
            maxShifts,
            oldbytes,
            bytesRead;

    HPTR    node;

    olddata = (uchar*)malloc(blocksize);
    newdata = (uchar*)malloc(MAXBUFSIZE);

    if (olddata == NULL || newdata == NULL ) {
        fprintf( stderr, "Out of memory!\n" );
        goto bail;
    }

    do {
        bytesRead = fread( newdata, 1, MAXBUFSIZE, newf );
        if ( bytesRead >= blocksize )
            maxShifts = (bytesRead - blocksize + 1);
        else
            maxShifts = 0;
#ifdef DEBUG
        fprintf( stderr, "read %ld new bytes\n", bytesRead );
#endif

        unwritten = 0;
        for ( i=0; i < maxShifts ; i++ ) {
            /* wastefully slow, convert to rolling checksum */
            rChk = checksum( newdata, i, blocksize );
            hash = rChk % HTBLSIZE;

            node = htbl[hash];
            while( node != NULL ) {
                hashhits++;
                /* compare checksums to see if this might really be a match */
                if ( rChk != node->chksum ) {
                    /* can't be a match */
                    node = node->next;
                }
                else {
                    /* might be a match, compare actual bits */
                    fseek( oldf, node->offset, SEEK_SET );
                    oldbytes = fread( olddata, 1, blocksize, oldf );
                    if ( memcmp( olddata, newdata+i, oldbytes ) != 0 ) {
                        cmpfailed++;
                        node = node->next;
                    }
                    else {
                        blocksfound++;

                        /* add any unmatched bytes from new file */
                        if ( i > unwritten )
                            add( newdata+unwritten, i-unwritten, outf );

                        /* copy the matched block from old file */
                        copy( node->offset, oldbytes, outf );

                        /* skip the copied bytes */
                        unwritten = (i + oldbytes);
                        /* "i" gets one less, the "for" increments it */
                        i = unwritten - 1;

                        /* done with this hash entry */
                        node = NULL;
                    }
                }
            }
        }

        /* prepare to read another block */

        if ( unwritten < i ) {
            /* add the unmatched data */
            add( newdata+unwritten, i-unwritten, outf );
        }

#if 0
        if ( i < bytesRead ) {
            /* shift filepointer back so uncompared data isn't missed */
            fseek( newf, (i - bytesRead), SEEK_CUR );
        }
#endif

    } while ( bytesRead > 0 );


    /* Terminate the GDIFF file */

    fwrite( GDIFF_EOF, 1, 1, outf );


bail:
    if ( olddata != NULL )
        free( olddata );
    if (newdata != NULL )
        free( newdata );
}
#endif /* NEWDIFF */


/*--------------------------------------
 *  openFiles
 *------------------------------------*/

int openFiles()
{
    char namebuf[FILENAMESIZE];

    oldfile = fopen( oldname, "rb" );
    if ( oldfile == NULL ) {
        fprintf( stderr, "Can't open %s for reading\n", oldname );
        return ERR_ACCESS;
    }

    newfile = fopen( newname, "rb" );
    if ( newfile == NULL ) {
        fprintf( stderr, "Can't open %s for reading\n", newname );
        return ERR_ACCESS;
    }

    if ( outname == NULL || *outname == '\0' ) {
        strcpy( namebuf, newname );
        strcat( namebuf, ".gdf" );
        outname = namebuf;
    }
    outfile = fopen( outname, "wb" );
    if ( outfile == NULL ) {
        fprintf( stderr, "Can't open %s for writing\n", outname );
        return ERR_ACCESS;
    }

    return ERR_OK;
}


/*--------------------------------------
 *  usage
 *------------------------------------*/

void usage ()
{
    fprintf( stderr, "\n  NSDiff [-b#] [-o\"outfile\"] oldfile newfile\n\n" );
    fprintf( stderr, "       -b   size of blocks to compare in bytes, " \
            "default %d\n", DEFAULT_BLOCKSIZE );
    fprintf( stderr, "       -o   name of output diff file\n\n" );
}


/*--------------------------------------
 *  validateArgs
 *------------------------------------*/

BOOL validateArgs( int argc, char *argv[] )
{
    int i;
    for ( i = 1; i < argc; i++ ) {
        if ( *argv[i] == '-' ) {
            switch (*(argv[i]+1)) {
                case 'b':
                    blocksize = atoi( argv[i]+2 );
                    break;

                case 'o':
                    outname = argv[i]+2;
                    break;

                default:
                    fprintf( stderr, "Unknown option %s\n", argv[i] );
                    return (FALSE);
            }
        }
        else if ( oldname == NULL ) {
            oldname = argv[i];
        }
        else if ( newname == NULL ) {
            newname = argv[i];
        }
        else {
            fprintf( stderr, "Too many arguments\n" );
            return (FALSE);
        }
    }
#ifdef DEBUG
    fprintf( stderr, "Blocksize: %d\n", blocksize );
    fprintf( stderr, "Old file: %s\n", oldname );
    fprintf( stderr, "New file: %s\n", newname );
    fprintf( stderr, "diff file: %s\n", outname );
#endif

    /* validate arguments */

    if ( blocksize <= IGNORE_SIZE || blocksize > MAXBUFSIZE ) {
        fprintf( stderr, "Invalid blocksize: %d\n", blocksize );
        return (FALSE);
    }

    if ( oldname == NULL || newname == NULL ) {
        fprintf( stderr, "Old and new file name parameters are required.\n" );
        return (FALSE);
    }

    return (TRUE);
}


/*--------------------------------------
 *  writeHeader
 *------------------------------------*/

void writeHeader( FILE *outf )
{
    fwrite( "\xd1\xff\xd1\xff\05\0\0\0\0\0\0", 1, 11, outf );
}

