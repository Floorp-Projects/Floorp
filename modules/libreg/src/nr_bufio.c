/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *     Daniel Veditz <dveditz@netscape.com>
 *     Edward Kandrot <kandrot@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/*------------------------------------------------------------------------
 * nr_bufio
 * 
 * Buffered I/O routines to improve registry performance
 * the routines mirror fopen(), fclose() et al
 *
 * Inspired by the performance gains James L. Nance <jim_nance@yahoo.com>
 * got using NSPR memory-mapped I/O for the registry. Unfortunately NSPR 
 * doesn't support mmapio on the Mac.
 *-----------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#if defined(XP_MAC)
  #include <Errors.h>
#endif

#if defined(SUNOS4)
#include <unistd.h>  /* for SEEK_SET */
#endif /* SUNOS4 */

#include "prerror.h"
#include "prlog.h"

#include "vr_stubs.h"
#include "nr_bufio.h"


#define BUFIO_BUFSIZE_DEFAULT   0x10000

#define STARTS_IN_BUF(f) ((f->fpos >= f->datastart) && \
                         (f->fpos < (f->datastart+f->datasize)))

#define ENDS_IN_BUF(f,c) (((f->fpos + c) > (PRUint32)f->datastart) && \
                         ((f->fpos + c) <= (PRUint32)(f->datastart+f->datasize)))

#if DEBUG_dougt
static num_reads = 0;
#endif


struct BufioFileStruct 
{
    FILE    *fd;        /* real file descriptor */
    PRInt32 fsize;      /* total size of file */
    PRInt32 fpos;       /* our logical position in the file */
    PRInt32 datastart;  /* the file position at which the buffer starts */
    PRInt32 datasize;   /* the amount of data actually in the buffer*/
    PRInt32 bufsize;	/* size of the in memory buffer */
    PRBool  bufdirty;   /* whether the buffer been written to */
    PRInt32 dirtystart;
    PRInt32 dirtyend;
    PRBool  readOnly;   /* whether the file allows writing or not */
#ifdef DEBUG_dveditzbuf
    PRUint32 reads;
    PRUint32 writes;
#endif
    char    *data;      /* the data buffer */
};


static PRBool _bufio_loadBuf( BufioFile* file, PRUint32 count );
static int    _bufio_flushBuf( BufioFile* file );



/** 
 * like fopen() this routine takes *native* filenames, not NSPR names.
 */
BufioFile*  bufio_Open(const char* name, const char* mode)
{
    FILE        *fd;
    BufioFile   *file = NULL;

    fd = fopen( name, mode );
    
    if ( fd )
    {
        /* file opened successfully, initialize the bufio structure */

        file = PR_NEWZAP( BufioFile );
        if ( file )
        {
            file->fd = fd;
            file->bufsize = BUFIO_BUFSIZE_DEFAULT;  /* set the default buffer size */

            file->data = (char*)PR_Malloc( file->bufsize );
            if ( file->data )
            {
                /* get file size to finish initialization of bufio */
                if ( !fseek( fd, 0, SEEK_END ) )
                {
                    file->fsize = ftell( fd );

                    file->readOnly = strcmp(mode,XP_FILE_READ) == 0 || 
                                     strcmp(mode,XP_FILE_READ_BIN) == 0;
                }
                else
                {
                    PR_Free( file->data );
                    PR_DELETE( file );
                }
            }
            else
                PR_DELETE( file );
        }

        /* close file if we couldn't create BufioFile */
        if (!file)
        {
            fclose( fd );
            PR_SetError( PR_OUT_OF_MEMORY_ERROR, 0 );
        }
    }
    else
    {
        /* couldn't open file. Figure out why and set NSPR errors */
        
        switch (errno)
        {
            /* file not found */
#ifdef XP_MAC
            case fnfErr:
#else
            case ENOENT:
#endif
                PR_SetError(PR_FILE_NOT_FOUND_ERROR,0);
                break;

            /* file in use */
#ifdef XP_MAC
            case opWrErr:
#else
            case EACCES:
#endif
                PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR,0);
                break;

            default:
                PR_SetError(PR_UNKNOWN_ERROR,0);
                break;
        }
    }

    return file;
}



/**
 * close the buffered file and destroy BufioFile struct
 */
int bufio_Close(BufioFile* file)
{
    int retval = -1;

    if ( file )
    {
        if ( file->bufdirty )
            _bufio_flushBuf( file );

        retval = fclose( file->fd );

        if ( file->data )
            PR_Free( file->data );

        PR_DELETE( file );
#if DEBUG_dougt
        printf(" --- > Buffered registry read fs hits (%d)\n", num_reads);
#endif
    }

    return retval;
}



/**
 * change the logical position in the file. Equivalent to fseek()
 */
int bufio_Seek(BufioFile* file, PRInt32 offset, int whence)
{
    if (!file)
        return -1;

    switch(whence) 
    {
      case SEEK_SET:
	    file->fpos = offset;
	    break;
	  case SEEK_END:
	    file->fpos = file->fsize + offset;
	    break;
	  case SEEK_CUR:
	    file->fpos = file->fpos + offset;
	    break;
	  default:
	    return -1;
    }

    if ( file->fpos < 0 ) 
        file->fpos = 0;

    return 0;
}



/**
 * like ftell() returns the current position in the file, or -1 for error
 */
PRInt32 bufio_Tell(BufioFile* file)
{
    if (file)
        return file->fpos;
    else
        return -1;
}



PRUint32 bufio_Read(BufioFile* file, char* dest, PRUint32 count)
{
    PRInt32     startOffset;
    PRInt32     endOffset;
    PRInt32     leftover;
    PRUint32    bytesCopied;
    PRUint32    bytesRead;
    PRUint32    retcount = 0;

    /* sanity check arguments */
    if ( !file || !dest || count == 0 || file->fpos >= file->fsize )
        return 0;

    /* Adjust amount to read if we're near EOF */
    if ( (file->fpos + count) > (PRUint32)file->fsize )
        count = file->fsize - file->fpos;


    /* figure out how much of the data we want is already buffered */

    startOffset = file->fpos - file->datastart;
    endOffset = startOffset + count;

    if ( startOffset >= 0 && startOffset < file->datasize )
    {
        /* The beginning of what we want is in the buffer  */
        /* so copy as much as is available of what we want */

        if ( endOffset <= file->datasize )
            bytesCopied = count;
        else
            bytesCopied = file->datasize - startOffset;

        memcpy( dest, file->data + startOffset, bytesCopied );
        retcount = bytesCopied;
        file->fpos += bytesCopied;
#ifdef DEBUG_dveditzbuf
        file->reads++;
#endif

        /* Was that all we wanted, or do we need to get more? */

        leftover = count - bytesCopied;
        PR_ASSERT( leftover >= 0 );     /* negative left? something's wrong! */

        if ( leftover )
        {
            /* need data that's not in the buffer */

            /* if what's left fits in a buffer then load the buffer with the */
            /* new area before giving the data, otherwise just read right    */
            /* into the user's dest buffer */

            if ( _bufio_loadBuf( file, leftover ) )
            {
                startOffset = file->fpos - file->datastart;

                /* we may not have been able to load as much as we wanted */
                if ( startOffset > file->datasize )
                    bytesRead = 0;
                else if ( startOffset+leftover <= file->datasize )
                    bytesRead = leftover;
                else
                    bytesRead = file->datasize - startOffset;

                if ( bytesRead )
                {
                    memcpy( dest+bytesCopied, file->data+startOffset, bytesRead );
                    file->fpos += bytesRead;
                    retcount += bytesRead;
#ifdef DEBUG_dveditzbuf
                    file->reads++;
#endif
                }
            }
            else 
            {
                /* we need more than we could load into a buffer, so */
                /* skip buffering and just read the data directly    */

                if ( fseek( file->fd, file->fpos, SEEK_SET ) == 0 )
                {
#if DEBUG_dougt
                    ++num_reads;
#endif
                    bytesRead = fread(dest+bytesCopied, 1, leftover, file->fd);
                    file->fpos += bytesRead;
                    retcount += bytesRead;
                }
                else 
                {
                    /* XXX seek failed, couldn't load more data -- help! */
                    /* should we call PR_SetError() ? */
                }
            }
        }
    }
    else
    {
        /* range doesn't start in the loaded buffer but it might end there */
        if ( endOffset > 0 && endOffset <= file->datasize )
            bytesCopied = endOffset;
        else
            bytesCopied = 0;

        leftover = count - bytesCopied;

        if ( bytesCopied )
        {
            /* the tail end of the range we want is already buffered */
            /* first copy the buffered data to the dest area         */
            memcpy( dest+leftover, file->data, bytesCopied );
#ifdef DEBUG_dveditzbuf
            file->reads++;
#endif
        }
            
        /* now pick up the part that's not already in the buffer */

        if ( _bufio_loadBuf( file, leftover ) )
        {
            /* we were able to load some data */
            startOffset = file->fpos - file->datastart;

            /* we may not have been able to read as much as we wanted */
            if ( startOffset > file->datasize )
                bytesRead = 0;
            else if ( startOffset+leftover <= file->datasize )
                bytesRead = leftover;
            else
                bytesRead = file->datasize - startOffset;

            if ( bytesRead )
            {
                memcpy( dest, file->data+startOffset, bytesRead );
#ifdef DEBUG_dveditzbuf
                file->reads++;
#endif
            }
        }
        else
        {
            /* leftover data doesn't fit so skip buffering */
            if ( fseek( file->fd, file->fpos, SEEK_SET ) == 0 )
            {
                bytesRead = fread(dest, 1, leftover, file->fd);
#if DEBUG_dougt
                ++num_reads;
#endif        
            }
            else
                bytesRead = 0;
        }

        /* if we couldn't read all the leftover, don't tell caller */
        /* about the tail end we copied from the first buffer      */
        if ( bytesRead == (PRUint32)leftover )
            retcount = bytesCopied + bytesRead;
        else
            retcount = bytesRead;

        file->fpos += retcount;
    }

    return retcount;
}



/**
 * Buffered writes
 */
PRUint32 bufio_Write(BufioFile* file, const char* src, PRUint32 count)
{
    const char* newsrc;
    PRInt32  startOffset;
    PRInt32  endOffset;
    PRUint32 leftover;
    PRUint32 retcount = 0;
    PRUint32 bytesWritten = 0;
    PRUint32 bytesCopied = 0;

    /* sanity check arguments */
    if ( !file || !src || count == 0 || file->readOnly )
        return 0;

    /* Write to the current buffer if we can, otherwise load a new buffer */

    startOffset = file->fpos - file->datastart;
    endOffset = startOffset + count;

    if ( startOffset >= 0 && startOffset <  file->bufsize )
    {
        /* the area we want to write starts in the buffer */

        if ( endOffset <= file->bufsize )
            bytesCopied = count;
        else
            bytesCopied = file->bufsize - startOffset;

        memcpy( file->data + startOffset, src, bytesCopied );
        file->bufdirty = PR_TRUE;
        endOffset = startOffset + bytesCopied;
        file->dirtystart = PR_MIN( startOffset, file->dirtystart );
        file->dirtyend   = PR_MAX( endOffset,   file->dirtyend );
#ifdef DEBUG_dveditzbuf
        file->writes++;
#endif

        if ( endOffset > file->datasize )
            file->datasize = endOffset;

        retcount = bytesCopied;
        file->fpos += bytesCopied;

        /* was that all we had to write, or is there more? */
        leftover = count - bytesCopied;
        newsrc = src+bytesCopied;
    }
    else
    {
        /* range doesn't start in the loaded buffer but it might end there */
        if ( endOffset > 0 && endOffset <= file->bufsize )
            bytesCopied = endOffset;
        else
            bytesCopied = 0;

        leftover = count - bytesCopied;
        newsrc = src;

        if ( bytesCopied )
        {
            /* the tail end of the write range is already in the buffer */
            memcpy( file->data, src+leftover, bytesCopied );
            file->bufdirty      = PR_TRUE;
            file->dirtystart    = 0;
            file->dirtyend      = PR_MAX( endOffset, file->dirtyend );
#ifdef DEBUG_dveditzbuf
            file->writes++;
#endif

            if ( endOffset > file->datasize )
                file->datasize = endOffset;
        }
    }

    /* if we only wrote part of the request pick up the leftovers */
    if ( leftover )
    {
        /* load the buffer with the new range, if possible */
        if ( _bufio_loadBuf( file, leftover ) )
        {
            startOffset = file->fpos - file->datastart;
            endOffset   = startOffset + leftover;

            memcpy( file->data+startOffset, newsrc, leftover );
            file->bufdirty      = PR_TRUE;
            file->dirtystart    = startOffset;
            file->dirtyend      = endOffset;
#ifdef DEBUG_dveditzbuf
            file->writes++;
#endif
            if ( endOffset > file->datasize )
                file->datasize = endOffset;

            bytesWritten = leftover;
        }
        else
        {
            /* request didn't fit in a buffer, write directly */
            if ( fseek( file->fd, file->fpos, SEEK_SET ) == 0 )
                bytesWritten = fwrite( newsrc, 1, leftover, file->fd );
            else
                bytesWritten = 0; /* seek failed! */
        }

        if ( retcount )
        {
            /* we already counted the first part we wrote */
            retcount    += bytesWritten;
            file->fpos  += bytesWritten;
        }
        else
        {
            retcount    = bytesCopied + bytesWritten;
            file->fpos  += retcount;
        }
    }

    if ( file->fpos > file->fsize )
        file->fsize = file->fpos;
    
    return retcount;
}



int bufio_Flush(BufioFile* file)
{
    if ( file->bufdirty )
        _bufio_flushBuf( file );
    
    return fflush(file->fd);
}



/*---------------------------------------------------------------------------*
 * internal helper functions
 *---------------------------------------------------------------------------*/
/**
 * Attempts to load the buffer with the requested amount of data.
 * Returns PR_TRUE if it was able to load *some* of the requested
 * data, but not necessarily all. Returns PR_FALSE if the read fails
 * or if the requested amount wouldn't fit in the buffer.
 */
static PRBool _bufio_loadBuf( BufioFile* file, PRUint32 count )
{
    PRInt32     startBuf;
    PRInt32     endPos;
    PRInt32     endBuf;
    PRUint32    bytesRead;

    /* no point in buffering more than the physical buffer will hold */
    if ( count > (PRUint32)file->bufsize )
        return PR_FALSE;

    /* Is caller asking for data we already have? */
    if ( STARTS_IN_BUF(file) && ENDS_IN_BUF(file,count) )
    {   
        PR_ASSERT(0);
        return PR_TRUE;
    }

    /* if the buffer's dirty make sure we successfully flush it */
    if ( file->bufdirty && _bufio_flushBuf(file) != 0 )
        return PR_FALSE;

    /* For now we're not trying anything smarter than simple paging. */
    /* Slide over if necessary to fit the entire request             */
    startBuf = ( file->fpos / file->bufsize ) * file->bufsize;
    endPos = file->fpos + count;
    endBuf = startBuf + file->bufsize;
    if ( endPos > endBuf )
        startBuf += (endPos - endBuf);

    if ( fseek( file->fd, startBuf, SEEK_SET ) != 0 )
        return PR_FALSE;
    else
    {
#if DEBUG_dougt
        ++num_reads;
#endif
        bytesRead = fread( file->data, 1, file->bufsize, file->fd );
        file->datastart  = startBuf;
        file->datasize   = bytesRead;
        file->bufdirty   = PR_FALSE;
        file->dirtystart = file->bufsize;
        file->dirtyend   = 0;
#ifdef DEBUG_dveditzbuf
        printf("REG: buffer read %d (%d) after %d reads\n",startBuf,file->fpos,file->reads);
        file->reads = 0;
        file->writes = 0;
#endif
        return PR_TRUE;
    }
}



static int _bufio_flushBuf( BufioFile* file )
{
    PRUint32 written;
    PRUint32 dirtyamt;
    PRInt32  startpos;

    PR_ASSERT(file);
    if ( !file || !file->bufdirty )
        return 0;

    startpos = file->datastart + file->dirtystart;
    if ( !fseek( file->fd, startpos, SEEK_SET ) )
    {
        dirtyamt = file->dirtyend - file->dirtystart;
        written = fwrite( file->data+file->dirtystart, 1, dirtyamt, file->fd );
        if ( written == dirtyamt )
        {
#ifdef DEBUG_dveditzbuf
            printf("REG: buffer flush %d - %d after %d writes\n",startpos,startpos+written,file->writes);
            file->writes = 0;
#endif
            file->bufdirty   = PR_FALSE;
            file->dirtystart = file->bufsize;
            file->dirtyend   = 0;
            return 0;
        }
    }
    return -1;
}



/*
*  sets the file buffer size to bufsize, clearing the buffer in the process.
*
*  accepts bufsize of -1 to mean default buffer size, defined by BUFIO_BUFSIZE_DEFAULT
*  returns new buffers size, or -1 if error occured
*/

int bufio_SetBufferSize(BufioFile* file, int bufsize)
{
    char    *newBuffer;
    int     retVal = -1;

    PR_ASSERT(file);
    if (!file)
        return retVal;

    if (bufsize == -1)
        bufsize = BUFIO_BUFSIZE_DEFAULT;
    if (bufsize == file->bufsize)
        return bufsize;

    newBuffer = (char*)PR_Malloc( bufsize );
    if (newBuffer)
    {
        /* if the buffer's dirty make sure we successfully flush it */
        if ( file->bufdirty && _bufio_flushBuf(file) != 0 )
        {
            PR_Free( newBuffer );
            return -1;
        }


        file->bufsize = bufsize;
        if ( file->data )
            PR_Free( file->data );
        file->data = newBuffer;
        file->datasize = 0;
        file->datastart = 0;
        retVal = bufsize;
    }
 
    return retVal;
}


/* EOF nr_bufio.c */
