/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/*--------------------------------------------------------------------
 *  NSDiff
 *
 *  A binary file diffing utility that creates diffs in GDIFF format
 *
 *       nsdiff [-b#] [-d] [-c#] [-o"outfile"] oldfile newfile
 *
 *  Blocksize defaults to 64
 *  The outfile defaults to "newfile" with a .GDF extension
 *
 *------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "zlib.h"

/*--------------------------------------
 *  types
 *------------------------------------*/

#if 0
/* zlib.h appears to define these now. If it stops remove the #if */
typedef unsigned long   uint32;
typedef long            int32;
typedef unsigned short  uint16;
typedef short           int16;
typedef unsigned char   uint8;
#endif
typedef unsigned char   XP_Bool;
typedef FILE*           XP_File;


#include "gdiff.h"


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
    XP_Bool bDoAdds;        /* If true check for "add" data before reading */
} FILEDATA;


/*--------------------------------------
 *  constants
 *------------------------------------*/

#define IGNORE_SIZE         8
#define DEFAULT_BLOCKSIZE   64
#define FILENAMESIZE        513
#define OLDBUFSIZE          0x1FFF
#define MAXBUFSIZE          0xFFFF  /* more requires long adds, wasting space */
#define CHKMOD              65536
#define HTBLSIZE            65521

#define destroyFdata(fd)    do { if ((fd)->data != NULL) free((fd)->data); } while (0)

#define writelong(n,buf)   do { \
            *((buf))   = (uchar)( (n) >> 24 );                        \
            *((buf)+1) = (uchar)( ( (n) >> 16 ) & 0x00FF );           \
            *((buf)+2) = (uchar)( ( (n) >> 8 ) & 0x00FF );            \
            *((buf)+3) = (uchar)( (n) & 0x00FF ); } while(0)



/*--------------------------------------
 *  prototypes
 *------------------------------------*/

void    add( uchar *data, uint32 count, FILE *outf );
void    adddata( FILEDATA *fd );
void    calchash( uint8 type, FILE *fh, uchar *outbuf, int *outlen );
void    copy( uint32 offset, uint32 count, FILE *outf );
uint32  checksum( uchar *buffer, uint32 offset, uint32 count );
HPTR    *chunkOldFile( FILE *fh, uint32 blocksize );
void    cleanup( void );
uint32  compareFdata( FILEDATA *oldfd, uint32 oldpos, FILEDATA *newfd );
void    diff( FILE *oldf, FILE *newf, FILE *outf );
uint32  getChecksum( FILEDATA *fd );
XP_Bool getFdata( FILEDATA *fdata, uint32 position );
XP_Bool initFdata( FILEDATA *fdata, FILE *file, uint32 bufsize, XP_Bool bDoAdds );
XP_Bool moreFdata( FILEDATA *fdata );
int     openFiles( void );
void    readFdata( FILEDATA *fdata, uint32 position );
void    usage( void );
XP_Bool validateArgs( int argc, char *argv[] );
void    writeHeader( FILE *oldf, FILE *newf, FILE *outf );

#ifdef XP_PC
XP_Bool unbind( FILE *oldf );
XP_Bool unbindFile( char *name );
XP_Bool hasImports( FILE *oldf );
#endif


/*--------------------------------------
 *  Global data
 *------------------------------------*/

XP_Bool bDebug = FALSE;

char    *oldname = NULL,
        *newname = NULL,
        *outname = NULL;

FILE    *oldfile,
        *newfile,
        *outfile;

uint32  blocksize  = DEFAULT_BLOCKSIZE;
uint8   chksumtype = GDIFF_CS_CRC32;


HPTR    *htbl;

uint32  addtotal = 0,
        addmax   = 0,
        addnum   = 0,
        copymax  = 0,
        copynum  = 0,
        copytotal = 0;

  /* for hashing stats */
uint32  slotsused = 0,
        blockshashed = 0,
        blocksfound = 0,
        hashhits = 0,
        partialmatch = 0,
        cmpfailed = 0;

#ifdef XP_PC
XP_Bool bUnbind = TRUE;
#endif /* XP_PC */


/*--------------------------------------
 *  main
 *------------------------------------*/

int main( int argc, char *argv[] )
{
    int err;

    /* Parse command line */

    if ( !validateArgs( argc, argv ) ) {
        err = GDIFF_ERR_ARGS;
    }
    else {
        err = openFiles();
    }

#ifdef XP_PC
    /* unbind windows executables if requested */
    if ( bUnbind )
        bUnbind = (err == GDIFF_OK) ? unbind( oldfile ) : FALSE;
#endif /* XP_PC */

    /* Calculate block checksums in old file */

    if ( err == GDIFF_OK ) {
        htbl = chunkOldFile( oldfile, blocksize );
        if ( htbl == NULL )
            err = GDIFF_ERR_MEM;
    }


    /* Do the diff */

    if ( err == GDIFF_OK ) {
        writeHeader( oldfile, newfile, outfile );
        diff( oldfile, newfile, outfile );
    }


    /* Cleanup */

    cleanup();


    /* Report status */

    if ( err != GDIFF_OK ) {
        switch (err) {
            case GDIFF_ERR_ARGS:
                fprintf( stderr, "Invalid arguments\n" );
                usage();
                break;

            case GDIFF_ERR_ACCESS:
                fprintf( stderr, "Error opening file\n" );
                break;

            case GDIFF_ERR_MEM:
                fprintf( stderr, "Insufficient memory\n" );
                break;

            default:
                fprintf( stderr, "Unknown error %d\n", err );
                break;
        }
    }
    else if ( bDebug ) {
        fprintf( stderr, "\nHashed %ld blocks into %ld slots (out of %ld)\n",
            blockshashed, slotsused, HTBLSIZE );
        fprintf( stderr, "Blocks found: %ld\n", blocksfound );
        fprintf( stderr, "Hashtable hits: %ld\n", hashhits );
        fprintf( stderr, "memcmps failed: %ld\n", cmpfailed );
        fprintf( stderr, "partial matches: %ld\n", partialmatch );
        fprintf( stderr, "\n%ld bytes in %ld ADDs (%ld in largest)\n", addtotal, addnum, addmax );
        fprintf( stderr, "%ld bytes in %ld COPYs (%ld in largest)\n", copytotal, copynum, copymax );
    }

    return (err);
}


/*--------------------------------------
 *  add
 *------------------------------------*/

void add( uchar *data, uint32 count, FILE *outf )
{
    uchar numbuf[5];

    addnum++;
    addtotal += count;
    if ( count > addmax )
        addmax = count;

#ifdef DEBUG
    if ( bDebug ) {
        fprintf( stderr, "Adding %ld bytes\n", count );
    }
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

    copynum++;
    copytotal += count;
    if ( count > copymax )
        copymax = count;

#ifdef DEBUG
    if ( bDebug ) {
        fprintf( stderr, "Copying %ld bytes from offset %ld\n", count, offset );
    }
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
    if ( bDebug ) {
        fprintf( stderr, "bufsize: %ld, tblsize: %ld\n", bufsize, HTBLSIZE*sizeof(HPTR) );
    }
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
#ifdef XP_PC
    if ( bUnbind )
        unlink( oldname );
#endif
}



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


XP_Bool initFdata( FILEDATA *fdata, FILE *file, uint32 bufsize, XP_Bool bDoAdds )
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

XP_Bool moreFdata( FILEDATA *fd )
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

XP_Bool getFdata( FILEDATA *fd, uint32 position )
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



/*--------------------------------------
 *  openFiles
 *------------------------------------*/

int openFiles()
{
    char namebuf[FILENAMESIZE];

    oldfile = fopen( oldname, "rb" );
    if ( oldfile == NULL ) {
        fprintf( stderr, "Can't open %s for reading\n", oldname );
        return GDIFF_ERR_ACCESS;
    }

    newfile = fopen( newname, "rb" );
    if ( newfile == NULL ) {
        fprintf( stderr, "Can't open %s for reading\n", newname );
        return GDIFF_ERR_ACCESS;
    }

    if ( outname == NULL || *outname == '\0' ) {
        strcpy( namebuf, newname );
        strcat( namebuf, ".gdf" );
        outname = namebuf;
    }
    outfile = fopen( outname, "wb" );
    if ( outfile == NULL ) {
        fprintf( stderr, "Can't open %s for writing\n", outname );
        return GDIFF_ERR_ACCESS;
    }

    return GDIFF_OK;
}


/*--------------------------------------
 *  usage
 *------------------------------------*/

void usage ()
{
    fprintf( stderr, "\n  NSDiff [-b#] [-o\"outfile\"] oldfile newfile\n\n" );
    fprintf( stderr, "       -b   size of blocks to compare in bytes, " \
            "default %d\n", DEFAULT_BLOCKSIZE );
    fprintf( stderr, "       -c#  checksum type\n");
    fprintf( stderr, "            1:  MD5 (not supported yet)\n");
    fprintf( stderr, "            2:  SHA (not supported yet)\n");
    fprintf( stderr, "            32: CRC-32 (default)\n");
    fprintf( stderr, "       -d   diagnostic output\n" );
    fprintf( stderr, "       -o   name of output diff file\n" );
#ifdef XP_PC
    fprintf( stderr, "       -wb- turn off special handling for Win32 bound images\n" );
#endif
    fprintf( stderr, "\n" );

}


/*--------------------------------------
 *  validateArgs
 *------------------------------------*/

XP_Bool validateArgs( int argc, char *argv[] )
{
    int i;
    for ( i = 1; i < argc; i++ ) {
        if ( *argv[i] == '-' ) {
            switch (*(argv[i]+1)) {
                case 'b':
                    blocksize = atoi( argv[i]+2 );
                    break;

                case 'c':
                    chksumtype = atoi( argv[i]+2 );
                    break;

                case 'o':
                    outname = argv[i]+2;
                    break;

                case 'd':
                    bDebug = TRUE;
                    break;

#ifdef XP_PC
                /* windows only option. -wb to set-up for bound
                 * images (the default) -wb- to turn it off     */
                case 'w':
                    if ( *(argv[i]+2) == 'b' ) {
                        if ( *(argv[i]+3) == '-' )
                            bUnbind = FALSE;
                        else
                            bUnbind = TRUE;
                    }
                    else {
                        fprintf( stderr, "Unknown windows option %s\n", argv[i] );
                        return (FALSE);
                    }
#endif /* XP_PC */

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

void writeHeader( FILE *oldf, FILE *newf, FILE *outf )
{
    uchar databuf[512];
    int   datalen;
    int   oldlen, newlen, applen;
    uchar *plen;

    /* magic number and version */
    memcpy( databuf, "\xd1\xff\xd1\xff\05", 5 );
    datalen = 5;


    /* calculate and store checksum info */
    databuf[datalen] = chksumtype;
    datalen++;

    databuf[datalen] = 0; /* initial checksum length */
    plen = databuf+datalen;
    datalen++;

    calchash( chksumtype, oldf, databuf+datalen, &oldlen );
    datalen += oldlen;

    calchash( chksumtype, newf, databuf+datalen, &newlen );
    datalen += newlen;

    *plen = oldlen+newlen;

    
    /* application-specific data */
    applen = 0;
#ifdef XP_PC
    if ( bUnbind ) {
        strcpy( databuf+datalen+4, APPFLAG_W32BOUND );
        applen = strlen( APPFLAG_W32BOUND ) + 1;
    }
#endif /* XP_PC */
    writelong( applen, databuf+datalen );
    datalen += (applen + 4);


    /* write out the header */
    fwrite( databuf, 1, datalen, outf );
}


/*------------------------------------------
 *  calchash
 *----------------------------------------*/
#define BUFSIZE  8192

void calchash( uint8 type, FILE *fh, uchar *outbuf, int *outlen )
{
    uchar buffer[BUFSIZE];
    switch (type) 
    {
        case GDIFF_CS_CRC32:
        {
            uint32 crc;
            uint32 nRead;

            fseek( fh, 0, SEEK_SET );
            crc = crc32( 0L, Z_NULL, 0 );

            nRead = fread( buffer, 1, BUFSIZE, fh );
            while ( nRead > 0 ) {
                crc = crc32( crc, buffer, nRead );
                nRead = fread( buffer, 1, BUFSIZE, fh );
            }


            fseek( fh, 0, SEEK_SET );

            *(outbuf)   = (uchar)( crc >> 24 );
            *(outbuf+1) = (uchar)( ( crc >> 16 ) & 0x00FF );
            *(outbuf+2) = (uchar)( ( crc >> 8 ) & 0x00FF );
            *(outbuf+3) = (uchar)( crc & 0x00FF );

            *outlen = 4;

            break;
        }


        case GDIFF_CS_NONE:
        default:
            *outlen = 0;
            break;
    }
}


#ifdef XP_PC

#include <sys/types.h>
#include <sys/stat.h>
#include <windows.h>
#include <winnt.h>

XP_Bool unbind( FILE *oldf )
{
    int             i;
    struct _stat    st;
    char            filename[MAX_PATH];
    XP_Bool         unbound = FALSE;

    if ( hasImports( oldf ) ) 
    {
        strcpy( filename, oldname );
        for ( i = strlen(filename); i > 0; i-- ) {
            filename[i-1] = '~';
            if ( _stat( filename, &st ) != 0 )
                break;
        }

        if ( CopyFile( oldname, filename, FALSE ) ) {
            strcpy( oldname, filename ); /* save new name */
            if ( unbindFile( oldname ) ) {
                oldfile = fopen( oldname, "rb" );
                unbound = TRUE;
            }
        }
    }

    fseek( oldfile, 0, SEEK_SET );
    return unbound;
}



XP_Bool hasImports( FILE *fh )
{
    int     i;
    DWORD   nRead;
    IMAGE_DOS_HEADER            mz;
    IMAGE_NT_HEADERS            nt;
    IMAGE_SECTION_HEADER        sec;

    /* read and validate the MZ header */
    fseek( fh, 0, SEEK_SET );
    nRead = fread( &mz, 1, sizeof(mz), fh );
    if ( nRead != sizeof(mz) || mz.e_magic != IMAGE_DOS_SIGNATURE )
        goto bail;

    /* read and validate the NT header */
    fseek( fh, mz.e_lfanew, SEEK_SET );
    nRead = fread( &nt, 1, sizeof(nt), fh );
    if ( nRead != sizeof(nt) || 
         nt.Signature != IMAGE_NT_SIGNATURE ||
         nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC ) 
    {
        goto bail;
    }

    /* find .idata section */
    for (i = nt.FileHeader.NumberOfSections; i > 0; i--) {
        nRead = fread( &sec, 1, sizeof(sec), fh );
        if ( nRead != sizeof(sec) ) 
            goto bail;

        if ( memcmp( sec.Name, ".idata", 6 ) == 0 ) {
            /* found imports! */
            return TRUE;
        }
    }

bail:   
    /* not a Win32 PE image or no import section */
    return FALSE;
}


XP_Bool unbindFile( char* fname )
{
    int     i;
    DWORD   nRead;
    PDWORD  pOrigThunk;
    PDWORD  pBoundThunk;
    FILE    *fh;
    char    *buf;
    BOOL    bModified = FALSE;
    BOOL    bImports = FALSE;

    IMAGE_DOS_HEADER            mz;
    IMAGE_NT_HEADERS            nt;
    IMAGE_SECTION_HEADER        sec;

    PIMAGE_DATA_DIRECTORY       pDir;
    PIMAGE_IMPORT_DESCRIPTOR    pImp;

    typedef BOOL (__stdcall *BINDIMAGEEX)(DWORD Flags,
									  LPSTR ImageName,
									  LPSTR DllPath,
									  LPSTR SymbolPath,
									  PVOID StatusRoutine);
    HINSTANCE   hImageHelp;
    BINDIMAGEEX pfnBindImageEx;


    /* call BindImage() first to make maximum room for a possible
     * NT-style Bound Import Descriptors which can change various
     * offsets in the file */
	hImageHelp = LoadLibrary("IMAGEHLP.DLL");
	if ( hImageHelp > (HINSTANCE)HINSTANCE_ERROR ) {
    	pfnBindImageEx = (BINDIMAGEEX)GetProcAddress(hImageHelp, "BindImageEx");
    	if (pfnBindImageEx) {
            if (!pfnBindImageEx(0, fname, NULL, NULL, NULL))
                fprintf( stderr, "    ERROR: Pre-binding failed\n" );
        }
        else {
            fprintf( stderr, "    ERROR: Couldn't find BindImageEx()\n" );
        }
		FreeLibrary(hImageHelp);
	}
    else {
        fprintf( stderr, "    ERROR: Could not load ImageHlp.dll\n" );
        goto bail;
    }
    

    fh = fopen( fname, "r+b" );
    if ( fh == NULL ) {
        fprintf( stderr, "    ERROR: Couldn't open %s\n", fname );
        goto bail;
    }


    /* read and validate the MZ header */

    nRead = fread( &mz, 1, sizeof(mz), fh );
    if ( nRead != sizeof(mz) ) {
        fprintf( stderr, "    ERROR: Unexpected EOF reading DOS header\n" );
        goto bail;
    }
    else if ( mz.e_magic != IMAGE_DOS_SIGNATURE ) {
        fprintf( stderr, "    ERROR: Invalid DOS header\n" );
        goto bail;
    }



    /* read and validate the NT header */

    fseek( fh, mz.e_lfanew, SEEK_SET );
    nRead = fread( &nt, 1, sizeof(nt), fh );
    if ( nRead != sizeof(nt) ) {
        fprintf( stderr, "    ERROR: Unexpected EOF reading PE headers\n" );
        goto bail;
    }

    if ( nt.Signature != IMAGE_NT_SIGNATURE ) {
        fprintf( stderr, "    ERROR: Not a Win32 Portable Executable (PE) file\n" );
        goto bail;
    }
    else if ( nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC ) {
        fprintf( stderr, "    ERROR: Invalid PE Optional Header\n" );
        goto bail;
    }



    /* find .idata section */

    for (i = nt.FileHeader.NumberOfSections; i > 0; i--)
    {
        nRead = fread( &sec, 1, sizeof(sec), fh );
        if ( nRead != sizeof(sec) ) {
            fprintf( stderr, "    ERROR: EOF reading section headers\n" );
            goto bail;
        }

        if ( memcmp( sec.Name, ".idata", 6 ) == 0 ) {
            bImports = TRUE;
            break;
        }
    }



    /* Zap any binding in the imports section */

    if ( bImports ) 
    {
        buf = (char*)malloc( sec.SizeOfRawData );
        if ( buf == NULL ) {
            fprintf( stderr, "    ERROR: Memory allocation problem\n" );
            goto bail;
        }

        fseek( fh, sec.PointerToRawData, SEEK_SET );
        nRead = fread( buf, 1, sec.SizeOfRawData, fh );
        if ( nRead != sec.SizeOfRawData ) {
            fprintf( stderr, "    ERROR: Unexpected EOF reading .idata\n" );
            goto bail;
        }

        pImp = (PIMAGE_IMPORT_DESCRIPTOR)buf;
        while ( pImp->OriginalFirstThunk != 0 )
        {
            if ( pImp->TimeDateStamp != 0 || pImp->ForwarderChain != 0 )
            {
                /* found a bound .DLL */
                pImp->TimeDateStamp = 0;
                pImp->ForwarderChain = 0;
                bModified = TRUE;

                pOrigThunk = (PDWORD)(buf + (DWORD)(pImp->OriginalFirstThunk) - sec.VirtualAddress);
                pBoundThunk = (PDWORD)(buf + (DWORD)(pImp->FirstThunk) - sec.VirtualAddress);

                for ( ; *pOrigThunk != 0; pOrigThunk++, pBoundThunk++ ) {
                    *pBoundThunk = *pOrigThunk;
                }

                fprintf( stdout, "    %s bindings removed\n",
                    buf + (pImp->Name - sec.VirtualAddress) );
            }

            pImp++;
        }

        if ( bModified ) 
        {
            /* it's been changed, write out the section */
            fseek( fh, sec.PointerToRawData, SEEK_SET );
            fwrite( buf, 1, sec.SizeOfRawData, fh );
        }

        free( buf );
    }



    /* Check for a Bound Import Directory in the headers */
    
    pDir = &nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT];
    if ( pDir->VirtualAddress != 0 ) 
    {
        /* we've got one, so stomp it */
        fprintf( stdout, "    Deleting NT Bound Import Directory\n" );

        buf = (char*)calloc( pDir->Size, 1 );
        if ( buf == NULL ) {
            fprintf( stderr, "    ERROR: Memory allocation problem\n" );
            goto bail;
        }

        fseek( fh, pDir->VirtualAddress, SEEK_SET );
        fwrite( buf, pDir->Size, 1, fh );
        free( buf );

        pDir->VirtualAddress = 0;
        pDir->Size = 0;

        bModified = TRUE;
    }



    /* Write out changed headers if necessary */
    
    if ( bModified )
    {
        /* zap checksum since it's now invalid */
        nt.OptionalHeader.CheckSum = 0;

        fseek( fh, mz.e_lfanew, SEEK_SET );
        fwrite( &nt, 1, sizeof(nt), fh );
    }

    fclose(fh);
    return TRUE;

bail:
    fclose(fh);
    return FALSE;
}

#endif
