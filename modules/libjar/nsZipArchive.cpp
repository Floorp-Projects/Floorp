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
 * The Initial Developer of the Original Code is Netscape Communications  Corporation..
 * Portions created by Netscape Communications  Corporation. are
 * Copyright (C) 1998 Netscape Communications  Corporation.. All
 * Rights Reserved.
 *
 * Contributor(s):
 *     Daniel Veditz <dveditz@netscape.com>
 *     Samir Gehani <sgehani@netscape.com>
 *     Mitch Stoltz <mstoltz@netscape.com>
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

/* 
 * This module implements a simple archive extractor for the PKZIP format.
 *
 * The underlying nsZipArchive is NOT thread-safe. Do not pass references
 * or pointers to it across thread boundaries.
 */


#ifndef STANDALONE

#include "nsWildCard.h"
#include "nscore.h"
#include "prmem.h"
#include "prio.h"
#include "plstr.h"
#include "prlog.h"
#define ZFILE_CREATE    PR_WRONLY | PR_CREATE_FILE
#define READTYPE  PRInt32
#include "zlib.h"
#include "nsISupportsUtils.h"

#else /* STANDALONE */

#ifdef XP_WIN
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
 * ZIP_TestArchive
 *
 * Tests the integrity of this open zip archive by extracting each 
 * item to memory and performing a CRC check.
 *
 * @param   hZip      handle obtained from ZIP_OpenArchive
 * @return  status code (success indicated by ZIP_OK)
 */
PR_PUBLIC_API(PRInt32) ZIP_TestArchive( void *hZip )
{
  /*--- error check args ---*/
  if ( hZip == 0 )
    return ZIP_ERR_PARAM;
   
  nsZipArchive* zip = NS_STATIC_CAST(nsZipArchive*,hZip);
  if ( zip->kMagic != ZIP_MAGIC )
    return ZIP_ERR_PARAM;   /* whatever it is isn't one of ours! */

  /*--- test the archive ---*/
  return zip->Test(NULL);
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

#if defined XP_WIN
void ProcessWindowsMessages()
{
  MSG msg;

  while(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}
#endif /* XP_WIN */

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
//  nsZipArchive::Test
//---------------------------------------------
PRInt32 nsZipArchive::Test(const char *aEntryName)
{
  PRInt32 rv = ZIP_OK;
  nsZipItem *currItem = 0;

  if (aEntryName) // only test specified item
  {
    currItem = GetFileItem(aEntryName);
    if(!currItem)
      return ZIP_ERR_FNF;

    rv = TestItem(currItem);
  }
  else // test all items in archive
  {
    nsZipFind *iterator = FindInit( NULL );
    if (!iterator)
      return ZIP_ERR_GENERAL;

    // iterate over items in list
    while (ZIP_OK == FindNext(iterator, &currItem)) 
    {
      rv = TestItem(currItem);

      // check if crc check failed
      if (rv != ZIP_OK)
        break;

#if defined STANDALONE && defined XP_WIN
      ProcessWindowsMessages();
#endif
    }
  
    FindFree(iterator);
  }

  return rv;
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

  //-- find item
  nsZipItem* item = GetFileItem(zipEntry);
  if (!item)
    return ZIP_ERR_FNF; 

  //-- Create nsZipRead object
  *aRead = new nsZipRead(this, item);
  if (aRead == 0)
    return ZIP_ERR_MEMORY;

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
  
  //-- Find item in archive
  nsZipItem* item = GetFileItem( zipEntry );
  if (!item)
    return ZIP_ERR_FNF;

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
  PRBool  regExp = PR_FALSE;
  char*   pattern = 0;

  // validate the pattern
  if ( aPattern )
  {
    switch (NS_WildCardValid( (char*)aPattern ))
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
      found = (NS_WildCardMatch( item->name, aFind->mPattern, PR_FALSE ) == MATCH);
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

#define BR_BUF_SIZE 1024 /* backward read buffer size */

//---------------------------------------------
//  nsZipArchive::BuildFileList
//---------------------------------------------
PRInt32 nsZipArchive::BuildFileList()
{
  PRInt32   status = ZIP_OK;
  PRUint8   buf[4*BR_BUF_SIZE];

  ZipEnd     *End;

  //-----------------------------------------------------------------------
  // locate the central directory via the End record
  //-----------------------------------------------------------------------
  PRInt32  pos = 0L;
  PRInt32  bufsize = 0;

  //-- get archive size using end pos
  pos = PR_Seek(mFd, 0, PR_SEEK_END);
#ifndef STANDALONE
  if (pos <= 0)
#else
  if (pos || ((pos = ftell(mFd)) <= 0))
#endif
    status = ZIP_ERR_CORRUPT;

  while (status == ZIP_OK)
  {
    //-- read backwards in 1K-sized chunks (unless file is less than 1K)
    pos > BR_BUF_SIZE ? bufsize = BR_BUF_SIZE : bufsize = pos;
    pos -= bufsize;

    if ( !ZIP_Seek( mFd, pos, PR_SEEK_SET ) )
    {
      status = ZIP_ERR_CORRUPT;
      break;
    }

    if ( PR_Read( mFd, buf, bufsize ) != (READTYPE)bufsize ) 
    {
      status = ZIP_ERR_CORRUPT;
      break;
    }

    //-- scan for ENDSIG
    PRUint8 *endp = buf + bufsize;
    PRUint32 endsig;
    PRBool bEndsigFound = PR_FALSE;

    for (endp -= sizeof(ZipEnd); endp >= buf; endp--)
    {
		endsig = xtolong(endp);
		if (endsig == ENDSIG)
      {
        bEndsigFound = PR_TRUE;
        break;
      }
    }

    if (bEndsigFound)
    {
      End = (ZipEnd *) endp;
        
      //-- set pos to start of central directory
      pos = xtolong(End->offset_central_dir);
      if ( !ZIP_Seek( mFd, pos, PR_SEEK_SET ) )
        status = ZIP_ERR_CORRUPT;

      break;
    }

    if(pos <= 0)
      //-- We're at the beginning of the file, and still no sign
      //-- of the end signiture.  File must be corrupted!
      status = ZIP_ERR_CORRUPT;

    //-- backward read must overlap ZipEnd length
    pos += sizeof(ZipEnd);

  } /* while looking for end signature */


  //-------------------------------------------------------
  // read the central directory headers
  //-------------------------------------------------------
  if ( status == ZIP_OK )
  {
    //-- we think we found the central directory, read in the first chunk
    pos = 0;
    bufsize = PR_Read( mFd, &buf, sizeof(buf) );
    if (bufsize < sizeof(ZipCentral) + sizeof(ZipEnd))
    {
      // We know we read the end sig and got pointed at the central
      // directory--there should be at least this much
      //
      // (technically there can be a completely empty archive with only a
      // ZipEnd structure; that's pointless and might as well be an error.)
      status = ZIP_ERR_DISK;
    }

    //-- verify it's a central directory record
    if ( xtolong( buf ) != CENTRALSIG )
      status = ZIP_ERR_CORRUPT;
  }

  //-- loop through directory records
  //
  //-- we enter the loop positioned at a central directory record
  //-- with enough valid data in the buffer to contain one
  while ( status == ZIP_OK )
  {
    //-------------------------------------------------------
    // read the fixed-size data
    //-------------------------------------------------------
    ZipCentral* central = (ZipCentral*)(buf+pos);

    PRUint32 namelen = xtoint( central->filename_len );
    PRUint32 extralen = xtoint( central->extrafield_len );
    PRUint32 commentlen = xtoint( central->commentfield_len );
    
    nsZipItem* item = new nsZipItem();
    if (!item)
    {
      status = ZIP_ERR_MEMORY;
      break;
    }

    item->namelen = namelen;
    item->headerloc = xtolong( central->localhdr_offset );
    item->compression = xtoint( central->method );
    item->size = xtolong( central->size );
    item->realsize = xtolong( central->orglen );
    item->crc32 = xtolong( central->crc32 );
    item->mode = ExtractMode(xtolong( central->external_attributes )); 
    item->time = xtoint( central->time );
    item->date = xtoint( central->date );

    pos += sizeof(ZipCentral);

    //-------------------------------------------------------
    // get the item name
    //-------------------------------------------------------
    item->name = new char[namelen + 1];
    if ( !item->name )
    {
      status = ZIP_ERR_MEMORY;
      delete item;
      break;
    }

    PRUint32 leftover = (PRUint32)(bufsize - pos);
    if ( leftover < namelen )
    {
      //-- not enough data left in buffer for the name.
      //-- move leftover to top of buffer and read more
      memcpy( buf, buf+pos, leftover );
      bufsize = leftover + PR_Read( mFd, buf+leftover, bufsize-leftover );
      pos = 0;

      //-- make sure we were able to read enough
      if ( (PRUint32)bufsize < namelen )
      {
        status = ZIP_ERR_CORRUPT;
        break;
      }
    }
    memcpy( item->name, buf+pos, namelen );
    item->name[namelen] = 0;

    //-- add item to file table
    PRUint32 hash = HashName( item->name );
    item->next = mFiles[hash];
    mFiles[hash] = item;

    pos += namelen;

    //-------------------------------------------------------
    // set up to process the next item at the top of loop
    //-------------------------------------------------------
    leftover = (PRUint32)(bufsize - pos);
    if ( leftover < (extralen + commentlen + sizeof(ZipCentral)) )
    {
      //-- not enough data left to process at top of loop.
      //-- move leftover and read more
      memcpy( buf, buf+pos, leftover );
      bufsize = leftover + PR_Read( mFd, buf+leftover, bufsize-leftover );
      pos = 0;
    }
    //-- set position to start of next ZipCentral record
    pos += extralen + commentlen;
    
    PRUint32 sig = xtolong( buf+pos );
    if ( sig != CENTRALSIG )
    {
      //-- we must be done or else archive is corrupt
      if ( sig != ENDSIG )
        status = ZIP_ERR_CORRUPT;
      break;
    }

    //-- make sure we've read enough
    if ( bufsize < pos + sizeof(ZipCentral) )
    {
      status = ZIP_ERR_CORRUPT;
      break;
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
PRInt32  nsZipArchive::SeekToItem(const nsZipItem* aItem)
{
  PR_ASSERT (aItem);

  //-- the first time an item is used we need to calculate its offset
  if ( aItem->offset == 0 )
  {
    //-- read local header to extract local extralen
    //-- NOTE: extralen is different in central header and local header
    //--       for archives created using the Unix "zip" utility. To set 
    //--       the offset accurately we need the local extralen.
    if ( !ZIP_Seek( mFd, aItem->headerloc, PR_SEEK_SET ) )
      return ZIP_ERR_CORRUPT;
    
    ZipLocal   Local;
    if ( PR_Read(mFd, (char*)&Local, sizeof(ZipLocal)) != (READTYPE)sizeof(ZipLocal)
         || xtolong( Local.signature ) != LOCALSIG )
    { 
      //-- read error or local header not found
      return ZIP_ERR_CORRUPT;
    }

    ((nsZipItem*)aItem)->offset = aItem->headerloc + sizeof(Local) +
                                  xtoint( Local.filename_len ) +
                                  xtoint( Local.extrafield_len );
  }

  //-- move to start of file in archive
  if ( !ZIP_Seek( mFd, aItem->offset, PR_SEEK_SET ) )
    return  ZIP_ERR_CORRUPT;

  return ZIP_OK;
}

//---------------------------------------------
// nsZipArchive::CopyItemToBuffer
//---------------------------------------------
PRInt32 nsZipArchive::CopyItemToBuffer(const nsZipItem* aItem, char* aOutBuf)
{
  PR_ASSERT(aOutBuf != 0 && aItem != 0);

  //-- move to the start of file's data
  if ( SeekToItem( aItem ) != ZIP_OK )
    return ZIP_ERR_CORRUPT;
  
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

  //-- move to the start of file's data
  if ( SeekToItem( aItem ) != ZIP_OK )
    return ZIP_ERR_CORRUPT;
  
  char* buf = (char*)PR_Malloc(ZIP_BUFLEN);
  if ( buf == 0 )
    return ZIP_ERR_MEMORY;

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
  
  //-- move to the start of file's data
  if ( SeekToItem( aItem ) != ZIP_OK )
    return ZIP_ERR_CORRUPT;
  
  //-- allocate deflation buffers
  Bytef *inbuf  = (Bytef*)PR_Malloc(ZIP_BUFLEN);
  Bytef *outbuf = (Bytef*)PR_Malloc(ZIP_BUFLEN);
  if ( inbuf == 0 || outbuf == 0 )
  {
    status = ZIP_ERR_MEMORY;
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

#if defined STANDALONE && defined XP_WIN
    ProcessWindowsMessages();
#endif
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

//---------------------------------------------
// nsZipArchive::TestItem
//---------------------------------------------
PRInt32 nsZipArchive::TestItem( const nsZipItem* aItem )
{
  Bytef *inbuf = NULL, *outbuf = NULL, *old_next_out;
  PRUint32 size, chunk=0, inpos, crc;
  PRInt32 status = ZIP_OK; 
  int zerr = Z_OK;
  z_stream zs;
  PRBool bInflating = PR_FALSE;
  PRBool bRead;
  PRBool bWrote;

  //-- param checks
  if (!aItem)
    return ZIP_ERR_PARAM;
  if (aItem->compression != STORED && aItem->compression != DEFLATED)
    return ZIP_ERR_UNSUPPORTED;

  //-- move to the start of file's data
  if ( SeekToItem( aItem ) != ZIP_OK )
    return ZIP_ERR_CORRUPT;
  
  //-- allocate buffers
  inbuf = (Bytef *) PR_Malloc(ZIP_BUFLEN);
  if (aItem->compression == DEFLATED)
    outbuf = (Bytef *) PR_Malloc(ZIP_BUFLEN);

  if ( inbuf == 0 || (aItem->compression == DEFLATED && outbuf == 0) )
  {
    status = ZIP_ERR_MEMORY;
    goto cleanup;
  }

  //-- set up the inflate if DEFLATED
  if (aItem->compression == DEFLATED)
  {
    memset( &zs, 0, sizeof(zs) );
    zerr = inflateInit2( &zs, -MAX_WBITS );
    if ( zerr != Z_OK )
    {
      status = ZIP_ERR_GENERAL;
      goto cleanup;
    }
    else
    {
      zs.next_out = outbuf;
      zs.avail_out = ZIP_BUFLEN;
    }
    bInflating = PR_TRUE;
  }

  //-- initialize crc checksum
  crc = crc32(0L, Z_NULL, 0);

  size = aItem->size;
  inpos = 0;

  //-- read in ZIP_BUFLEN-sized chunks of item
  //-- inflating if item is DEFLATED
  while ( zerr == Z_OK )
  {
    bRead = PR_FALSE;  // used to check if new data to inflate
    bWrote = PR_FALSE; // used to reset zs.next_out to outbuf 
                       //   when outbuf fills up

    //-- read to inbuf
    if (aItem->compression == DEFLATED)
    {
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
        bRead = PR_TRUE;
      }

      if ( zs.avail_out == 0 )
      {
        //-- reuse output buffer
        zs.next_out = outbuf;
        zs.avail_out = ZIP_BUFLEN;
        bWrote = PR_TRUE; // mimic writing to disk/memory
      }
    }
    else
    {
      if (inpos < size)
      {
        //-- read a chunk in 
        chunk = ( inpos + ZIP_BUFLEN <= size ) ? ZIP_BUFLEN : size - inpos;

        if ( PR_Read( mFd, inbuf, chunk ) != (READTYPE)chunk )
        {
          //-- unexpected end of data
          status = ZIP_ERR_CORRUPT;
          break;
        }

        inpos += chunk;
      }
      else
      {
        //-- finished reading STORED item
        break;
      }
    }

    //-- inflate if item is DEFLATED
    if (aItem->compression == DEFLATED)
    {
      if (bRead || bWrote)
      {
        old_next_out = zs.next_out;

        zerr = inflate( &zs, Z_PARTIAL_FLUSH );

        //-- incrementally update crc checksum
        crc = crc32(crc, (const unsigned char*)old_next_out, zs.next_out - old_next_out); 
      }
      else
        zerr = Z_STREAM_END;
    }
    //-- else just use input buffer containing data from STORED item
    else
    {
      //-- incrementally update crc checksum
      crc = crc32(crc, (const unsigned char*)inbuf, chunk); 
    }
  }

  //-- convert zlib error to return value
  if ( status == ZIP_OK && zerr != Z_OK && zerr != Z_STREAM_END )
  {
    status = (zerr == Z_MEM_ERROR) ? ZIP_ERR_MEMORY : ZIP_ERR_CORRUPT;
    goto cleanup;
  }

  //-- verify computed crc checksum against header info crc 
  if (status == ZIP_OK && crc != aItem->crc32)
  {
    status = ZIP_ERR_CORRUPT;
  }

cleanup:
  if ( bInflating )
  {
    //-- free zlib internal state
    inflateEnd( &zs );
  }

  PR_FREEIF(inbuf);
  if (aItem->compression == DEFLATED)
    PR_FREEIF(outbuf);
           
  return status;
}

//------------------------------------------
// nsZipArchive constructor and destructor
//------------------------------------------

MOZ_DECL_CTOR_COUNTER(nsZipArchive)

nsZipArchive::nsZipArchive()
  : kMagic(ZIP_MAGIC), mFd(0)
{
  MOZ_COUNT_CTOR(nsZipArchive);

  // initialize the table to NULL
  for ( int i = 0; i < ZIP_TABSIZE; ++i) {
    mFiles[i] = 0;
  }

}

nsZipArchive::~nsZipArchive()
{
  (void)CloseArchive();

  MOZ_COUNT_DTOR(nsZipArchive);
}




//------------------------------------------
// nsZipItem constructor and destructor
//------------------------------------------

MOZ_DECL_CTOR_COUNTER(nsZipItem)

nsZipItem::nsZipItem() : name(0), offset(0), next(0) 
{
  MOZ_COUNT_CTOR(nsZipItem);
}

nsZipItem::~nsZipItem()
{
  if (name != 0 )
  {
    delete [] name;
    name = 0;
  }

  MOZ_COUNT_DTOR(nsZipItem);
}

//------------------------------------------
// nsZipRead constructor and destructor
//------------------------------------------

MOZ_DECL_CTOR_COUNTER(nsZipRead)

nsZipRead::nsZipRead( nsZipArchive* aZipArchive, nsZipItem* aZipItem )
: mArchive(aZipArchive), 
  mItem(aZipItem), 
  mCurPos(0), 
  mFileBuffer(0)
{
  MOZ_COUNT_CTOR(nsZipRead);
}

nsZipRead::~nsZipRead()
{
  PR_FREEIF(mFileBuffer);

  MOZ_COUNT_DTOR(nsZipRead);
}

//------------------------------------------
// nsZipFind constructor and destructor
//------------------------------------------

MOZ_DECL_CTOR_COUNTER(nsZipFind)

nsZipFind::nsZipFind( nsZipArchive* aZip, char* aPattern, PRBool aRegExp )
: kMagic(ZIPFIND_MAGIC), 
  mArchive(aZip), 
  mPattern(aPattern), 
  mSlot(0), 
  mItem(0), 
  mRegExp(aRegExp) 
{
  MOZ_COUNT_CTOR(nsZipFind);
}

nsZipFind::~nsZipFind()
{
  if (mPattern != 0 )
    PL_strfree( mPattern );

  MOZ_COUNT_DTOR(nsZipFind);
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

