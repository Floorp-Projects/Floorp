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
 *  NSPatch
 *
 *  Applies GDIFF binary patches
 *
 *       nspatch [-o"outfile"] oldfile newfile
 *
 *  The outfile defaults console
 *
 *------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*--------------------------------------
 *  constants / macros
 *------------------------------------*/

#define BUFSIZE     32768
#define OPSIZE      1
#define MAXCMDSIZE  12

#define getshort(s) (uint16)( ((uchar)*(s) << 8) + ((uchar)*((s)+1)) )

#define getlong(s)  \
            (uint32)( ((uchar)*(s) << 24) + ((uchar)*((s)+1) << 16 ) + \
                      ((uchar)*((s)+2) << 8) + ((uchar)*((s)+3)) )



/*--------------------------------------
 *  types
 *------------------------------------*/

typedef unsigned long   uint32;
typedef long            int32;
typedef unsigned short  uint16;
typedef short           int16;
typedef unsigned char   BOOL;
typedef unsigned char   XP_Bool;
typedef unsigned char   uint8;
typedef FILE*           XP_File;


#include "gdiff.h"


/*--------------------------------------
 *  prototypes
 *------------------------------------*/

int     add( uint32 count );
void    cleanup( void );
int     copy( uint32 position, uint32 count );
int     doPatch( void );
int     getcmd( uchar *buffer, uint32 length );
char    openFiles( void );
int     parseHeader( void );
int     parseAppdata( void );
void    usage( void );
char    validateArgs( int argc, char *argv[] );
int     validateNewFile( void );
int     validateOldFile( void );




/*--------------------------------------
 *  Global data
 *------------------------------------*/

char    *oldname = NULL,
        *difname = NULL,
        *outname = NULL;

uchar   *databuf;

FILE    *oldfile,
        *diffile,
        *outfile;

uchar   checksumType;

uchar   *oldChecksum = NULL,
        *finalChecksum = NULL;



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


    /* Process the patchfile */

    if ( err == GDIFF_OK ) {
        err = doPatch();
    }


    /* Cleanup */

    cleanup();


    /* Report status */

    if ( err == GDIFF_OK ) {

    }
    else {
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
                fprintf( stderr, "Unexpected error %d\n", err );
                break;
        }
    }

    return (err);
}



/*--------------------------------------
 *  add
 *------------------------------------*/

int add( uint32 count )
{
    int     err = GDIFF_OK;
    uint32  nRead;
    uint32  chunksize;

    while ( count > 0 ) {
        chunksize = ( count > BUFSIZE) ? BUFSIZE : count;
        nRead = fread( databuf, 1, chunksize, diffile );
        if ( nRead != chunksize ) {
            err = GDIFF_ERR_BADDIFF;
            break;
        }

        fwrite( databuf, 1, chunksize, outfile );

        count -= chunksize;
    }

    return (err);
}



/*--------------------------------------
 *  cleanup
 *------------------------------------*/

void cleanup()
{
    if ( oldfile != NULL )
        fclose( oldfile );

    if ( diffile != NULL )
        fclose( diffile );

    if ( outfile != NULL /* && outfile != stdout */ )
        fclose( outfile );

    if ( oldChecksum != NULL )
        free( oldChecksum );

    if ( finalChecksum != NULL )
        free( finalChecksum );

}



/*--------------------------------------
 *  copy
 *------------------------------------*/

int copy( uint32 position, uint32 count )
{
    int err = GDIFF_OK;
    uint32 nRead;
    uint32 chunksize;

    fseek( oldfile, position, SEEK_SET );

    while ( count > 0 ) {
        chunksize = (count > BUFSIZE) ? BUFSIZE : count;

        nRead = fread( databuf, 1, chunksize, oldfile );
        if ( nRead != chunksize ) {
            err = GDIFF_ERR_OLDFILE;
            break;
        }

        fwrite( databuf, 1, chunksize, outfile );

        count -= chunksize;
    }

    return (err);
}



/*--------------------------------------
 *  doPatch
 *------------------------------------*/

int doPatch()
{
    int     err;
    int     done;
    uchar   opcode;
    uchar   cmdbuf[MAXCMDSIZE];

    databuf = (uchar*)malloc(BUFSIZE);
    if ( databuf == NULL ) {
        err = GDIFF_ERR_MEM;
        goto fail;
    }

    err = parseHeader();
    if (err != GDIFF_OK)
        goto fail;

    err = validateOldFile();
    if ( err != GDIFF_OK )
        goto fail;

    err = parseAppdata();
    if ( err != GDIFF_OK )
        goto fail;


    /* apply patch */

    done = feof(diffile);
    while ( !done ) {
        err = getcmd( &opcode, OPSIZE );
        if ( err != GDIFF_OK )
            break;

        switch (opcode)
        {
            case ENDDIFF:
                done = TRUE;
                break;

            case ADD16:
                err = getcmd( cmdbuf, ADD16SIZE );
                if ( err == GDIFF_OK ) {
                    err = add( getshort( cmdbuf ) );
                }
                break;

            case ADD32:
                err = getcmd( cmdbuf, ADD32SIZE );
                if ( err == GDIFF_OK ) {
                    err = add( getlong( cmdbuf ) );
                }
                break;

            case COPY16BYTE:
                err = getcmd( cmdbuf, COPY16BYTESIZE );
                if ( err == GDIFF_OK ) {
                    err = copy( getshort(cmdbuf), *(cmdbuf+sizeof(short)) );
                }
                break;

            case COPY16SHORT:
                err = getcmd( cmdbuf, COPY16SHORTSIZE );
                if ( err == GDIFF_OK ) {
                    err = copy(getshort(cmdbuf),getshort(cmdbuf+sizeof(short)));
                }
                break;

            case COPY16LONG:
                err = getcmd( cmdbuf, COPY16LONGSIZE );
                if ( err == GDIFF_OK ) {
                    err = copy(getshort(cmdbuf),getlong(cmdbuf+sizeof(short)));
                }
                break;

            case COPY32BYTE:
                err = getcmd( cmdbuf, COPY32BYTESIZE );
                if ( err == GDIFF_OK ) {
                    err = copy( getlong(cmdbuf), *(cmdbuf+sizeof(long)) );
                }
                break;

            case COPY32SHORT:
                err = getcmd( cmdbuf, COPY32SHORTSIZE );
                if ( err == GDIFF_OK ) {
                    err = copy( getlong(cmdbuf),getshort(cmdbuf+sizeof(long)) );
                }
                break;

            case COPY32LONG:
                err = getcmd( cmdbuf, COPY32LONGSIZE );
                if ( err == GDIFF_OK ) {
                    err = copy( getlong(cmdbuf), getlong(cmdbuf+sizeof(long)) );
                }
                break;

            case COPY64:
                /* we don't support 64-bit file positioning yet */
                err = GDIFF_ERR_OPCODE;
                break;

            default:
                err = add( opcode );
                break;
        }

        if ( err != GDIFF_OK )
            done = TRUE;
    }

    if ( err == GDIFF_OK ) {
        err = validateNewFile();
    }
    

    /* return status */
fail:
    if ( databuf != NULL )
        free( databuf );

    return (err);
}



/*--------------------------------------
 *  getcmd
 *------------------------------------*/
int getcmd( uchar *buffer, uint32 length )
{
    uint32 bytesRead;

    bytesRead = fread( buffer, 1, length, diffile );
    if ( bytesRead != length )
        return GDIFF_ERR_BADDIFF;

    return GDIFF_OK;
}



/*--------------------------------------
 *  openFiles
 *------------------------------------*/

char openFiles()
{
    oldfile = fopen( oldname, "rb" );
    if ( oldfile == NULL ) {
        fprintf( stderr, "Can't open %s for reading\n", oldname );
        return GDIFF_ERR_ACCESS;
    }

    diffile = fopen( difname, "rb" );
    if ( diffile == NULL ) {
        fprintf( stderr, "Can't open %s for reading\n", difname );
        return GDIFF_ERR_ACCESS;
    }

    if ( outname == NULL || *outname == '\0' ) {
        outfile = stdout;
    }
    else {
        outfile = fopen( outname, "wb" );
    }
    if ( outfile == NULL ) {
        fprintf( stderr, "Can't open %s for writing\n", outname );
        return GDIFF_ERR_ACCESS;
    }

    return GDIFF_OK;
}



/*--------------------------------------
 *  parseHeader
 *------------------------------------*/

int     parseHeader( void )
{
    int     err = GDIFF_OK;
    uint32  cslen;
    uint32  oldcslen;
    uint32  newcslen;
    uint32  nRead;
    uchar   header[GDIFF_HEADERSIZE];

    nRead = fread( header, 1, GDIFF_HEADERSIZE, diffile );
    if ( nRead != GDIFF_HEADERSIZE ) {
        err = GDIFF_ERR_HEADER;
    }
    else if (memcmp( header, GDIFF_MAGIC, GDIFF_MAGIC_LEN ) != 0 ) {
        err = GDIFF_ERR_HEADER;
    }
    else if ( header[GDIFF_VER_POS] != GDIFF_VER ) {
        err = GDIFF_ERR_HEADER;
    }
    else {
        checksumType = header[GDIFF_CS_POS];
        cslen = header[GDIFF_CSLEN_POS];

        if ( checksumType > 0 ) {
            err = GDIFF_ERR_CHKSUMTYPE;
        }
        else if ( cslen > 0 ) {
            oldcslen = cslen / 2;
            newcslen = cslen - oldcslen;

            oldChecksum = (uchar*)malloc(oldcslen);
            finalChecksum = (uchar*)malloc(newcslen);

            if ( oldChecksum != NULL || finalChecksum != NULL ) {
                nRead = fread( oldChecksum, 1, oldcslen, diffile );
                if ( nRead == oldcslen ) {
                    nRead = fread( finalChecksum, 1, newcslen, diffile );
                    if ( nRead != newcslen ) {
                        err = GDIFF_ERR_HEADER;
                    }
                }
                else {
                    err = GDIFF_ERR_HEADER;
                }
            }
            else {
                err = GDIFF_ERR_MEM;
            }
        }
    }

    return (err);

}



/*--------------------------------------
 *  parseAppdata
 *------------------------------------*/

int     parseAppdata( void )
{
    int     err = GDIFF_OK;
    uint32  nRead;
    uint32  appdataSize;
    uchar   lenbuf[GDIFF_APPDATALEN];

    nRead = fread( lenbuf, 1, GDIFF_APPDATALEN, diffile );
    if ( nRead != GDIFF_APPDATALEN ) {
        err = GDIFF_ERR_HEADER;
    }
    else {
        appdataSize = getlong(lenbuf);

        if ( appdataSize > 0 ) {
            /* we currently don't know about appdata, so skip it */
            fseek( diffile, appdataSize, SEEK_CUR );
        }
    }

    return (err);
}



/*--------------------------------------
 *  usage
 *------------------------------------*/

void usage ()
{
    fprintf( stderr, "\n  NSPatch [-o\"outfile\"] oldfile diff\n\n" );
    fprintf( stderr, "       -o   name of patched output file\n\n" );
}


/*--------------------------------------
 *  validateArgs
 *------------------------------------*/

char validateArgs( int argc, char *argv[] )
{
    int i;
    for ( i = 1; i < argc; i++ ) {
        if ( *argv[i] == '-' ) {
            switch (*(argv[i]+1)) {
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
        else if ( difname == NULL ) {
            difname = argv[i];
        }
        else {
            fprintf( stderr, "Too many arguments\n" );
            return (FALSE);
        }
    }
#ifdef DEBUG
    fprintf( stderr, "Old file: %s\n", oldname );
    fprintf( stderr, "Diff file: %s\n", difname );
    fprintf( stderr, "output file: %s\n", outname );
#endif

    /* validate arguments */

    if ( oldname == NULL || difname == NULL ) {
        fprintf( stderr, "Old and diff file name parameters are required.\n" );
        return (FALSE);
    }

    return TRUE;
}



/*--------------------------------------
 *  validateNewFile
 *------------------------------------*/
int validateNewFile()
{
    if ( checksumType > 0 )
        return GDIFF_ERR_CHKSUMTYPE;
    else
        return GDIFF_OK;
}


/*--------------------------------------
 *  validateOldFile
 *------------------------------------*/
int validateOldFile()
{
    if ( checksumType > 0 )
        return GDIFF_ERR_CHKSUMTYPE;
    else
        return GDIFF_OK;
}

