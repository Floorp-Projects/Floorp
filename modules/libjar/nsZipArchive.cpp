/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Daniel Veditz <dveditz@netscape.com>
 *     Samir Gehani <sgehani@netscape.com>
 *     Mitch Stoltz <mstoltz@netscape.com>
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
#include "zlib.h"
#include "nsISupportsUtils.h"

#else

#ifdef WIN32
#include "windows.h"
#endif

#undef MOZILLA_CLIENT       // undoes prtypes damage in zlib.h
#define ZFILE_CREATE  "wb"
#define READTYPE  PRUint32
#include "zlib.h"
#undef PR_PUBLIC_API
#include "zipstub.h"

#ifdef XP_MAC
#include <string.h>
#include <stdlib.h>

char * strdup(const char *src);
char * strdup(const char *src)
{
	long len = strlen(src);
	char *dup = (char *)malloc(len+1 * sizeof(char));
	memcpy(dup, src, len+1);
	return dup;
}
#endif

#endif /* STANDALONE */

#ifdef XP_UNIX
    #include <sys/stat.h>
#elif defined(XP_PC)
    #include <io.h>
#endif

#include "zipfile.h"
#include "zipstruct.h"
#include "nsZipArchive.h"

static PRUint16 xtoint(unsigned char *ii);
static PRUint32 xtolong(unsigned char *ll);
static PRUint16 ExtractMode(PRUint32 ext_attr);
static void dosdate(char *aOutDateStr, PRUint16 aDate);
static void dostime(char *aOutTimeStr, PRUint16 aTime);

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
  status = find->GetArchive()->FindNext( find, &item );
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
  return find->GetArchive()->FindFree( find );
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

PRInt32 nsZipArchive::OpenArchiveWithFileDesc(PRFileDesc* fd)
{
  //-- validate arguments
  if (fd == 0) 
    return ZIP_ERR_PARAM;

  //-- not allowed to do two opens on the same object!
   if ( mFd != 0 )
    return ZIP_ERR_GENERAL;

  mFd = fd;

  //-- get table of contents for archive
  return BuildFileList();
}

//---------------------------------------------
//  nsZipArchive::CloseArchive
//---------------------------------------------
PRInt32 nsZipArchive::CloseArchive()
{
  // close the file if open
  if ( mFd != 0 ) {
    PR_Close(mFd);
	mFd = 0;
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
  return ZIP_OK;
}

//---------------------------------------------
// nsZipArchive::GetItem
//---------------------------------------------
PRInt32 nsZipArchive::GetItem( const char * aFilename, nsZipItem **result)
{
	//-- Parameter validity check
	if (aFilename == 0)
		return ZIP_ERR_PARAM;

	//PRInt32 result;
	nsZipItem* item;

	//-- find file information
	item = GetFileItem( aFilename );
	if ( item == 0 )
	{
		return ZIP_ERR_FNF;
	}


    *result = item; // Return a pointer to the struct
	return ZIP_OK;
}


//---------------------------------------------
// nsZipArchive::ReadInit
//---------------------------------------------
PRInt32 nsZipArchive::ReadInit(const char* zipEntry, nsZipRead** aRead)
{
  //-- Parameter validity check
  if (zipEntry == 0 || aRead == 0)
    return ZIP_ERR_PARAM;

  PRInt32 result;

  //-- find item and seek to file position
  nsZipItem* item;
  result = SeekToItem(zipEntry, &item);
  if (result != ZIP_OK) return result;

  //-- Create nsZipRead object
  *aRead = new nsZipRead(this, item);
  if (aRead == 0)
    return ZIP_ERR_MEMORY;

  //-- initialize crc
  (*aRead)->mCRC32 = crc32(0L, Z_NULL, 0);

  //-- Read the item into memory
  //   Inflate if necessary and save in mInflatedFileBuffer
  //   for sequential reading.
  //   (nsJAR needs the whole file in memory before passing it on)
  char* buf = (char*)PR_Malloc(item->realsize);
  if (!buf) return ZIP_ERR_MEMORY;
  switch(item->compression)
  {
    case DEFLATED:
      result = InflateItem(item, 0, buf);
      break;
    case STORED:
      result = CopyItemToBuffer(item, buf);
      break;
    default:
      return ZIP_ERR_UNSUPPORTED;
  }

  if (result == ZIP_OK)
    (*aRead)->mFileBuffer = buf;
  return result;
}
  
//------------------------------------------
// nsZipArchive::Read
//------------------------------------------
PRInt32 nsZipArchive::Read(nsZipRead* aRead, char* aOutBuf,
                           PRUint32 aCount, PRUint32* aBytesRead)
{
  //-- Copies data from aRead->mFileBuffer
  //-- Parameter check
  if (aBytesRead == 0 || aRead == 0 || aOutBuf == 0 ||
      aRead->mArchive != this)
    return ZIP_ERR_PARAM;

  if(!aRead->mItem || !aRead->mFileBuffer)
     return ZIP_ERR_GENERAL;

  //-- Set up the copy
  PRUint32 bigBufSize = aRead->mItem->realsize;
  PRUint32 curPos = aRead->mCurPos;
  *aBytesRead  = (curPos + aCount) < bigBufSize ? aCount : bigBufSize - curPos;
  char* src = aRead->mFileBuffer + curPos;

  //-- Do the copy and record number of bytes copied
  memcpy(aOutBuf, src, *aBytesRead);
  aRead->mCurPos += *aBytesRead;

  return ZIP_OK;
}

//---------------------------------------------
// nsZipArchive::Available
//---------------------------------------------
PRUint32 nsZipArchive::Available(nsZipRead* aRead)
{
  if (aRead == 0)
    return 0;

  nsZipItem* item = aRead->mItem;

  if (item->compression == DEFLATED)
    return item->realsize - aRead->mCurPos;
  else
    return item->size - aRead->mCurPos;
}

//---------------------------------------------
// nsZipArchive::ExtractFile
//---------------------------------------------
PRInt32 nsZipArchive::ExtractFile(const char* zipEntry, const char* aOutname)
{
  PRFileDesc* fOut = PR_Open(aOutname, ZFILE_CREATE, 0644);
  if (fOut == 0)
    return ZIP_ERR_DISK;

  nsZipItem *item = 0;
  PRInt32 status = ExtractFileToFileDesc(zipEntry, fOut, &item);
  PR_Close(fOut);

  if (status != ZIP_OK) {
      PR_Delete(aOutname);
  }
#if defined(XP_UNIX)
  else {
      //-- set extracted file permissions
      chmod(aOutname, item->mode);
  }
#endif
  return status;
}

PRInt32
nsZipArchive::ExtractFileToFileDesc(const char * zipEntry, PRFileDesc* outFD,
                                    nsZipItem **outItem)
{
  //-- sanity check arguments
  if ( zipEntry == 0 || outFD == 0 || outItem == 0)
    return ZIP_ERR_PARAM;

  PRInt32 status;
  nsZipItem* item;
  
  //-- Find item in archive and seek to it
  status = SeekToItem( zipEntry, &item );
  if (status != ZIP_OK)
    return status;

  //-- extract the file using the appropriate method
  switch( item->compression )
  {
    case STORED:
      status = CopyItemToDisk( item, outFD );
      break;

    case DEFLATED:
      status = InflateItem( item, outFD, 0 );
      break;

    default:
      //-- unsupported compression type
      return ZIP_ERR_UNSUPPORTED;
  }

  *outItem = item;
  return status;
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
#if defined(STANDALONE) && defined(XP_MAC)
      // simulate <regexp>* matches
      found = ( strncmp( item->name, aFind->mPattern, strlen(aFind->mPattern) ) == 0 );
#else
      found = ( PL_strcmp( item->name, aFind->mPattern ) == 0 );
#endif
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
  PRUint32  namelen, extralen, commentlen;
  PRUint32  hash;
  PRUint32  size;

  ZipLocal   Local;
  ZipCentral Central;

  nsZipItem*  item;

  //-----------------------------------------------------------------------
  // skip to the central directory
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

    //-- check if we hit the central directory
    sig = xtolong( Local.signature );
    if ( sig == LOCALSIG )
    {
        //-- check length of this file and its metadata so we can skip over it
        namelen = xtoint( Local.filename_len );
        extralen = xtoint( Local.extrafield_len );
        size = xtolong( Local.size );

        //-- reposition file mark to next expected local header
        pos += sizeof(ZipLocal) + namelen + extralen + size;
    }
    else if ( sig == CENTRALSIG ) 
    {
      // file mark set to start of central directory
      break;
    }
    else
    {
      //-- otherwise expected to find a local header
      status = ZIP_ERR_CORRUPT;
      break;
    }
  } /* while reading local headers */

  //-------------------------------------------------------
  // read the central directory headers
  //-------------------------------------------------------
  while ( status == ZIP_OK )
  {
#ifndef STANDALONE 
    if ( PR_Seek( mFd, pos, PR_SEEK_SET ) != (PRInt32)pos )
#else
    if ( PR_Seek( mFd, pos, PR_SEEK_SET ) != 0 )
#endif
    {
      //-- couldn't seek to next position
      status = ZIP_ERR_CORRUPT;
      break;
    }

    if ( PR_Read( mFd, (char*)&Central, sizeof(Central) ) != sizeof(ZipCentral) )
    {
      //-- central dir end record is smaller than a central dir header
      sig = xtolong( Central.signature );
      if ( sig == ENDSIG )
      {
        //-- we've reached the end of the central dir
        break;
      }
      else
      {
        status = ZIP_ERR_CORRUPT;
        break;
      }
    }

    sig = xtolong( Central.signature );
    if ( sig == ENDSIG )
    {
      //-- we've reached the end of the central dir
      break;
    }
    else if ( sig != CENTRALSIG )
    {
        //-- otherwise expected to find a  central dir header
        status = ZIP_ERR_CORRUPT;
        break;
    }

    namelen = xtoint( Central.filename_len );
    extralen = xtoint( Central.extrafield_len );
    commentlen = xtoint( Central.commentfield_len );
    
    item = new nsZipItem();
    if (item != 0)
    {
      item->name = new char[namelen+1];

      if ( item->name != 0 )
      {
        if ( PR_Read( mFd, item->name, namelen ) == (READTYPE)namelen )
        {
          item->name[namelen] = 0;
          item->namelen = namelen;
          item->headerloc = xtolong( Central.localhdr_offset );

          //-- seek to local header
#ifndef STANDALONE 
          if ( PR_Seek( mFd, item->headerloc, PR_SEEK_SET ) != (PRInt32)item->headerloc )
#else
          if ( PR_Seek( mFd, item->headerloc, PR_SEEK_SET ) != 0 )
#endif
          {
            //-- couldn't seek to next position
            status = ZIP_ERR_CORRUPT;
            break;
          }

          //-- read local header to extract local extralen
          //-- NOTE: extralen is different in central header and local header
          //--       for archives created using the Unix "zip" utility. To set 
          //--       the offset accurately we need the local extralen.
          if ( PR_Read( mFd, (char*)&Local, sizeof(ZipLocal) ) != (READTYPE)sizeof(ZipLocal) )
          { 
              //-- expected a complete local header
              status = ZIP_ERR_CORRUPT;
              break;
          }

          item->offset = item->headerloc + 
                         sizeof(ZipLocal) + 
                         namelen + 
                         xtoint( Local.extrafield_len );
          item->compression = xtoint( Central.method );
          item->size = xtolong( Central.size );
          item->realsize = xtolong( Central.orglen );
          item->crc32 = xtolong( Central.crc32 );
          item->mode = ExtractMode(xtolong( Central.external_attributes )); 
          item->time = xtoint( Central.time );
          item->date = xtoint( Central.date );

          //-- add item to file table
          hash = HashName( item->name );
          item->next = mFiles[hash];
          mFiles[hash] = item;

          //-- set pos to next expected central dir header
          pos += sizeof(ZipCentral) + namelen + extralen + commentlen;
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
      //-- couldn't create an nsZipItem
      status = ZIP_ERR_MEMORY;
    }
  } /* while reading central directory records */

  return status;
}
  
//---------------------------------------------
// nsZipArchive::GetFileItem
//---------------------------------------------
nsZipItem*  nsZipArchive::GetFileItem( const char * zipEntry )
{
  PR_ASSERT( zipEntry != 0 );

  nsZipItem* item = mFiles[ HashName(zipEntry) ];

  for ( ; item != 0; item = item->next )
  {
    if ( 0 == PL_strcmp( zipEntry, item->name ) ) 
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
// nsZipArchive::SeekToItem
//---------------------------------------------
PRInt32  nsZipArchive::SeekToItem(const char* zipEntry, nsZipItem** aItem)
{
  PR_ASSERT (zipEntry != 0);

  //-- find file information
  *aItem = GetFileItem( zipEntry );
  if ( *aItem == 0 )
    return ZIP_ERR_FNF;

  //-- find start of file in archive
#ifndef STANDALONE 
  if ( PR_Seek( mFd, (*aItem)->offset, PR_SEEK_SET ) != (PRInt32)(*aItem)->offset )
#else
  // For standalone, PR_Seek() is stubbed with fseek(), which returns 0
  // if successfull, otherwise a non-zero.
  if ( PR_Seek( mFd, (*aItem)->offset, PR_SEEK_SET ) != 0)
#endif
    return  ZIP_ERR_CORRUPT;

  return ZIP_OK;
}

//---------------------------------------------
// nsZipArchive::CopyItemToBuffer
//---------------------------------------------
PRInt32 nsZipArchive::CopyItemToBuffer(const nsZipItem* aItem, char* aOutBuf)
{
  PR_ASSERT(aOutBuf != 0 && aItem != 0);

  //-- Already at the correct point in the file
  //-- Read from file
  PRUint32 actual = PR_Read( mFd, aOutBuf, aItem->realsize);
  if (actual != aItem->realsize)
    return ZIP_ERR_CORRUPT;

  //-- verify crc32
  PRUint32 calculatedCRC = crc32(0L, Z_NULL, 0);
  calculatedCRC = crc32( calculatedCRC,(const unsigned char*)aOutBuf, 
                         aItem->realsize);
  if (calculatedCRC != aItem->crc32)
    return ZIP_ERR_CORRUPT;
  return ZIP_OK;
}


//---------------------------------------------
// nsZipArchive::CopyItemToDisk
//---------------------------------------------
PRInt32 nsZipArchive::CopyItemToDisk(const nsZipItem* aItem, PRFileDesc* fOut)
{
  PRInt32     status = ZIP_OK;
  PRUint32    chunk, pos, size, crc;

  PR_ASSERT( aItem != 0 && fOut != 0 );

  char* buf = (char*)PR_Malloc(ZIP_BUFLEN);
  if ( buf == 0 )
    return ZIP_ERR_MEMORY;

  //-- We should already be at the correct spot in the archive.
  //-- SeekToItem did the seek().

  //-- initialize crc
  crc = crc32(0L, Z_NULL, 0);

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

    //-- incrementally update crc32
    crc = crc32(crc, (const unsigned char*)buf, chunk);

    if ( PR_Write( fOut, buf, chunk ) < (READTYPE)chunk ) 
    {
      //-- Couldn't write all the data (disk full?)
      status = ZIP_ERR_DISK;
      break;
    }
  }

  //-- verify crc32
  if ( (status == ZIP_OK) && (crc != aItem->crc32) )
      status = ZIP_ERR_CORRUPT;

  PR_FREEIF( buf );
  return status;
}


//---------------------------------------------
// nsZipArchive::InflateItem
//---------------------------------------------
PRInt32 nsZipArchive::InflateItem( const nsZipItem* aItem, PRFileDesc* fOut,
                                   char* bigBuf )
/*
 * This function either inflates an archive item to disk, to the 
 * file specified by aOutname, or into a buffer specified by
 * bigBuf. bigBuf then gets copied into the "real" output
 * buffer a chunk at a time by ReadInflatedItem(). Memory-wise,
 * this is inefficient, since it stores an entire copy of the
 * decompressed item in memory, then copies it to the caller's
 * buffer. A more memory-efficient implementation is possible,
 * in which the outbuf in this function could be returned
 * directly, but implementing it would be complex.
 */
{
  PRInt32     status = ZIP_OK;    
  PRUint32    chunk, inpos, outpos, size, crc;
  PRUint32    bigBufSize;
  z_stream    zs;
  int         zerr;
  PRBool      bInflating = PR_FALSE;
  PRBool      bRead;
  PRBool      bWrote;
  PRBool      bToFile;
  Bytef*      old_next_out;

#if defined STANDALONE && defined WIN32
  MSG msg;
#endif /* STANDALONE */

  // -- if aOutname is null, we'll be writing to a buffer instead of a file
  if (fOut != 0)
  {
    PR_ASSERT( aItem != 0 );
    bToFile = PR_TRUE;
  }
  else
  {
    // -- Writing to a buffer, so bigBuf must not be null.
    PR_ASSERT( aItem != 0 && bigBuf != 0 );
    bToFile = PR_FALSE;
    bigBufSize = aItem->realsize;
  }
  
  //-- allocate deflation buffers
  Bytef *inbuf  = (Bytef*)PR_Malloc(ZIP_BUFLEN);
  Bytef *outbuf = (Bytef*)PR_Malloc(ZIP_BUFLEN);
  if ( inbuf == 0 || outbuf == 0 )
  {
    status = ZIP_ERR_MEMORY;
    goto cleanup;
  }
  
  //-- We should already be at the correct spot in the archive.
  //-- SeekToItem did the seek().

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
  crc = crc32(0L, Z_NULL, 0);
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
      if (bToFile)
      {
        //-- write inflated buffer to disk and make space
        if ( PR_Write( fOut, outbuf, ZIP_BUFLEN ) < ZIP_BUFLEN ) 
        {
          //-- Couldn't write all the data (disk full?)
          status = ZIP_ERR_DISK;
          break;
        }
      }
      else
      {
        //-- copy inflated buffer to our big buffer
        // Assertion makes sure we don't overflow bigBuf
        PR_ASSERT( outpos + ZIP_BUFLEN <= bigBufSize);
        char* copyStart = bigBuf + outpos;
        memcpy(copyStart, outbuf, ZIP_BUFLEN);
      }  
      
      outpos = zs.total_out;
      zs.next_out  = outbuf;
      zs.avail_out = ZIP_BUFLEN;
      bWrote       = PR_TRUE;
    }

    if(bRead || bWrote)
    {
      old_next_out = zs.next_out;
       
      zerr = inflate( &zs, Z_PARTIAL_FLUSH );
      
      //-- incrementally update crc32
      crc = crc32(crc, (const unsigned char*)old_next_out, zs.next_out - old_next_out);
    }
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

  //-- verify crc32
  if ( (status == ZIP_OK) && (crc != aItem->crc32) )
  {
      status = ZIP_ERR_CORRUPT;
      goto cleanup;
  } 

  //-- write last inflated bit to disk
  if ( zerr == Z_STREAM_END && outpos < zs.total_out )
  {
    chunk = zs.total_out - outpos;
    if (bToFile)
    {
      if ( PR_Write( fOut, outbuf, chunk ) < (READTYPE)chunk ) 
        status = ZIP_ERR_DISK;
    }
    else
    {
      PR_ASSERT( (outpos + chunk) <= bigBufSize );
        char* copyStart = bigBuf + outpos;
        memcpy(copyStart, outbuf, chunk);
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

  PR_FREEIF( inbuf );
  PR_FREEIF( outbuf );
  return status;
}

//------------------------------------------
// nsZipArchive constructor and destructor
//------------------------------------------

#ifndef STANDALONE
MOZ_DECL_CTOR_COUNTER(nsZipArchive);
#endif

nsZipArchive::nsZipArchive()
  : kMagic(ZIP_MAGIC), mFd(0)
{
#ifndef STANDALONE
  MOZ_COUNT_CTOR(nsZipArchive);
#endif

  // initialize the table to NULL
  for ( int i = 0; i < ZIP_TABSIZE; ++i) {
    mFiles[i] = 0;
  }

}

nsZipArchive::~nsZipArchive()
{
  (void)CloseArchive();

#ifndef STANDALONE
  MOZ_COUNT_DTOR(nsZipArchive);
#endif
}




//------------------------------------------
// nsZipItem constructor and destructor
//------------------------------------------

#ifndef STANDALONE
MOZ_DECL_CTOR_COUNTER(nsZipItem);
#endif

nsZipItem::nsZipItem() : name(0), next(0) 
{
#ifndef STANDALONE
  MOZ_COUNT_CTOR(nsZipItem);
#endif
}

nsZipItem::~nsZipItem()
{
  if (name != 0 )
  {
    delete [] name;
    name = 0;
  }

#ifndef STANDALONE
  MOZ_COUNT_DTOR(nsZipItem);
#endif
}

//------------------------------------------
// nsZipRead constructor and destructor
//------------------------------------------

#ifndef STANDALONE
MOZ_DECL_CTOR_COUNTER(nsZipRead);
#endif

nsZipRead::nsZipRead( nsZipArchive* aZipArchive, nsZipItem* aZipItem )
: mArchive(aZipArchive), 
  mItem(aZipItem), 
  mCurPos(0), 
  mFileBuffer(0)
{
#ifndef STANDALONE
  MOZ_COUNT_CTOR(nsZipRead);
#endif
}

nsZipRead::~nsZipRead()
{
  PR_FREEIF(mFileBuffer);

#ifndef STANDALONE
  MOZ_COUNT_DTOR(nsZipRead);
#endif
}

//------------------------------------------
// nsZipFind constructor and destructor
//------------------------------------------

#ifndef STANDALONE
MOZ_DECL_CTOR_COUNTER(nsZipFind);
#endif

nsZipFind::nsZipFind( nsZipArchive* aZip, char* aPattern, PRBool aRegExp )
: kMagic(ZIPFIND_MAGIC), 
  mArchive(aZip), 
  mPattern(aPattern), 
  mSlot(0), 
  mItem(0), 
  mRegExp(aRegExp) 
{
#ifndef STANDALONE
  MOZ_COUNT_CTOR(nsZipFind);
#endif
}

nsZipFind::~nsZipFind()
{
  if (mPattern != 0 )
    PL_strfree( mPattern );

#ifndef STANDALONE
  MOZ_COUNT_DTOR(nsZipFind);
#endif
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

/*
 * ExtractMode
 *
 * Extracts bits 17-24 from a 32-bit unsigned long
 * representation of the external attributes field.
 * Subsequently it tacks on the implicit user-read
 * bit.
 */
static PRUint16 ExtractMode(PRUint32 ext_attr)
{
    ext_attr &= 0x00FF0000; 
    ext_attr >>= 16;
    ext_attr |= 0x00000100;

    return (PRUint16) ext_attr;
}

/*
 *  d o s d a t e
 *
 *  Based on standard MS-DOS format date.
 *  Tweaked to be Y2K compliant and NSPR friendly.
 */
static void dosdate (char *aOutDateStr, PRUint16 aDate)
  {
  PRUint16 y2kCompliantYear = (aDate >> 9) + 1980;

  sprintf (aOutDateStr, "%02d/%02d/%02d",
     ((aDate >> 5) & 0x0F), (aDate & 0x1F), y2kCompliantYear);

  return;
  }

/*
 *  d o s t i m e
 *
 *  Standard MS-DOS format time.
 */
static void dostime (char *aOutTimeStr, PRUint16 aTime)
  {
  sprintf (aOutTimeStr, "%02d:%02d",
     ((aTime >> 11) & 0x1F), ((aTime >> 5) & 0x3F));

  return;
  }

char *
nsZipItem::GetModTime()
{
	char *timestr;    /* e.g. 21:07                        */
	char *datestr;    /* e.g. 06/20/1995                   */
	char *nsprstr;    /* e.g. 06/20/1995 21:07             */
	                  /* NSPR bug parsing dd/mm/yyyy hh:mm */
	                  /*        so we use mm/dd/yyyy hh:mm */
	
	timestr = (char *) PR_Malloc(6 * sizeof(char));
	datestr = (char *) PR_Malloc(11 * sizeof(char));
	nsprstr = (char *) PR_Malloc(17 * sizeof(char));
	if (!timestr || !datestr || !nsprstr)
	{
        PR_FREEIF(timestr);
        PR_FREEIF(datestr);
        PR_FREEIF(nsprstr);
		return 0;
	}

	memset(timestr, 0, 6);
	memset(datestr, 0, 11);
	memset(nsprstr, 0, 17);

	dosdate(datestr, this->date);
	dostime(timestr, this->time);

	sprintf(nsprstr, "%s %s", datestr, timestr);

    PR_FREEIF(timestr);
    PR_FREEIF(datestr);

	return nsprstr;
}

