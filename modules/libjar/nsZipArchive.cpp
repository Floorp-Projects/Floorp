/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Daniel Veditz <dveditz@netscape.com>
 */

/* 
 * This module implements a simple archive extractor for the PKZIP format.
 */
#include <string.h>

#include "nscore.h"
#include "prmem.h"
#include "prio.h"
#include "plstr.h"
#include "prlog.h"

#include "xp_regexp.h"

#include "zlib.h"

#include "zipfile.h"
#include "zipstruct.h"
#include "nsZipArchive.h"


static PRUint16 xtoint(unsigned char *ii);
static PRUint32 xtolong(unsigned char *ll);


/*---------------------------------------------
 * C API wrapper for nsZipArchive
 *--------------------------------------------*/

/**
 * ZIP_OpenArchive
 *
 * opens the named zip/jar archive and returns a handle that
 * represents the archive in other ZIP_ calls.
 *
 * @param   zipname   archive filename
 * @param   hZip      receives handle if archive opened OK
 * @return  status code
 */
PR_PUBLIC_API(PRInt32) ZIP_OpenArchive( const char * zipname, void** hZip )
{
  PRInt32 status;

  /*--- error check args ---*/
  if ( hZip == NULL )
    return ZIP_ERR_PARAM;

  /*--- NULL output to prevent use by bozos who don't check errors ---*/
  *hZip = NULL;

  /*--- create and open the archive ---*/
  nsZipArchive* zip = new nsZipArchive();
  if ( zip == NULL )
    return ZIP_ERR_MEMORY;

  status = zip->OpenArchive(zipname);

  if ( status == ZIP_OK )
    *hZip = NS_STATIC_CAST(void*,zip);
  else
      delete zip;

  return status;
}



/**
 * ZIP_CloseArchive
 *
 * closes zip archive and frees memory
 * @param   hZip  handle obtained from ZIP_OpenArchive
 * @return  status code
 */
PR_PUBLIC_API(PRInt32) ZIP_CloseArchive( void** hZip )
{
  /*--- error check args ---*/
  if ( hZip == NULL || *hZip == NULL )
    return ZIP_ERR_PARAM;

  nsZipArchive* zip = NS_STATIC_CAST(nsZipArchive*,*hZip);
  if ( zip->kMagic != ZIP_MAGIC )
    return ZIP_ERR_PARAM;   /* whatever it is isn't one of ours! */

  /*--- close the archive ---*/
  *hZip = NULL;
  delete zip;

  return ZIP_OK;
}



/**
 * ZIP_ExtractFile
 *
 * extracts named file from an opened archive
 *
 * @param   hZip      handle obtained from ZIP_OpenArchive
 * @param   filename  name of file in archive
 * @param   outname   filename to extract to
 */
PR_PUBLIC_API(PRInt32) ZIP_ExtractFile( void* hZip, const char * filename, const char * outname )
{
  /*--- error check args ---*/
  if ( hZip == NULL )
    return ZIP_ERR_PARAM;

  nsZipArchive* zip = NS_STATIC_CAST(nsZipArchive*,hZip);
  if ( zip->kMagic != ZIP_MAGIC )
    return ZIP_ERR_PARAM;   /* whatever it is isn't one of ours! */

  /*--- extract the file ---*/
  return zip->ExtractFile( filename, outname );
}



/**
 * ZIP_FindInit
 *
 * Initializes an enumeration of files in the archive
 *
 * @param   hZip      handle obtained from ZIP_OpenArchive
 * @param   pattern   regexp to match files in archive, the usual shell expressions.
 *                    NULL pattern also matches all files, faster than "*"
 */
PR_EXTERN(PRInt32) ZIP_FindInit( void* hZip, const char * pattern )
{
  /*--- error check args ---*/
  if ( hZip == NULL )
    return ZIP_ERR_PARAM;

  nsZipArchive* zip = NS_STATIC_CAST(nsZipArchive*,hZip);
  if ( zip->kMagic != ZIP_MAGIC )
    return ZIP_ERR_PARAM;   /* whatever it is isn't one of ours! */

  /*--- initialize the pattern search ---*/
  return zip->FindInit( pattern );
}



/**
 * ZIP_FindInit
 *
 * Puts the next name in the passed buffer. Returns ZIP_ERR_SMALLBUF when
 * the name is too large for the buffer, and ZIP_ERR_FNF when there are no 
 * more files that match the pattern
 *
 * @param   hZip      handle obtained from ZIP_OpenArchive
 * @param   outbuf    buffer to receive next filename
 * @param   bufsize   size of allocated buffer
 */
PR_EXTERN(PRInt32) ZIP_FindNext( void* hZip, char * outbuf, PRUint16 bufsize )
{
  /*--- error check args ---*/
  if ( hZip == NULL )
    return ZIP_ERR_PARAM;

  nsZipArchive* zip = NS_STATIC_CAST(nsZipArchive*,hZip);
  if ( zip->kMagic != ZIP_MAGIC )
    return ZIP_ERR_PARAM;   /* whatever it is isn't one of ours! */

  /*--- return next filename file ---*/
  return zip->FindNext( outbuf, bufsize );
}






//***********************************************************
//      nsZipArchive  --  public methods
//***********************************************************


//---------------------------------------------
//  nsZipArchive::OpenArchive
//---------------------------------------------
PRInt32 nsZipArchive::OpenArchive( const char * aArchiveName )
{
  //-- validate arguments
  if ( aArchiveName == NULL || *aArchiveName == '\0') 
    return ZIP_ERR_PARAM;

  //-- not allowed to do two opens on the same object!
  if ( mFd != NULL )
    return ZIP_ERR_GENERAL;

  //-- open the physical file
  mFd = PR_Open( aArchiveName, PR_RDONLY, 0 );
  if ( mFd == NULL )
    return ZIP_ERR_DISK;

  //-- get table of contents for archive
  return BuildFileList();
}



//---------------------------------------------
// nsZipArchive::ExtractFile
//---------------------------------------------
PRInt32 nsZipArchive::ExtractFile(const char* aFilename, const char* aOutname)
{
  //-- sanity check arguments
  if ( aFilename == NULL || aOutname == NULL )
    return ZIP_ERR_PARAM;

  //-- find file information
  const nsZipItem* item = GetFileItem( aFilename );
  if ( item == NULL )
    return ZIP_ERR_FNF;

  //-- extract the file using appropriate method
  switch( item->compression )
  {
    case STORED:
      return CopyItemToDisk( item, aOutname );

    case DEFLATED:
      return InflateItemToDisk( item, aOutname );

    default:
      //-- unsupported compression type
      return ZIP_ERR_UNSUPPORTED;
  }
}



//---------------------------------------------
// nsZipArchive::FindInit
//---------------------------------------------
PRInt32 nsZipArchive::FindInit( const char * aPattern )
{
  // validate the pattern
  if ( aPattern != NULL )
    switch (XP_RegExpValid( (char*)aPattern ))
    {
      case INVALID_SXP:
        return ZIP_ERR_PARAM;

      case NON_SXP:
        mPatternIsRegExp = PR_FALSE;
        break;

      case VALID_SXP:
        mPatternIsRegExp = PR_TRUE;
        break;

      default:
        // unexpected/undoc'ed return value
        PR_ASSERT( PR_FALSE );
        return ZIP_ERR_GENERAL;
    }

  // clear the old pattern
  if ( mPattern != NULL ) 
  {
    PL_strfree( mPattern );
    mPattern = NULL;
  }

  // re-initialize pattern state
  mPatternSlot = 0;
  mPatternItem = NULL;
  if ( aPattern != NULL )
    mPattern = PL_strdup( aPattern );

  return ZIP_OK;
}



//---------------------------------------------
// nsZipArchive::FindNext
//---------------------------------------------
PRInt32 nsZipArchive::FindNext( char * aBuf, PRUint16 aSize )
{
  PRInt32    status;
  PRBool     found  = PR_FALSE;
  PRUint32   slot   = mPatternSlot;
  nsZipItem* item   = mPatternItem;

  // we start from last match, look for next
  while ( slot < ZIP_TABSIZE && !found )
  {
    if ( item != NULL ) 
      item = item->next;    // move to next in current chain
    else
      item = mFiles[slot];  // starting a new slot

    if ( item == NULL )
    { // no more in this chain, move to next slot
      ++slot;
      continue;
    }
    else if ( mPattern == NULL )  
      found = PR_TRUE;            // always match
    else if ( mPatternIsRegExp )
      found = XP_RegExpMatch( item->name, mPattern, PR_FALSE );
    else
      found = ( PL_strcmp( item->name, mPattern ) == 0 );
  }

  if ( found ) 
  {
    if ( aSize > item->namelen ) 
    {
      PL_strcpy( aBuf, item->name );
      status = ZIP_OK;
    }
    else
      status = ZIP_ERR_SMALLBUF;
  }
  else
    status = ZIP_ERR_FNF;

  // save state for next Find. For 'smallbuf' we give user another chance
  if ( status != ZIP_ERR_SMALLBUF ) 
  {
    mPatternSlot = slot;
    mPatternItem = item;
  }

  return status;
}





//***********************************************************
//      nsZipArchive  --  private implementation
//***********************************************************


//---------------------------------------------
//  nsZipArchive::BuildFileList
//---------------------------------------------
PRInt32 nsZipArchive::BuildFileList()
{
  PRInt32   status = ZIP_OK;
  PRUint32  sig = 0L;
  PRUint32  namelen, extralen;
  PRUint16  hash;

  ZipLocal  Local;

  nsZipItem*  item;

  //-----------------------------------------------------------------------
  // read the local file headers. 
  //
  // What we *should* be doing is reading the central directory at the end,
  // all in one place. We'll have to change this eventually since the local
  // headers don't have the mode information we need for Unix files.
  //-----------------------------------------------------------------------
  PRInt32  pos = 0L;
  while ( status == ZIP_OK )
  {
    if ( PR_Seek( mFd, pos, PR_SEEK_SET ) != (PRInt32)pos )
    {
      //-- couldn't seek to next position
      status = ZIP_ERR_CORRUPT;
      break;
    }

    if ( PR_Read( mFd, (char*)&Local, sizeof(ZipLocal) ) != sizeof(ZipLocal) ) 
    {
      //-- file ends prematurely
      status = ZIP_ERR_CORRUPT;
      break;
    }

    //-- make sure we're processing a local header
    sig = xtolong( Local.signature );
    if ( sig == CENTRALSIG ) 
    {
      // we're onto the next section
      break;
    }
    else if ( sig != LOCALSIG ) 
    {
      //-- otherwise expected to find a local header
      status = ZIP_ERR_CORRUPT;
      break;
    }

    namelen = xtoint( Local.filename_len );
    extralen = xtoint( Local.extrafield_len );

    item = new nsZipItem();
    if ( item != NULL ) 
    {
      item->name = new char[namelen+1];

      if ( item->name != NULL )
      {
        if ( PR_Read( mFd, item->name, namelen ) == (PRInt32)namelen )
        {
          item->name[namelen] = 0;
          item->namelen = namelen;

          item->headerloc = pos;
          item->offset = pos + sizeof(ZipLocal) + namelen + extralen;
          item->compression = xtoint( Local.method );
          item->size = xtolong( Local.size );
          item->realsize = xtolong( Local.orglen );
          item->crc32 = xtolong( Local.crc32 );

          //-- add item to file table
          hash = HashName( item->name );
          item->next = mFiles[hash];
          mFiles[hash] = item;

          pos = item->offset + item->size;
        }
        else 
        {
          //-- file is truncated
          status = ZIP_ERR_CORRUPT;
          delete item;
        }
      }
      else 
      {
        //-- couldn't allocate for the filename
        status = ZIP_ERR_MEMORY;
        delete item;
      }
    }
    else 
    {
      //-- couldn't create a nsZipItem
      status = ZIP_ERR_MEMORY;
    }
  } /* while reading local headers */

  //-------------------------------------------------------
  //  we don't care about the rest of the file (until we
  //  fix this to read the central directory instead)
  //-------------------------------------------------------

  return status;
}



//---------------------------------------------
// nsZipArchive::CopyItemToDisk
//---------------------------------------------
PRInt32 nsZipArchive::CopyItemToDisk( const nsZipItem* aItem, const char* aOutname )
{
  PRInt32     status = ZIP_OK;
  PRUint32    chunk, pos, size;
  PRFileDesc* fOut = NULL;

  PR_ASSERT( aItem != NULL && aOutname != NULL );

  char* buf = (char*)PR_Malloc(ZIP_BUFLEN);
  if ( buf == NULL )
    return ZIP_ERR_MEMORY;

  //-- find start of file in archive
  if ( PR_Seek( mFd, aItem->offset, PR_SEEK_SET ) != (PRInt32)aItem->offset )
  {
    status = ZIP_ERR_CORRUPT;
    goto cleanup;
  }

  //-- open output file
  fOut = PR_Open( aOutname, PR_WRONLY | PR_CREATE_FILE, 0644);
  if ( fOut == NULL )
  {
    status = ZIP_ERR_DISK;
    goto cleanup;
  }

  //-- copy chunks until file is done
  size = aItem->size;
  for ( pos=0; pos < size; pos += chunk )
  {
    chunk = (pos+ZIP_BUFLEN <= size) ? ZIP_BUFLEN : size - pos;

    if ( PR_Read( mFd, buf, chunk ) != (PRInt32)chunk ) 
    {
      //-- unexpected end of data in archive
      status = ZIP_ERR_CORRUPT;
      break;
    }

    if ( PR_Write( fOut, buf, chunk ) < (PRInt32)chunk ) 
    {
      //-- Couldn't write all the data (disk full?)
      status = ZIP_ERR_DISK;
      break;
    }
  }

cleanup:
  if ( fOut != NULL )
    PR_Close( fOut );

  PR_FREEIF( buf );
  return status;
}


  
//---------------------------------------------
// nsZipArchive::GetFileItem
//---------------------------------------------
const nsZipItem*  nsZipArchive::GetFileItem( const char * aFilename )
{
  PR_ASSERT( aFilename != NULL );

  nsZipItem* item = mFiles[ HashName(aFilename) ];

  for ( ; item != NULL; item = item->next )
  {
    if ( 0 == PL_strcmp( aFilename, item->name ) ) 
      break; //-- found it
  }

  return item;
}



//---------------------------------------------
// nsZipArchive::HashName
//---------------------------------------------
PRUint16 nsZipArchive::HashName( const char* aName )
{
  PRUint16 val = 0;
  PRUint8* c;

  PR_ASSERT( aName != NULL );

  for ( c = (PRUint8*)aName; *c != 0; c++ ) {
    val = val*37 + *c;
  }

  return (val % ZIP_TABSIZE);
}



//---------------------------------------------
// nsZipArchive::InflateItemToDisk
//---------------------------------------------
PRInt32 nsZipArchive::InflateItemToDisk( const nsZipItem* aItem, const char* aOutname )
{
  PRInt32     status = ZIP_OK;    
  PRUint32    chunk, inpos, outpos, size;
  PRFileDesc* fOut = NULL;
  z_stream    zs;
  int         zerr;
  PRBool      bInflating = PR_FALSE;

  PR_ASSERT( aItem != NULL && aOutname != NULL );

  //-- allocate deflation buffers
  Bytef *inbuf  = (Bytef*)PR_Malloc(ZIP_BUFLEN);
  Bytef *outbuf = (Bytef*)PR_Malloc(ZIP_BUFLEN);
  if ( inbuf == NULL || outbuf == NULL )
  {
    status = ZIP_ERR_MEMORY;
    goto cleanup;
  }

  //-- find start of file in archive
  if ( PR_Seek( mFd, aItem->offset, PR_SEEK_SET ) != (PRInt32)aItem->offset )
  {
    status = ZIP_ERR_CORRUPT;
    goto cleanup;
  }

  //-- open output file
  fOut = PR_Open( aOutname, PR_WRONLY | PR_CREATE_FILE, 0644);
  if ( fOut == NULL )
  {
    status = ZIP_ERR_DISK;
    goto cleanup;
  }

  //-- set up the inflate
  memset( &zs, 0, sizeof(zs) );
  zerr = inflateInit2( &zs, -MAX_WBITS );
  if ( zerr != Z_OK )
  {
    status = ZIP_ERR_GENERAL;
    goto cleanup;
  }
  bInflating = PR_TRUE;


  //-- inflate loop
  size = aItem->size;
  outpos = inpos = 0;
  zs.next_out = outbuf;
  zs.avail_out = ZIP_BUFLEN;
  while ( zerr == Z_OK )
  {
    if ( zs.avail_in == 0 && zs.total_in < size )
    {
      //-- no data to inflate yet still more in file:
      //-- read another chunk of compressed data

      inpos = zs.total_in;  // input position
      chunk = ( inpos + ZIP_BUFLEN <= size ) ? ZIP_BUFLEN : size - inpos;

      if ( PR_Read( mFd, inbuf, chunk ) != (PRInt32)chunk )
      {
        //-- unexpected end of data
        status = ZIP_ERR_CORRUPT;
        break;
      }
      zs.next_in  = inbuf;
      zs.avail_in = ZIP_BUFLEN;
    }

    if ( zs.avail_out == 0 )
    {
      //-- write inflated buffer to disk and make space
      if ( PR_Write( fOut, outbuf, ZIP_BUFLEN ) < ZIP_BUFLEN ) 
      {
        //-- Couldn't write all the data (disk full?)
        status = ZIP_ERR_DISK;
        break;
      }
      outpos = zs.total_out;

      zs.next_out  = outbuf;
      zs.avail_out = chunk;
    }

    zerr = inflate( &zs, Z_PARTIAL_FLUSH );

  } // while

  //-- write last inflated bit to disk
  if ( zerr == Z_STREAM_END && outpos < zs.total_out )
  {
    chunk = zs.total_out - outpos;
    if ( PR_Write( fOut, outbuf, chunk ) < (PRInt32)chunk ) 
    {
      status = ZIP_ERR_DISK;
    }
  }

  //-- convert zlib error to return value
  if ( status == ZIP_OK && zerr != Z_OK && zerr != Z_STREAM_END )
  {
    status = (zerr == Z_MEM_ERROR) ? ZIP_ERR_MEMORY : ZIP_ERR_CORRUPT;
  }

  //-- if found no errors make sure we've converted the whole thing
  PR_ASSERT( status != ZIP_OK || zs.total_in == aItem->size );
  PR_ASSERT( status != ZIP_OK || zs.total_out == aItem->realsize );

cleanup:
  if ( bInflating )
  {
    //-- free zlib internal state
    inflateEnd( &zs );
  }

  if ( fOut != NULL )
    PR_Close( fOut );

  PR_FREEIF( inbuf );
  PR_FREEIF( outbuf );

  return status;
}



//------------------------------------------
// nsZipArchive constructor and destructor
//------------------------------------------

nsZipArchive::nsZipArchive()
  : kMagic(ZIP_MAGIC), mFd(NULL), mPattern(NULL), 
    mPatternSlot(ZIP_TABSIZE), mPatternItem(NULL)
{
  // initialize the table to NULL
  for ( int i = 0; i < ZIP_TABSIZE; ++i) {
    mFiles[i] = NULL;
  }
}

nsZipArchive::~nsZipArchive()
{
  // close the file if open
  if ( mFd != NULL ) {
    PR_Close(mFd);
  }

  // delete nsZipItems in table
  nsZipItem* pItem;
  for ( int i = 0; i < ZIP_TABSIZE; ++i)
  {
    pItem = mFiles[i];
    while ( pItem != NULL )
    {
      mFiles[i] = pItem->next;
      delete pItem;
      pItem = mFiles[i];
    }
  }

  if ( mPattern != NULL ) {
    PL_strfree(mPattern);
  }
}



//------------------------------------------
// nsZipItem constructor and destructor
//------------------------------------------

nsZipItem::nsZipItem() : name(NULL),next(NULL) {}

nsZipItem::~nsZipItem()
{
  if (name != NULL )
    delete [] name;
}



//------------------------------------------
// helper functions
//------------------------------------------

/*
 *  x t o i n t
 *
 *  Converts a two byte ugly endianed integer
 *  to our platform's integer.
 */
static PRUint16 xtoint (unsigned char *ii)
{
  return (PRUint16) (ii [0]) | ((PRUint16) ii [1] << 8);
}

/*
 *  x t o l o n g
 *
 *  Converts a four byte ugly endianed integer
 *  to our platform's integer.
 */
static PRUint32 xtolong (unsigned char *ll)
{
  PRUint32 ret;

  ret =  (
         (((PRUint32) ll [0]) <<  0) |
         (((PRUint32) ll [1]) <<  8) |
         (((PRUint32) ll [2]) << 16) |
         (((PRUint32) ll [3]) << 24)
         );

  return ret;
}
