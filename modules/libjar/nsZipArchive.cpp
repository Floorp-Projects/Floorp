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
 *     Samir Gehani <sgehani@netscape.com>
 */

/* 
 * This module implements a simple archive extractor for the PKZIP format.
 *
 * The underlying nsZipArchive is NOT thread-safe. Do not pass references
 * or pointers to it across thread boundaries.
 */


#ifndef STANDALONE 
#include "nscore.h"
#include "prmem.h"
#include "prio.h"
#include "plstr.h"
#include "xp_regexp.h"
#define ZFILE_CREATE    PR_WRONLY | PR_CREATE_FILE
#define READTYPE  PRInt32
#else
#ifdef WIN32
#include "windows.h"
#endif
#include "zipstub.h"
#undef NETSCAPE       // undoes prtypes damage in zlib.h
#define ZFILE_CREATE  "wb"
#define READTYPE  PRUint32
#endif /* STANDALONE */

#include "zlib.h"

#include "zipfile.h"
#include "zipstruct.h"
#include "nsZipArchive.h"


static PRUint16 xtoint(unsigned char *ii);
static PRUint32 xtolong(unsigned char *ll);


/*---------------------------------------------
 * C API wrapper for nsZipArchive
 *--------------------------------------------*/

#ifdef STANDALONE

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
  if ( hZip == 0 )
    return ZIP_ERR_PARAM;

  /*--- NULL output to prevent use by bozos who don't check errors ---*/
  *hZip = 0;

  /*--- create and open the archive ---*/
  nsZipArchive* zip = new nsZipArchive();
  if ( zip == 0 )
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
  if ( hZip == 0 || *hZip == 0 )
    return ZIP_ERR_PARAM;

  nsZipArchive* zip = NS_STATIC_CAST(nsZipArchive*,*hZip);
  if ( zip->kMagic != ZIP_MAGIC )
    return ZIP_ERR_PARAM;   /* whatever it is isn't one of ours! */

  /*--- close the archive ---*/
  *hZip = 0;
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
  if ( hZip == 0 )
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
PR_PUBLIC_API(void*) ZIP_FindInit( void* hZip, const char * pattern )
{
  /*--- error check args ---*/
  if ( hZip == 0 )
    return 0;

  nsZipArchive* zip = NS_STATIC_CAST(nsZipArchive*,hZip);
  if ( zip->kMagic != ZIP_MAGIC )
    return 0;   /* whatever it is isn't one of ours! */

  /*--- initialize the pattern search ---*/
  return zip->FindInit( pattern );
}



/**
 * ZIP_FindNext
 *
 * Puts the next name in the passed buffer. Returns ZIP_ERR_SMALLBUF when
 * the name is too large for the buffer, and ZIP_ERR_FNF when there are no 
 * more files that match the pattern
 *
 * @param   hFind     handle obtained from ZIP_FindInit
 * @param   outbuf    buffer to receive next filename
 * @param   bufsize   size of allocated buffer
 */
PR_PUBLIC_API(PRInt32) ZIP_FindNext( void* hFind, char * outbuf, PRUint16 bufsize )
{
  PRInt32 status;

  /*--- error check args ---*/
  if ( hFind == 0 )
    return ZIP_ERR_PARAM;

  nsZipFind* find = NS_STATIC_CAST(nsZipFind*,hFind);
  if ( find->kMagic != ZIPFIND_MAGIC )
    return ZIP_ERR_PARAM;   /* whatever it is isn't one of ours! */

  /*--- return next filename file ---*/
  nsZipItem* item;
  status = find->mArchive->FindNext( find, &item );
  if ( status == ZIP_OK )
  {
    if ( bufsize > item->namelen ) 
    {
        PL_strcpy( outbuf, item->name );
    }
    else
        status = ZIP_ERR_SMALLBUF;
  }

  return status;
}



/**
 * ZIP_FindFree
 *
 * Releases allocated memory associated with the find token
 *
 * @param   hFind     handle obtained from ZIP_FindInit
 */
PR_PUBLIC_API(PRInt32) ZIP_FindFree( void* hFind )
{
  /*--- error check args ---*/
  if ( hFind == 0 )
    return ZIP_ERR_PARAM;

  nsZipFind* find = NS_STATIC_CAST(nsZipFind*,hFind);
  if ( find->kMagic != ZIPFIND_MAGIC )
    return ZIP_ERR_PARAM;   /* whatever it is isn't one of ours! */

  /* free the find structure */
  return find->mArchive->FindFree( find );
}

#endif /* STANDALONE */





//***********************************************************
//      nsZipArchive  --  public methods
//***********************************************************


//---------------------------------------------
//  nsZipArchive::OpenArchive
//---------------------------------------------
PRInt32 nsZipArchive::OpenArchive( const char * aArchiveName )
{
  //-- validate arguments
  if ( aArchiveName == 0 || *aArchiveName == '\0') 
    return ZIP_ERR_PARAM;

  //-- not allowed to do two opens on the same object!
  if ( mFd != 0 )
    return ZIP_ERR_GENERAL;

  //-- open the physical file
  mFd = PR_Open( aArchiveName, PR_RDONLY, 0 );
  if ( mFd == 0 )
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
  if ( aFilename == 0 || aOutname == 0 )
    return ZIP_ERR_PARAM;

  //-- find file information
  const nsZipItem* item = GetFileItem( aFilename );
  if ( item == 0 )
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
nsZipFind* nsZipArchive::FindInit( const char * aPattern )
{
  PRBool  regExp;
  char*   pattern = 0;

  // validate the pattern
  if ( aPattern )
  {
    switch (XP_RegExpValid( (char*)aPattern ))
    {
      case INVALID_SXP:
        return 0;

      case NON_SXP:
        regExp = PR_FALSE;
        break;

      case VALID_SXP:
        regExp = PR_TRUE;
        break;

      default:
        // undocumented return value from RegExpValid!
        PR_ASSERT( PR_FALSE );
        return 0;
    }

    pattern = PL_strdup( aPattern );
    if ( !pattern )
      return 0;
  }

  return new nsZipFind( this, pattern, regExp );
}



//---------------------------------------------
// nsZipArchive::FindNext
//---------------------------------------------
PRInt32 nsZipArchive::FindNext( nsZipFind* aFind, nsZipItem** aResult)
{
  PRInt32    status;
  PRBool     found  = PR_FALSE;
  PRUint16   slot   = aFind->mSlot;
  nsZipItem* item   = aFind->mItem;

  if ( aFind->mArchive != this )
    return ZIP_ERR_PARAM;

  // we start from last match, look for next
  while ( slot < ZIP_TABSIZE && !found )
  {
    if ( item != 0 ) 
      item = item->next;    // move to next in current chain
    else
      item = mFiles[slot];  // starting a new slot

    if ( item == 0 )
    { // no more in this chain, move to next slot
      ++slot;
      continue;
    }
    else if ( aFind->mPattern == 0 )  
      found = PR_TRUE;            // always match
    else if ( aFind->mRegExp )
      found = (XP_RegExpMatch( item->name, aFind->mPattern, PR_FALSE ) == MATCH);
    else
      found = ( PL_strcmp( item->name, aFind->mPattern ) == 0 );
  }

  if ( found ) 
  {
      *aResult = item;
      aFind->mSlot = slot;
      aFind->mItem = item;
      status = ZIP_OK;
  }
  else
    status = ZIP_ERR_FNF;

  return status;
}



//---------------------------------------------
// nsZipArchive::FindFree
//---------------------------------------------
PRInt32 nsZipArchive::FindFree( nsZipFind* aFind )
{
  if ( aFind->mArchive != this )
    return ZIP_ERR_PARAM;

  delete aFind;
  return ZIP_OK;
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
  PRUint32  hash;

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
#ifndef STANDALONE 
    if ( PR_Seek( mFd, pos, PR_SEEK_SET ) != (PRInt32)pos )
#else
    // For standalone, PR_Seek() is stubbed with fseek(), which returns 0
    // if successfull, otherwise a non-zero.
    if ( PR_Seek( mFd, pos, PR_SEEK_SET ) != 0 )
#endif
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
    if ( item != 0 ) 
    {
      item->name = new char[namelen+1];

      if ( item->name != 0 )
      {
        if ( PR_Read( mFd, item->name, namelen ) == (READTYPE)namelen )
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
  PRFileDesc* fOut = 0;

  PR_ASSERT( aItem != 0 && aOutname != 0 );

  char* buf = (char*)PR_Malloc(ZIP_BUFLEN);
  if ( buf == 0 )
    return ZIP_ERR_MEMORY;

  //-- find start of file in archive

#ifndef STANDALONE 
  if ( PR_Seek( mFd, aItem->offset, PR_SEEK_SET ) != (PRInt32)aItem->offset )
#else
  // For standalone, PR_Seek() is stubbed with fseek(), which returns 0
  // if successfull, otherwise a non-zero.
  if ( PR_Seek( mFd, aItem->offset, PR_SEEK_SET ) != 0)
#endif
  {
    status = ZIP_ERR_CORRUPT;
    goto cleanup;
  }

  //-- open output file
  fOut = PR_Open( aOutname, ZFILE_CREATE , 0644);
  if ( fOut == 0 )
  {
    status = ZIP_ERR_DISK;
    goto cleanup;
  }

  //-- copy chunks until file is done
  size = aItem->size;
  for ( pos=0; pos < size; pos += chunk )
  {
    chunk = (pos+ZIP_BUFLEN <= size) ? ZIP_BUFLEN : size - pos;

    if ( PR_Read( mFd, buf, chunk ) != (READTYPE)chunk ) 
    {
      //-- unexpected end of data in archive
      status = ZIP_ERR_CORRUPT;
      break;
    }

    if ( PR_Write( fOut, buf, chunk ) < (READTYPE)chunk ) 
    {
      //-- Couldn't write all the data (disk full?)
      status = ZIP_ERR_DISK;
      break;
    }
  }

cleanup:
  if ( fOut != 0 )
    PR_Close( fOut );

  PR_FREEIF( buf );
  return status;
}


  
//---------------------------------------------
// nsZipArchive::GetFileItem
//---------------------------------------------
const nsZipItem*  nsZipArchive::GetFileItem( const char * aFilename )
{
  PR_ASSERT( aFilename != 0 );

  nsZipItem* item = mFiles[ HashName(aFilename) ];

  for ( ; item != 0; item = item->next )
  {
    if ( 0 == PL_strcmp( aFilename, item->name ) ) 
      break; //-- found it
  }

  return item;
}



//---------------------------------------------
// nsZipArchive::HashName
//---------------------------------------------
PRUint32 nsZipArchive::HashName( const char* aName )
{
  PRUint32 val = 0;
  PRUint8* c;

  PR_ASSERT( aName != 0 );

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
  PRFileDesc* fOut = 0;
  z_stream    zs;
  int         zerr;
  PRBool      bInflating = PR_FALSE;
  PRBool      bRead;
  PRBool      bWrote;

#ifdef STANDALONE 
  MSG msg;
#endif /* STANDALONE */

  PR_ASSERT( aItem != 0 && aOutname != 0 );

  //-- allocate deflation buffers
  Bytef *inbuf  = (Bytef*)PR_Malloc(ZIP_BUFLEN);
  Bytef *outbuf = (Bytef*)PR_Malloc(ZIP_BUFLEN);
  if ( inbuf == 0 || outbuf == 0 )
  {
    status = ZIP_ERR_MEMORY;
    goto cleanup;
  }

  //-- find start of file in archive
#ifndef STANDALONE 
  if ( PR_Seek( mFd, aItem->offset, PR_SEEK_SET ) != (PRInt32)aItem->offset )
#else
  // For standalone, PR_Seek() is stubbed with fseek(), which returns 0
  // if successfull, otherwise a non-zero.
  if ( PR_Seek( mFd, aItem->offset, PR_SEEK_SET ) != 0)
#endif
  {
    status = ZIP_ERR_CORRUPT;
    goto cleanup;
  }

  //-- open output file
  fOut = PR_Open( aOutname, ZFILE_CREATE, 0644);
  if ( fOut == 0 )
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
    bRead  = PR_FALSE;
    bWrote = PR_FALSE;

    if ( zs.avail_in == 0 && zs.total_in < size )
    {
      //-- no data to inflate yet still more in file:
      //-- read another chunk of compressed data

      inpos = zs.total_in;  // input position
      chunk = ( inpos + ZIP_BUFLEN <= size ) ? ZIP_BUFLEN : size - inpos;

      if ( PR_Read( mFd, inbuf, chunk ) != (READTYPE)chunk )
      {
        //-- unexpected end of data
        status = ZIP_ERR_CORRUPT;
        break;
      }
      zs.next_in  = inbuf;
      zs.avail_in = chunk;
      bRead       = PR_TRUE;
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
      zs.avail_out = ZIP_BUFLEN;
      bWrote       = PR_TRUE;
    }

    if(bRead || bWrote)
      zerr = inflate( &zs, Z_PARTIAL_FLUSH );
    else
      zerr = Z_STREAM_END;

#if defined STANDALONE && defined WIN32
    while(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
#endif /* STANDALONE */
  } // while

  //-- write last inflated bit to disk
  if ( zerr == Z_STREAM_END && outpos < zs.total_out )
  {
    chunk = zs.total_out - outpos;
    if ( PR_Write( fOut, outbuf, chunk ) < (READTYPE)chunk ) 
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

  if ( fOut != 0 )
    PR_Close( fOut );

  PR_FREEIF( inbuf );
  PR_FREEIF( outbuf );

  return status;
}



//------------------------------------------
// nsZipArchive constructor and destructor
//------------------------------------------

nsZipArchive::nsZipArchive()
  : kMagic(ZIP_MAGIC), mFd(0)
{
  // initialize the table to NULL
  for ( int i = 0; i < ZIP_TABSIZE; ++i) {
    mFiles[i] = 0;
  }
}

nsZipArchive::~nsZipArchive()
{
  // close the file if open
  if ( mFd != 0 ) {
    PR_Close(mFd);
  }

  // delete nsZipItems in table
  nsZipItem* pItem;
  for ( int i = 0; i < ZIP_TABSIZE; ++i)
  {
    pItem = mFiles[i];
    while ( pItem != 0 )
    {
      mFiles[i] = pItem->next;
      delete pItem;
      pItem = mFiles[i];
    }
  }
}




//------------------------------------------
// nsZipItem constructor and destructor
//------------------------------------------

nsZipItem::nsZipItem() : name(0),next(0) {}

nsZipItem::~nsZipItem()
{
  if (name != 0 )
    delete [] name;
}




//------------------------------------------
// nsZipFind constructor and destructor
//------------------------------------------

nsZipFind::nsZipFind( nsZipArchive* aZip, char* aPattern, PRBool aRegExp )
: kMagic(ZIPFIND_MAGIC), 
  mArchive(aZip), 
  mPattern(aPattern), 
  mSlot(0), 
  mItem(0), 
  mRegExp(aRegExp) 
{}

nsZipFind::~nsZipFind()
{
  if (mPattern != 0 )
    PL_strfree( mPattern );
}

//------------------------------------------
// nsZipFind::GetArchive
//------------------------------------------
nsZipArchive* nsZipFind::GetArchive()
{
    if (!mArchive)
        return NULL;

    return mArchive;
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
  return (PRUint16) ((ii [0]) | (ii [1] << 8));
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


