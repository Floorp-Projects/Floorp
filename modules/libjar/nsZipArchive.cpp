/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Veditz <dveditz@netscape.com>
 *   Samir Gehani <sgehani@netscape.com>
 *   Mitch Stoltz <mstoltz@netscape.com>
 *   Jeroen Dobbelaere <jeroen.dobbelaere@acunia.com>
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
#include "prprf.h"
#define ZFILE_CREATE    PR_WRONLY | PR_CREATE_FILE
#define READTYPE  PRInt32
#include "zlib.h"
#include "nsISupportsUtils.h"
#include "nsRecyclingAllocator.h"
#include "nsPrintfCString.h"
/**
 * Globals
 *
 * Global allocator used with zlib. Destroyed in module shutdown.
 */
#define NBUCKETS 6
#define BY4ALLOC_ITEMS 320
nsRecyclingAllocator *gZlibAllocator = NULL;

// For placement new used for arena allocations of zip file list
#include NEW_H

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
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <limits.h>
    #include <unistd.h>
#elif defined(XP_WIN) || defined(XP_OS2)
    #include <io.h>
#endif

#ifndef XP_UNIX /* we need to have some constant defined in limits.h and unistd.h */
#  ifndef S_IFMT
#    define S_IFMT 0170000
#  endif
#  ifndef S_IFLNK
#    define S_IFLNK  0120000
#  endif
#  ifndef PATH_MAX
#    define PATH_MAX 1024
#  endif
#endif  /* XP_UNIX */

#include "zipfile.h"
#include "zipstruct.h"
#include "nsZipArchive.h"

static PRUint16 xtoint(unsigned char *ii);
static PRUint32 xtolong(unsigned char *ll);
static PRUint16 ExtractMode(PRUint32 ext_attr);
static PRBool   IsSymlink(PRUint32 ext_attr);

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
PR_PUBLIC_API(PRInt32) ZIP_OpenArchive(const char * zipname, void** hZip)
{
  PRInt32 status;

  /*--- error check args ---*/
  if (hZip == 0)
    return ZIP_ERR_PARAM;

  /*--- NULL output to prevent use by bozos who don't check errors ---*/
  *hZip = 0;

  /*--- create and open the archive ---*/
  nsZipArchive* zip = new nsZipArchive();
  if (zip == 0)
    return ZIP_ERR_MEMORY;

  status = zip->OpenArchive(zipname);

  if (status == ZIP_OK)
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
PR_PUBLIC_API(PRInt32) ZIP_TestArchive(void *hZip)
{
  /*--- error check args ---*/
  if (hZip == 0)
    return ZIP_ERR_PARAM;

  nsZipArchive* zip = NS_STATIC_CAST(nsZipArchive*,hZip);
  if (zip->kMagic != ZIP_MAGIC)
    return ZIP_ERR_PARAM;   /* whatever it is isn't one of ours! */

  /*--- test the archive ---*/
  return zip->Test(NULL, zip->GetFd());
}


/**
 * ZIP_CloseArchive
 *
 * closes zip archive and frees memory
 * @param   hZip  handle obtained from ZIP_OpenArchive
 * @return  status code
 */
PR_PUBLIC_API(PRInt32) ZIP_CloseArchive(void** hZip)
{
  /*--- error check args ---*/
  if (hZip == 0 || *hZip == 0)
    return ZIP_ERR_PARAM;

  nsZipArchive* zip = NS_STATIC_CAST(nsZipArchive*,*hZip);
  if (zip->kMagic != ZIP_MAGIC)
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
PR_PUBLIC_API(PRInt32) ZIP_ExtractFile(void* hZip, const char * filename, const char * outname)
{
  /*--- error check args ---*/
  if (hZip == 0)
    return ZIP_ERR_PARAM;

  nsZipArchive* zip = NS_STATIC_CAST(nsZipArchive*,hZip);
  if (zip->kMagic != ZIP_MAGIC)
    return ZIP_ERR_PARAM;   /* whatever it is isn't one of ours! */

  /*--- extract the file ---*/
  return zip->ExtractFile(filename, outname, zip->GetFd());
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
PR_PUBLIC_API(void*) ZIP_FindInit(void* hZip, const char * pattern)
{
  /*--- error check args ---*/
  if (hZip == 0)
    return 0;

  nsZipArchive* zip = NS_STATIC_CAST(nsZipArchive*,hZip);
  if (zip->kMagic != ZIP_MAGIC)
    return 0;   /* whatever it is isn't one of ours! */

  /*--- initialize the pattern search ---*/
  return zip->FindInit(pattern);
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
PR_PUBLIC_API(PRInt32) ZIP_FindNext(void* hFind, char * outbuf, PRUint16 bufsize)
{
  PRInt32 status;

  /*--- error check args ---*/
  if (hFind == 0)
    return ZIP_ERR_PARAM;

  nsZipFind* find = NS_STATIC_CAST(nsZipFind*,hFind);
  if (find->kMagic != ZIPFIND_MAGIC)
    return ZIP_ERR_PARAM;   /* whatever it is isn't one of ours! */

  /*--- return next filename file ---*/
  nsZipItem* item;
  status = find->GetArchive()->FindNext(find, &item);
  if (status == ZIP_OK)
  {
    PRUint16 namelen = (PRUint16)PL_strlen(item->name);

    if (bufsize > namelen)
    {
        PL_strcpy(outbuf, item->name);
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
PR_PUBLIC_API(PRInt32) ZIP_FindFree(void* hFind)
{
  /*--- error check args ---*/
  if (hFind == 0)
    return ZIP_ERR_PARAM;

  nsZipFind* find = NS_STATIC_CAST(nsZipFind*,hFind);
  if (find->kMagic != ZIPFIND_MAGIC)
    return ZIP_ERR_PARAM;   /* whatever it is isn't one of ours! */

  /* free the find structure */
  return find->GetArchive()->FindFree(find);
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

#else /* STANDALONE */

//***********************************************************
// Allocators for use with zlib
//
// These are allocators that are performance tuned for
// use with zlib. Our use of zlib for every file we read from
// the jar file when running navigator, we do these allocation.
// alloc 24
// alloc 64
// alloc 11520
// alloc 32768
// alloc 1216 [304x4] max
// alloc 76   [19x4]
// free  76   [19x4]
// alloc 1152 [288x4]
// free  1152 [288x4]
// free  1216 [304x4]
// alloc 28
// free  28
// free  32768
// free  11520
// free  64
// free  24
//
// The pool will allocate these as:
//
//          32,768
//          11,520
//           1,280 [320x4] - shared by first x4 alloc, 28
//           1,280 [320x4] - shared by second and third x4 alloc
//              64
//              24
//          ------
//          46,936
//
// And almost all of the file reads happen serially. Hence this
// allocator tries to keep one set of memory needed for one file around
// and reused the same blocks for other file reads.
//
// The interesting question is when should be free this ?
// - memory pressure should be one.
// - after startup of navigator
// - after startup of mail
// In general, this allocator should be enabled before
// we startup and disabled after we startup if memory is a concern.
//***********************************************************

PR_STATIC_CALLBACK(void *)
zlibAlloc(void *opaque, uInt items, uInt size)
{
  nsRecyclingAllocator *zallocator = (nsRecyclingAllocator *)opaque;
  if (zallocator) {
    // Bump up x4 allocations
    PRUint32 realitems = items;
    if (size == 4 && items < BY4ALLOC_ITEMS)
      realitems = BY4ALLOC_ITEMS;
     return zallocator->Calloc(realitems, size);
  }
  else
    return calloc(items, size);
}

PR_STATIC_CALLBACK(void)
zlibFree(void *opaque, void *ptr)
{
  nsRecyclingAllocator *zallocator = (nsRecyclingAllocator *)opaque;
  if (zallocator)
    zallocator->Free(ptr);
  else
    free(ptr);
  return;
}
#endif /* STANDALONE */

//***********************************************************
//      nsZipReadState  --  public methods
//***********************************************************

void nsZipReadState::Init(nsZipItem* aZipItem, PRFileDesc* aFd)
{
    PR_ASSERT(aFd);
    mItem = aZipItem;
    mCurPos = 0;
#ifndef STANDALONE
    // take ownership of the file descriptor
    mFd = aFd;
#endif

    if (mItem->compression != STORED) {
      memset(&mZs, 0, sizeof(mZs));

#ifndef STANDALONE
      //-- ensure we have our zlib allocator for better performance
      if (!gZlibAllocator) {
        gZlibAllocator = new nsRecyclingAllocator(NBUCKETS, NS_DEFAULT_RECYCLE_TIMEOUT, "libjar");
      }

      mZs.zalloc = zlibAlloc;
      mZs.zfree = zlibFree;
      mZs.opaque = gZlibAllocator;
#endif
      int zerr = inflateInit2(&mZs, -MAX_WBITS);
      PR_ASSERT(zerr == Z_OK);
    }
    mCrc = crc32(0L, Z_NULL, 0);
}

//***********************************************************
//      nsZipArchive  --  public methods
//***********************************************************


#ifdef STANDALONE
//---------------------------------------------
//  nsZipArchive::OpenArchive
//---------------------------------------------
PRInt32 nsZipArchive::OpenArchive(const char * aArchiveName)
{
  //-- validate arguments
  if (aArchiveName == 0 || *aArchiveName == '\0')
    return ZIP_ERR_PARAM;

  //-- open the physical file
  mFd = PR_Open(aArchiveName, PR_RDONLY, 0000);
  if (mFd == 0)
    return ZIP_ERR_DISK;

  //-- get table of contents for archive
  return BuildFileList(GetFd());
}

#else
PRInt32 nsZipArchive::OpenArchiveWithFileDesc(PRFileDesc* fd)
{
  //-- validate arguments
  if (fd == 0)
    return ZIP_ERR_PARAM;

  //-- get table of contents for archive
  return BuildFileList(fd);
}
#endif

//---------------------------------------------
//  nsZipArchive::Test
//---------------------------------------------
PRInt32 nsZipArchive::Test(const char *aEntryName, PRFileDesc* aFd)
{
  PRInt32 rv = ZIP_OK;
  nsZipItem *currItem = 0;

  if (aEntryName) // only test specified item
  {
    currItem = GetFileItem(aEntryName);
    if(!currItem)
      return ZIP_ERR_FNF;

    rv = TestItem(currItem, aFd);
  }
  else // test all items in archive
  {
    nsZipFind *iterator = FindInit(NULL);
    if (!iterator)
      return ZIP_ERR_GENERAL;

    // iterate over items in list
    while (ZIP_OK == FindNext(iterator, &currItem))
    {
      rv = TestItem(currItem, aFd);

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
  
#ifndef STANDALONE
  PL_FinishArenaPool(&mArena);

  // CAUTION:
  // We dont need to delete each of the nsZipItem as the memory for
  // the zip item and the filename it holds are both allocated from the Arena.
  // Hence, destroying the Arena is like destroying all the memory
  // for all the nsZipItem in one shot. But if the ~nsZipItem is doing
  // anything more than cleaning up memory, we should start calling it.
#else
  // delete nsZipItems in table
  nsZipItem* pItem;
  for (int i = 0; i < ZIP_TABSIZE; ++i)
  {
    pItem = mFiles[i];
    while (pItem != 0)
    {
      mFiles[i] = pItem->next;
      delete pItem;
      pItem = mFiles[i];
    }
    mFiles[i] = 0;              // make sure we don't double-delete
  }
  
  if (mFd) {
    PR_Close(mFd);
    mFd = 0;
  }
#endif

  return ZIP_OK;
}

//---------------------------------------------
// nsZipArchive::GetItem
//---------------------------------------------
PRInt32 nsZipArchive::GetItem(const char * aFilename, nsZipItem **result)
{
    //-- Parameter validity check
    if (aFilename == 0)
        return ZIP_ERR_PARAM;

    nsZipItem* item;

    //-- find file information
    item = GetFileItem(aFilename);
    if (item == 0)
    {
        return ZIP_ERR_FNF;
    }

    *result = item; // Return a pointer to the struct
    return ZIP_OK;
}

//---------------------------------------------
// nsZipArchive::ReadInit
//---------------------------------------------
PRInt32 nsZipArchive::ReadInit(const char* zipEntry, nsZipReadState* aRead,
                               PRFileDesc* aFd)
{
 
  //-- Parameter validity check
  if (zipEntry == 0 || aRead == 0)
    return ZIP_ERR_PARAM;

  //-- find item
  nsZipItem* item = GetFileItem(zipEntry);
  if (!item) {
    PR_Close(aFd);
    return ZIP_ERR_FNF;
  }

  //-- verify we can handle the compression type
  if (item->compression != DEFLATED && item->compression != STORED) {
    PR_Close(aFd);
    return ZIP_ERR_UNSUPPORTED;
  }

  SeekToItem(item, aFd);

#ifdef STANDALONE
  // in standalone, nsZipArchive owns the file descriptor
  mFd = aFd;
#endif
  
  // in non-standalone builds, the nsZipReadState will take ownership
  // of the file descriptor
  aRead->Init(item, aFd);
    
  return ZIP_OK;
}

//---------------------------------------------
// nsZipReadState::Available
//---------------------------------------------
PRUint32 nsZipReadState::Available()
{
  if (mItem->compression == DEFLATED)
    return (mItem->realsize - mZs.total_out);

  return mItem->size - mCurPos;
}

//---------------------------------------------
// nsZipArchive::ExtractFile
//---------------------------------------------
PRInt32 nsZipArchive::ExtractFile(const char* zipEntry, const char* aOutname,
                                  PRFileDesc* aFd)
{
  //-- Find item in archive
  nsZipItem* item = GetFileItem(zipEntry);
  if (!item)
    return ZIP_ERR_FNF;

  // delete any existing file so that we overwrite the file permissions
  PR_Delete(aOutname);

  PRFileDesc* fOut = PR_Open(aOutname, ZFILE_CREATE, item->mode);
  if (fOut == 0)
    return ZIP_ERR_DISK;

#if defined(XP_UNIX) && defined(STANDALONE)
  // When STANDALONE is defined, PR_Open ignores its 3d argument.
  mode_t msk = umask(0);
  umask(msk);
  chmod(aOutname, item->mode & ~msk);
#endif

  PRInt32 status = ExtractItemToFileDesc(item, fOut, aFd);
  PR_Close(fOut);

  if (status != ZIP_OK)
  {
    PR_Delete(aOutname);
  }
#if defined(XP_UNIX)
  else
  {
    if (ZIFLAG_SYMLINK & item->flags)
    {
      status = ResolveSymlink(aOutname, item);
    }
  }
#endif
  return status;
}

PRInt32
nsZipArchive::ExtractItemToFileDesc(nsZipItem* item, PRFileDesc* outFD,
                                    PRFileDesc* aFd)
{
  //-- sanity check arguments
  if (item == 0 || outFD == 0)
    return ZIP_ERR_PARAM;

  PRInt32 status;

  //-- extract the file using the appropriate method
  switch(item->compression)
  {
    case STORED:
      status = CopyItemToDisk(item, outFD, aFd);
      break;

    case DEFLATED:
      status = InflateItem(item, outFD, aFd);
      break;

    default:
      //-- unsupported compression type
      return ZIP_ERR_UNSUPPORTED;
  }

  return status;
}

//---------------------------------------------
// nsZipArchive::FindInit
//---------------------------------------------
nsZipFind* nsZipArchive::FindInit(const char * aPattern)
{
  PRBool  regExp = PR_FALSE;
  char*   pattern = 0;

  // validate the pattern
  if (aPattern)
  {
    switch (NS_WildCardValid((char*)aPattern))
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
        PR_ASSERT(PR_FALSE);
        return 0;
    }

    pattern = PL_strdup(aPattern);
    if (!pattern)
      return 0;
  }

  return new nsZipFind(this, pattern, regExp);
}



//---------------------------------------------
// nsZipArchive::FindNext
//---------------------------------------------
PRInt32 nsZipArchive::FindNext(nsZipFind* aFind, nsZipItem** aResult)
{
  PRInt32    status;
  PRBool     found  = PR_FALSE;
  PRUint16   slot   = aFind->mSlot;
  nsZipItem* item   = aFind->mItem;

  if (aFind->mArchive != this)
    return ZIP_ERR_PARAM;

  // we start from last match, look for next
  while (slot < ZIP_TABSIZE && !found)
  {
    if (item != 0)
      item = item->next;    // move to next in current chain
    else
      item = mFiles[slot];  // starting a new slot

    if (item == 0)
    { // no more in this chain, move to next slot
      ++slot;
      continue;
    }
    else if (aFind->mPattern == 0)
      found = PR_TRUE;            // always match
    else if (aFind->mRegExp)
      found = (NS_WildCardMatch(item->name, aFind->mPattern, PR_FALSE) == MATCH);
    else
#if defined(STANDALONE) && defined(XP_MAC)
      // simulate <regexp>* matches
      found = (strncmp(item->name, aFind->mPattern, strlen(aFind->mPattern)) == 0);
#else
      found = (PL_strcmp(item->name, aFind->mPattern) == 0);
#endif
  }

  if (found)
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
PRInt32 nsZipArchive::FindFree(nsZipFind* aFind)
{
  if (aFind->mArchive != this)
    return ZIP_ERR_PARAM;

  delete aFind;
  return ZIP_OK;
}

#ifdef XP_UNIX
//---------------------------------------------
// nsZipArchive::ResolveSymlink
//---------------------------------------------
PRInt32 nsZipArchive::ResolveSymlink(const char *path, nsZipItem *item)
{
  PRInt32    status = ZIP_OK;
  if (item->flags & ZIFLAG_SYMLINK)
  {
    char buf[PATH_MAX+1];
    PRFileDesc * fIn = PR_Open(path, PR_RDONLY, 0000);
    if (fIn)
    {
      PRInt32 length = PATH_MAX;
      length = PR_Read(fIn,(void*)buf,length);
      PR_Close(fIn);
      fIn = 0;
      if (  length <= 0
      || (buf[length] = 0, PR_Delete(path)) != 0
      || symlink(buf, path) != 0)
      {
        status = ZIP_ERR_DISK;
      }
    } else {
      status = ZIP_ERR_DISK;
    }
    if (fIn)
    {
      PR_Close(fIn);
    }
  }
  return status;
}
#endif

//***********************************************************
//      nsZipArchive  --  private implementation
//***********************************************************

#define BR_BUF_SIZE 1024 /* backward read buffer size */

//---------------------------------------------
//  nsZipArchive::BuildFileList
//---------------------------------------------
PRInt32 nsZipArchive::BuildFileList(PRFileDesc* aFd)
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
  pos = PR_Seek(aFd, 0, PR_SEEK_END);
#ifndef STANDALONE
  if (pos <= 0)
#else
  if (pos || ((pos = ftell(aFd)) <= 0))
#endif
    status = ZIP_ERR_CORRUPT;

  while (status == ZIP_OK)
  {
    //-- read backwards in 1K-sized chunks (unless file is less than 1K)
    pos > BR_BUF_SIZE ? bufsize = BR_BUF_SIZE : bufsize = pos;
    pos -= bufsize;

    if (!ZIP_Seek(aFd, pos, PR_SEEK_SET))
    {
      status = ZIP_ERR_CORRUPT;
      break;
    }

    if (PR_Read(aFd, buf, bufsize) != (READTYPE)bufsize)
    {
      status = ZIP_ERR_CORRUPT;
      break;
    }

    //-- scan for ENDSIG
    PRUint8 *endp = buf + bufsize;
    PRUint32 endsig;
    PRBool bEndsigFound = PR_FALSE;

    for (endp -= ZIPEND_SIZE; endp >= buf; endp--)
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
      if (!ZIP_Seek(aFd, pos, PR_SEEK_SET))
        status = ZIP_ERR_CORRUPT;

      break;
    }

    if(pos <= 0)
      //-- We're at the beginning of the file, and still no sign
      //-- of the end signiture.  File must be corrupted!
      status = ZIP_ERR_CORRUPT;

    //-- backward read must overlap ZipEnd length
    pos += ZIPEND_SIZE;

  } /* while looking for end signature */


  //-------------------------------------------------------
  // read the central directory headers
  //-------------------------------------------------------
  if (status == ZIP_OK)
  {
    //-- we think we found the central directory, read in the first chunk
    pos = 0;
    bufsize = PR_Read(aFd, &buf, sizeof(buf));
    if (bufsize < (PRInt32)(ZIPCENTRAL_SIZE + ZIPEND_SIZE))
    {
      // We know we read the end sig and got pointed at the central
      // directory--there should be at least this much
      //
      // (technically there can be a completely empty archive with only a
      // ZipEnd structure; that's pointless and might as well be an error.)
      status = ZIP_ERR_DISK;
    }

    //-- verify it's a central directory record
    if (xtolong(buf) != CENTRALSIG)
      status = ZIP_ERR_CORRUPT;
  }

  //-- loop through directory records
  //
  //-- we enter the loop positioned at a central directory record
  //-- with enough valid data in the buffer to contain one
  while (status == ZIP_OK)
  {
    //-------------------------------------------------------
    // read the fixed-size data
    //-------------------------------------------------------
    ZipCentral* central = (ZipCentral*)(buf+pos);

    PRUint32 namelen = xtoint(central->filename_len);
    PRUint32 extralen = xtoint(central->extrafield_len);
    PRUint32 commentlen = xtoint(central->commentfield_len);
#ifndef STANDALONE
    // Arena allocate the nsZipItem
    void *mem;
    PL_ARENA_ALLOCATE(mem, &mArena, sizeof(nsZipItem));
    // Use placement new to arena allcoate the nsZipItem
    nsZipItem* item = mem ? new (mem) nsZipItem() : nsnull;
#else
    nsZipItem* item = new nsZipItem();
#endif
    if (!item)
    {
      status = ZIP_ERR_MEMORY;
      break;
    }

    item->offset = xtolong(central->localhdr_offset);
    item->compression = (PRUint8)xtoint(central->method);
#if defined(DEBUG)
    /*
     * Make sure our space optimization is non lossy.
     */
    PR_ASSERT(xtoint(central->method) == (PRUint16)item->compression);
#endif
    item->size = xtolong(central->size);
    item->realsize = xtolong(central->orglen);
    item->crc32 = xtolong(central->crc32);
    PRUint32 external_attributes = xtolong(central->external_attributes);
    item->mode = ExtractMode(external_attributes);
    if (IsSymlink(external_attributes))
    {
      item->flags |= ZIFLAG_SYMLINK;
    }
    item->time = xtoint(central->time);
    item->date = xtoint(central->date);

    pos += ZIPCENTRAL_SIZE;

    //-------------------------------------------------------
    // get the item name
    //-------------------------------------------------------
#ifndef STANDALONE
    PL_ARENA_ALLOCATE(mem, &mArena, (namelen + 1));
    item->name = (char *) mem;
    if (!item->name)
    {
      status = ZIP_ERR_MEMORY;
      // No need to delete name. It gets deleted only when the entire arena
      // goes away.
      break;
    }
#else
    item->name = new char[namelen + 1];
#endif
    if (!item->name)
    {
      status = ZIP_ERR_MEMORY;
      delete item;
      break;
    }

    PRUint32 leftover = (PRUint32)(bufsize - pos);
    if (leftover < namelen)
    {
      //-- not enough data left in buffer for the name.
      //-- move leftover to top of buffer and read more
      memcpy(buf, buf+pos, leftover);
      bufsize = leftover + PR_Read(aFd, buf+leftover, bufsize-leftover);
      pos = 0;

      //-- make sure we were able to read enough
      if ((PRUint32)bufsize < namelen)
      {
        status = ZIP_ERR_CORRUPT;
        break;
      }
    }
    memcpy(item->name, buf+pos, namelen);
    item->name[namelen] = 0;

    //-- add item to file table
    PRUint32 hash = HashName(item->name);
    item->next = mFiles[hash];
    mFiles[hash] = item;

    pos += namelen;

    //-------------------------------------------------------
    // set up to process the next item at the top of loop
    //-------------------------------------------------------
    leftover = (PRUint32)(bufsize - pos);
    if (leftover < (extralen + commentlen + ZIPCENTRAL_SIZE))
    {
      //-- not enough data left to process at top of loop.
      //-- move leftover and read more
      memcpy(buf, buf+pos, leftover);
      bufsize = leftover + PR_Read(aFd, buf+leftover, bufsize-leftover);
      pos = 0;
    }
    //-- set position to start of next ZipCentral record
    pos += extralen + commentlen;

    PRUint32 sig = xtolong(buf+pos);
    if (sig != CENTRALSIG)
    {
      //-- we must be done or else archive is corrupt
      if (sig != ENDSIG)
        status = ZIP_ERR_CORRUPT;
      break;
    }

    //-- make sure we've read enough
    if (bufsize < pos + ZIPCENTRAL_SIZE)
    {
      status = ZIP_ERR_CORRUPT;
      break;
    }
  } /* while reading central directory records */

#if defined(DEBUG)
  if (status != ZIP_OK) {
    const char* msgs[] = { "ZIP_OK", "ZIP_ERR_GENERAL", "ZIP_ERR_MEMORY", "ZIP_ERR_DISK", "ZIP_ERR_CORRUPT", "ZIP_ERR_PARAM", "ZIP_ERR_FNF", "ZIP_ERR_UNSUPPORTED", "ZIP_ERR_SMALLBUF", "UNKNOWN" };
    printf("nsZipArchive::BuildFileList  status = %d '%s'\n", (int)status, msgs[(status <= 0 && status >= -8) ? -status : 9]);
  }
#endif

  return status;
}

//---------------------------------------------
// nsZipArchive::GetFileItem
//---------------------------------------------
nsZipItem*  nsZipArchive::GetFileItem(const char * zipEntry)
{
  PR_ASSERT(zipEntry != 0);

  nsZipItem* item = mFiles[ HashName(zipEntry) ];

  for (; item != 0; item = item->next)
  {
    if (0 == PL_strcmp(zipEntry, item->name))
      break; //-- found it
  }

  return item;
}


//---------------------------------------------
// nsZipArchive::HashName
//---------------------------------------------
PRUint32 nsZipArchive::HashName(const char* aName)
{
  PRUint32 val = 0;
  PRUint8* c;

  PR_ASSERT(aName != 0);

  for (c = (PRUint8*)aName; *c != 0; c++) {
    val = val*37 + *c;
  }

  return (val % ZIP_TABSIZE);
}

//---------------------------------------------
// nsZipArchive::SeekToItem
//---------------------------------------------
PRInt32  nsZipArchive::SeekToItem(const nsZipItem* aItem, PRFileDesc* aFd)
{
  PR_ASSERT (aItem);

  PRFileDesc* fd = aFd;
  
  //-- the first time an item is used we need to calculate its offset
  if (!(aItem->flags & ZIFLAG_DATAOFFSET))
  {
    //-- aItem->offset contains the header offset, not the data offset.
    //-- read local header to get variable length values and calculate
    //-- the real data offset
    //--
    //-- NOTE: extralen is different in central header and local header
    //--       for archives created using the Unix "zip" utility. To set
    //--       the offset accurately we need the _local_ extralen.
    if (!ZIP_Seek(fd, aItem->offset, PR_SEEK_SET))
      return ZIP_ERR_CORRUPT;

    ZipLocal   Local;
    if (PR_Read(fd, (char*)&Local, ZIPLOCAL_SIZE) != (READTYPE) ZIPLOCAL_SIZE
         || xtolong(Local.signature) != LOCALSIG)
    {
      //-- read error or local header not found
      return ZIP_ERR_CORRUPT;
    }

    ((nsZipItem*)aItem)->offset += ZIPLOCAL_SIZE +
                                   xtoint(Local.filename_len) +
                                   xtoint(Local.extrafield_len);
    ((nsZipItem*)aItem)->flags |= ZIFLAG_DATAOFFSET;
  }

  //-- move to start of file in archive
  if (!ZIP_Seek(fd, aItem->offset, PR_SEEK_SET))
    return  ZIP_ERR_CORRUPT;

  return ZIP_OK;
}

//---------------------------------------------
// nsZipArchive::CopyItemToDisk
//---------------------------------------------
PRInt32
nsZipArchive::CopyItemToDisk(const nsZipItem* aItem,
                             PRFileDesc* fOut, PRFileDesc* aFd)
{
  PRInt32     status = ZIP_OK;
  PRUint32    chunk, pos, size, crc;

  PR_ASSERT(aItem != 0 && fOut != 0);

  PRFileDesc* fd = aFd;
  
  //-- move to the start of file's data
  if (SeekToItem(aItem, aFd) != ZIP_OK)
    return ZIP_ERR_CORRUPT;

  char buf[ZIP_BUFLEN];

  //-- initialize crc
  crc = crc32(0L, Z_NULL, 0);

  //-- copy chunks until file is done
  size = aItem->size;
  for (pos=0; pos < size; pos += chunk)
  {
    chunk = (pos+ZIP_BUFLEN <= size) ? ZIP_BUFLEN : size - pos;

    if (PR_Read(fd, buf, chunk) != (READTYPE)chunk)
    {
      //-- unexpected end of data in archive
      status = ZIP_ERR_CORRUPT;
      break;
    }

    //-- incrementally update crc32
    crc = crc32(crc, (const unsigned char*)buf, chunk);

    if (PR_Write(fOut, buf, chunk) < (READTYPE)chunk)
    {
      //-- Couldn't write all the data (disk full?)
      status = ZIP_ERR_DISK;
      break;
    }
  }

  //-- verify crc32
  if ((status == ZIP_OK) && (crc != aItem->crc32))
      status = ZIP_ERR_CORRUPT;

  return status;
}

#ifndef STANDALONE
//------------------------------------------
// nsZipArchive::Read
//------------------------------------------
PRInt32
nsZipReadState::Read(char* aBuffer, PRUint32 aCount,
                     PRUint32* aBytesRead)
{
  if (!aBuffer)
    return ZIP_ERR_GENERAL;

  PRInt32 result;

  if (!Available()) {
    *aBytesRead = 0;
    return ZIP_OK;
  }

  switch (mItem->compression) {
  case DEFLATED:
    result = ContinueInflate(aBuffer, aCount, aBytesRead);
    break;
  case STORED:
    result = ContinueCopy(aBuffer, aCount, aBytesRead);
    break;
  default:
    result = ZIP_ERR_UNSUPPORTED;
  }

  // be agressive about closing!
  // note that sometimes, we will close mFd before we've finished
  // deflating - this is because zlib buffers the input
  if (mCurPos >= mItem->size && mFd) {
    PR_Close(mFd);
    mFd = NULL;
  }

  return result;
}

PRInt32
nsZipReadState::ContinueInflate(char* aBuffer, PRUint32 aCount,
                                PRUint32* aBytesRead)
{

  // just some stuff that will be helpful later
  const PRUint32 inSize = mItem->size;
  const PRUint32 outSize = mItem->realsize;

  int zerr = Z_OK;
  //-- inflate loop

  const PRUint32 oldTotalOut = mZs.total_out;

  mZs.next_out = (unsigned char*)aBuffer;
  mZs.avail_out = ((mZs.total_out + aCount) < outSize) ?
    aCount : (outSize - mZs.total_out);

  // make sure we aren't reading too much
  PR_ASSERT(mZs.avail_out <= aCount);
  
  *aBytesRead = 0;
  while (mZs.avail_out != 0 && zerr == Z_OK) {
    
    if (mZs.avail_in == 0 && mCurPos < inSize) {
      // time to fill the buffer!
      PRUint32 bytesToRead = ((mCurPos + ZIP_BUFLEN) < inSize) ?
        ZIP_BUFLEN : inSize - mCurPos;

      PR_ASSERT(mFd);
      PRInt32 bytesRead = PR_Read(mFd, mReadBuf, bytesToRead);
      if (bytesRead < 0) {
        zerr = Z_ERRNO;
        break;
      }

      mCrc = crc32(mCrc, mReadBuf, bytesRead);
      mCurPos += bytesRead;

      // now reset the state
      mZs.next_in = mReadBuf;
      mZs.avail_in = bytesRead;
    }

#if 0
    // stop returning valid data as soon as we know we have a bad CRC
    if (mCurPos >= inSize &&
        mCrc != mItem->crc32) {
      // asserting because while this rarely happens, you definitely
      // want to catch it in debug builds!
      PR_ASSERT(0);
      return ZIP_ERR_CORRUPT;
    }
#endif
    
    // now inflate
    zerr = inflate(&mZs, Z_PARTIAL_FLUSH);
  }

  if ((zerr != Z_OK) && (zerr != Z_STREAM_END))
    return ZIP_ERR_CORRUPT;
  
  *aBytesRead = (mZs.total_out - oldTotalOut);

  // be aggressive about closing the stream
  // for some reason we don't always get Z_STREAM_END
  if (zerr == Z_STREAM_END || mZs.total_out == mItem->realsize) {
    inflateEnd(&mZs);
  }

  return ZIP_OK;
}

PRInt32 nsZipReadState::ContinueCopy(char* aBuf,
                                     PRUint32 aCount,
                                     PRUint32* aBytesRead)
{
  // we still use the fields of mZs, we just use memcpy rather than inflate

  if (mCurPos + aCount > mItem->realsize)
    aCount = (mItem->realsize - mCurPos);

  PR_ASSERT(mFd);
  PRInt32 bytesRead = PR_Read(mFd, aBuf, aCount);
  if (bytesRead < 0)
    return ZIP_ERR_DISK;

  mCurPos += bytesRead;
  
  *aBytesRead = bytesRead;

  return ZIP_OK;
}

#endif

//---------------------------------------------
// nsZipArchive::InflateItem
//---------------------------------------------
PRInt32 nsZipArchive::InflateItem(const nsZipItem* aItem, PRFileDesc* fOut, PRFileDesc* aFd)
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
  PRUint32    bigBufSize=0;
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
    PR_ASSERT(aItem != 0);
    bToFile = PR_TRUE;
  }
  else
  {
    // -- Writing to a buffer, so bigBuf must not be null.
    PR_ASSERT(aItem);
    bToFile = PR_FALSE;
    bigBufSize = aItem->realsize;
  }

  //-- move to the start of file's data
  if (SeekToItem(aItem, aFd) != ZIP_OK)
    return ZIP_ERR_CORRUPT;

  //-- allocate deflation buffers
  Bytef inbuf[ZIP_BUFLEN];
  Bytef outbuf[ZIP_BUFLEN];

  //-- set up the inflate
  memset(&zs, 0, sizeof(zs));
#ifndef STANDALONE
  //-- ensure we have our zlib allocator for better performance
  if (!gZlibAllocator) {
    gZlibAllocator = new nsRecyclingAllocator(NBUCKETS, NS_DEFAULT_RECYCLE_TIMEOUT, "libjar");
  }

  zs.zalloc = zlibAlloc;
  zs.zfree = zlibFree;
  zs.opaque = gZlibAllocator;
#endif

  zerr = inflateInit2(&zs, -MAX_WBITS);
  if (zerr != Z_OK)
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
  while (zerr == Z_OK)
  {
    bRead  = PR_FALSE;
    bWrote = PR_FALSE;

    if (zs.avail_in == 0 && zs.total_in < size)
    {
      //-- no data to inflate yet still more in file:
      //-- read another chunk of compressed data

      inpos = zs.total_in;  // input position
      chunk = (inpos + ZIP_BUFLEN <= size) ? ZIP_BUFLEN : size - inpos;

      if (PR_Read(aFd, inbuf, chunk) != (READTYPE)chunk)
      {
        //-- unexpected end of data
        status = ZIP_ERR_CORRUPT;
        break;
      }

      zs.next_in  = inbuf;
      zs.avail_in = chunk;
      bRead       = PR_TRUE;
    }

    if (zs.avail_out == 0)
    {
      //-- write inflated buffer to disk and make space
      if (PR_Write(fOut, outbuf, ZIP_BUFLEN) < ZIP_BUFLEN)
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
    {
      old_next_out = zs.next_out;

      zerr = inflate(&zs, Z_PARTIAL_FLUSH);

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
  if ((status == ZIP_OK) && (crc != aItem->crc32))
  {
      status = ZIP_ERR_CORRUPT;
      goto cleanup;
  }

  //-- write last inflated bit to disk
  if (zerr == Z_STREAM_END && outpos < zs.total_out)
  {
    chunk = zs.total_out - outpos;
    if (PR_Write(fOut, outbuf, chunk) < (READTYPE)chunk)
      status = ZIP_ERR_DISK;
  }

  //-- convert zlib error to return value
  if (status == ZIP_OK && zerr != Z_OK && zerr != Z_STREAM_END)
  {
    status = (zerr == Z_MEM_ERROR) ? ZIP_ERR_MEMORY : ZIP_ERR_CORRUPT;
  }

  //-- if found no errors make sure we've converted the whole thing
  PR_ASSERT(status != ZIP_OK || zs.total_in == aItem->size);
  PR_ASSERT(status != ZIP_OK || zs.total_out == aItem->realsize);

cleanup:
  if (bInflating)
  {
    //-- free zlib internal state
    inflateEnd(&zs);
  }

  return status;
}

//---------------------------------------------
// nsZipArchive::TestItem
//---------------------------------------------
PRInt32 nsZipArchive::TestItem(const nsZipItem* aItem, PRFileDesc* aFd)
{
  Bytef inbuf[ZIP_BUFLEN], outbuf[ZIP_BUFLEN], *old_next_out;
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
  if (SeekToItem(aItem, aFd) != ZIP_OK)
    return ZIP_ERR_CORRUPT;

  //-- set up the inflate if DEFLATED
  if (aItem->compression == DEFLATED)
  {
    memset(&zs, 0, sizeof(zs));
    zerr = inflateInit2(&zs, -MAX_WBITS);
    if (zerr != Z_OK)
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
  while (zerr == Z_OK)
  {
    bRead = PR_FALSE;  // used to check if new data to inflate
    bWrote = PR_FALSE; // used to reset zs.next_out to outbuf
                       //   when outbuf fills up

    //-- read to inbuf
    if (aItem->compression == DEFLATED)
    {
      if (zs.avail_in == 0 && zs.total_in < size)
      {
        //-- no data to inflate yet still more in file:
        //-- read another chunk of compressed data

        inpos = zs.total_in;  // input position
        chunk = (inpos + ZIP_BUFLEN <= size) ? ZIP_BUFLEN : size - inpos;

        if (PR_Read(aFd, inbuf, chunk) != (READTYPE)chunk)
        {
          //-- unexpected end of data
          status = ZIP_ERR_CORRUPT;
          break;
        }

        zs.next_in  = inbuf;
        zs.avail_in = chunk;
        bRead = PR_TRUE;
      }

      if (zs.avail_out == 0)
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
        chunk = (inpos + ZIP_BUFLEN <= size) ? ZIP_BUFLEN : size - inpos;

        if (PR_Read(aFd, inbuf, chunk) != (READTYPE)chunk)
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

        zerr = inflate(&zs, Z_PARTIAL_FLUSH);

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
  if (status == ZIP_OK && zerr != Z_OK && zerr != Z_STREAM_END)
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
  if (bInflating)
  {
    //-- free zlib internal state
    inflateEnd(&zs);
  }

  return status;
}

//------------------------------------------
// nsZipArchive constructor and destructor
//------------------------------------------

MOZ_DECL_CTOR_COUNTER(nsZipArchive)

nsZipArchive::nsZipArchive()
  : kMagic(ZIP_MAGIC), kArenaBlockSize(1*1024)
#ifdef STANDALONE
    , mFd(0)
#endif
{
  MOZ_COUNT_CTOR(nsZipArchive);

  // initialize the table to NULL
  memset(mFiles, 0, sizeof mFiles);

#ifndef STANDALONE
  // Initialize our arena
  PL_INIT_ARENA_POOL(&mArena, "ZipArena", kArenaBlockSize);
#endif
}

nsZipArchive::~nsZipArchive()
{
  CloseArchive();

  MOZ_COUNT_DTOR(nsZipArchive);
}




//------------------------------------------
// nsZipItem constructor and destructor
//------------------------------------------

nsZipItem::nsZipItem() : name(0), offset(0), next(0), flags(0)
{
}

nsZipItem::~nsZipItem()
{
#ifdef STANDALONE
  if (name != 0)
  {
    delete [] name;
    name = 0;
  }
#endif
}

//------------------------------------------
// nsZipReadState
//------------------------------------------

MOZ_DECL_CTOR_COUNTER(nsZipReadState)

//------------------------------------------
// nsZipFind constructor and destructor
//------------------------------------------

MOZ_DECL_CTOR_COUNTER(nsZipFind)

nsZipFind::nsZipFind(nsZipArchive* aZip, char* aPattern, PRBool aRegExp)
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
  if (mPattern != 0)
    PL_strfree(mPattern);

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
 *
 *  Return true if the attributes are for a symbolic link
 *
 */

static PRBool IsSymlink(PRUint32 ext_attr)
{
  return (((ext_attr>>16) & S_IFMT) == S_IFLNK) ? PR_TRUE : PR_FALSE;
}

#ifndef STANDALONE
PRTime
nsZipItem::GetModTime()
{
  char buffer[17];

  PRUint16 aDate = this->date;
  PRUint16 aTime = this->time;

  PR_snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d %02d:%02d",
        ((aDate >> 5) & 0x0F), (aDate & 0x1F), (aDate >> 9) + 1980,
        ((aTime >> 11) & 0x1F), ((aTime >> 5) & 0x3F));

  PRTime result;
  PR_ParseTimeString(buffer, PR_FALSE, &result);
  return result;
}
#endif
