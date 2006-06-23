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
 *   Jeff Walden <jwalden+code@mit.edu>
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
#define ZFILE_CREATE    PR_WRONLY | PR_CREATE_FILE
#define READTYPE  PRInt32
#include "zlib.h"
#include "nsISupportsUtils.h"
#include "nsRecyclingAllocator.h"

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
#define ZIP_ARENABLOCKSIZE (1*1024)

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
#elif defined(XP_BEOS)
    #include <unistd.h>
#endif

#ifndef XP_UNIX /* we need some constants defined in limits.h and unistd.h */
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
static PRUint16 ExtractMode(unsigned char *ll);
static PRUint32 HashName(const char* aName);
#if defined(XP_UNIX) || defined(XP_BEOS)
static PRBool IsSymlink(unsigned char *ll);
static nsresult ResolveSymlink(const char *path);
#endif

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

  PRFileDesc * fd = PR_Open(zipname, PR_RDONLY, 0400);
  if (!fd)
    return ZIP_ERR_DISK;

  status = zip->OpenArchive(fd);
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
  return zip->Test(NULL);
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

  //-- Find item in archive
  nsZipItem* item = zip->GetItem(filename);
  if (!item)
    return ZIP_ERR_FNF;

  // Can't extract a directory
  if (item->isDirectory)
    return ZIP_ERR_PARAM;

  // delete any existing file so that we overwrite the file permissions         
  PR_Delete(outname);
                                                                    
  PRFileDesc* fOut = PR_Open(outname, ZFILE_CREATE, item->mode);
  if (!fOut)
    return ZIP_ERR_DISK;

#if defined(XP_UNIX) && defined(STANDALONE)
  // When STANDALONE is defined, PR_Open ignores its 3d argument.
  mode_t msk = umask(0);
  umask(msk);
  chmod(outname, (item->mode | S_IRUSR) & ~msk);
#endif

  // ExtractFile also closes the fOut handle and resolves the symlink if needed
  return zip->ExtractFile(item, outname, fOut);
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
  nsZipFind* find;
  PRInt32 rv = zip->FindInit(pattern, &find);
  if (rv != ZIP_OK)
      find = NULL;

  return find;
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
  const char* itemName;
  status = find->FindNext(&itemName);
  if (status == ZIP_OK)
  {
    PRUint16 namelen = (PRUint16)PL_strlen(itemName);

    if (bufsize > namelen)
    {
        PL_strcpy(outbuf, itemName);
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
  delete find;
  return ZIP_OK;
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

nsresult gZlibInit(z_stream *zs)
{
  memset(zs, 0, sizeof(z_stream));
#ifndef STANDALONE
  //-- ensure we have our zlib allocator for better performance
  if (!gZlibAllocator) {
    gZlibAllocator = new nsRecyclingAllocator(NBUCKETS, NS_DEFAULT_RECYCLE_TIMEOUT, "libjar");
  }
  if (gZlibAllocator) {
    zs->zalloc = zlibAlloc;
    zs->zfree = zlibFree;
    zs->opaque = gZlibAllocator;
  }
#endif /* STANDALONE */
  int zerr = inflateInit2(zs, -MAX_WBITS);
  if (zerr != Z_OK) return ZIP_ERR_MEMORY;

  return ZIP_OK;
}

//***********************************************************
//      nsZipArchive  --  public methods
//***********************************************************


//---------------------------------------------
//  nsZipArchive::OpenArchive
//---------------------------------------------
nsresult nsZipArchive::OpenArchive(PRFileDesc * fd)
{
  if (!fd)
    return ZIP_ERR_PARAM;

#ifndef STANDALONE
  // Initialize our arena
  PL_INIT_ARENA_POOL(&mArena, "ZipArena", ZIP_ARENABLOCKSIZE);
#endif

  //-- Keep the filedescriptor for further reading...
  mFd = fd;

  //-- get table of contents for archive
  return BuildFileList();
}

//---------------------------------------------
//  nsZipArchive::Test
//---------------------------------------------
nsresult nsZipArchive::Test(const char *aEntryName)
{
  nsZipItem* currItem;

  if (aEntryName) // only test specified item
  {
    currItem = GetItem(aEntryName);
    if (!currItem)
      return ZIP_ERR_FNF;
    //-- don't test synthetic items -- I think for a normal zip
    //-- you actually *can* get away with testing a synthetic
    //-- with no ill results except some unnecessary work, but
    //-- for a zip which doesn't start with a zip entry (e.g.,
    //-- a self-extracting zip) it'll fail
    if (currItem->isSynthetic)
      return ZIP_OK;
    return ExtractFile(currItem, 0, 0);
  }

  // test all items in archive
  for (int i = 0; i < ZIP_TABSIZE; i++) {
    for (currItem = mFiles[i]; currItem; currItem = currItem->next) {
      if (currItem->isSynthetic || currItem->isDirectory)
        continue;
      nsresult rv = ExtractFile(currItem, 0, 0);
      if (rv != ZIP_OK)
        return rv;
#if defined STANDALONE && defined XP_WIN
      ProcessWindowsMessages();
#endif
    }
  }

  return ZIP_OK;
}

//---------------------------------------------
//  nsZipArchive::CloseArchive
//---------------------------------------------
nsresult nsZipArchive::CloseArchive()
{
#ifndef STANDALONE
  if (mFd) {
    PL_FinishArenaPool(&mArena);
  }

  // CAUTION:
  // We don't need to delete each of the nsZipItem as the memory for
  // the zip item and the filename it holds are both allocated from the Arena.
  // Hence, destroying the Arena is like destroying all the memory
  // for all the nsZipItem in one shot. But if the ~nsZipItem is doing
  // anything more than cleaning up memory, we should start calling it.
  // Let us also cleanup the mFiles table for re-use on the next 'open' call
  for (int i = 0; i < ZIP_TABSIZE; i++) {
    mFiles[i] = 0;
  }
#else
  // delete nsZipItems in table
  nsZipItem* pItem;
  for (int i = 0; i < ZIP_TABSIZE; ++i)
  {
    pItem = mFiles[i];
    while (pItem != 0)
    {
      mFiles[i] = pItem->next;
      free(pItem);
      pItem = mFiles[i];
    }
    mFiles[i] = 0;              // make sure we don't double-delete
  }
#endif
  
  if (mFd) {
    PR_Close(mFd);
    mFd = 0;
  }
  return ZIP_OK;
}

//---------------------------------------------
// nsZipArchive::GetItem
//---------------------------------------------
nsZipItem*  nsZipArchive::GetItem(const char * aEntryName)
{
  if (aEntryName) {
    nsZipItem* item = mFiles[ HashName(aEntryName) ];
    while (item) {
      if (!strcmp(aEntryName, item->name))
        return item; //-- found it
      item = item->next;
    }
  }
  return 0;
}

//---------------------------------------------
// nsZipArchive::ExtractFile
// This extracts the item to the filehandle provided.
// If 'aFd' is null, it only tests the extraction.
// On extraction error(s) it removes the file.
// When needed, it also resolves the symlink.
//---------------------------------------------
nsresult nsZipArchive::ExtractFile(nsZipItem *item, const char *outname,
                                   PRFileDesc* aFd)
{
  if (!item)
    return ZIP_ERR_PARAM;
  if (!mFd)
    return ZIP_ERR_GENERAL;

  // Directory extraction is handled in nsJAR::Extract,
  // so the item to be extracted should never be a directory
  PR_ASSERT(!item->isDirectory);

  //-- move to the start of file's data
  if (SeekToItem(item, mFd) != ZIP_OK)
    return ZIP_ERR_CORRUPT;

  nsresult rv;

  //-- extract the file using the appropriate method
  switch(item->compression)
  {
    case STORED:
      rv = CopyItemToDisk(item->size, item->crc32, aFd);
      break;

    case DEFLATED:
      rv = InflateItem(item, aFd);
      break;

    default:
      //-- unsupported compression type
      rv = ZIP_ERR_UNSUPPORTED;
  }

  //-- delete the file on errors, or resolve symlink if needed
  if (aFd) {
    PR_Close(aFd);
    if (rv != ZIP_OK)
      PR_Delete(outname);
#if defined(XP_UNIX) || defined(XP_BEOS)
    else if (item->isSymlink)
      rv = ResolveSymlink(outname);
#endif
  }

  return rv;
}

//---------------------------------------------
// nsZipArchive::FindInit
//---------------------------------------------
PRInt32
nsZipArchive::FindInit(const char * aPattern, nsZipFind **aFind)
{
  if (!aFind)
    return ZIP_ERR_PARAM;

  // null out param in case an error happens
  *aFind = NULL;

  PRBool  regExp = PR_FALSE;
  char*   pattern = 0;

  // validate the pattern
  if (aPattern)
  {
    switch (NS_WildCardValid((char*)aPattern))
    {
      case INVALID_SXP:
        return ZIP_ERR_PARAM;

      case NON_SXP:
        regExp = PR_FALSE;
        break;

      case VALID_SXP:
        regExp = PR_TRUE;
        break;

      default:
        // undocumented return value from RegExpValid!
        PR_ASSERT(PR_FALSE);
        return ZIP_ERR_PARAM;
    }

    pattern = PL_strdup(aPattern);
    if (!pattern)
      return ZIP_ERR_MEMORY;
  }

  *aFind = new nsZipFind(this, pattern, regExp);
  if (!*aFind)
    return ZIP_ERR_MEMORY;

  return ZIP_OK;
}



//---------------------------------------------
// nsZipFind::FindNext
//---------------------------------------------
nsresult nsZipFind::FindNext(const char ** aResult)
{
  PRBool     found = PR_FALSE;

  if (!mArchive || !aResult)
    return ZIP_ERR_PARAM;

  *aResult = 0;

  // we start from last match, look for next
  while (mSlot < ZIP_TABSIZE && !found)
  {
    // move to next in current chain, or move to new slot
    mItem = mItem ? mItem->next : mArchive->mFiles[mSlot];

    if (!mItem)
      ++mSlot;                          // no more in this chain, move to next slot
    else if (!mPattern)
      found = PR_TRUE;            // always match
    else if (mRegExp)
      found = (NS_WildCardMatch(mItem->name, mPattern, PR_FALSE) == MATCH);
    else
#if defined(STANDALONE) && defined(XP_MAC)
      // simulate <regexp>* matches
      found = (strncmp(mItem->name, mPattern, strlen(mPattern)) == 0);
#else
      found = (PL_strcmp(mItem->name, mPattern) == 0);
#endif

    // The way that the actual zip entry for a directory overrides a synthetic
    // entry created earlier means that a properly-constructed zip could return
    // the "same" entry twice during enumeration.  For example, adding foo/bar
    // and then foo/ to a new zip with Info-ZIP will return two entries for foo/
    // during any enumeration that finds foo/.  Here's how we solve the problem:
    //
    //   * non-synthetic items are always matches
    //   * a synthetic item is a match if for every prior item in
    //     the current chain, either of the following hold:
    //     * the prior item is not a directory (synthetic implies directory)
    //     * the prior item's name is different from the current item's name
    //
    // We test whether a prior item is a directory before comparing names
    // because name comparison involves function call overhead and because
    // the typical zip contains more files than directories.
    if (found && mItem->isSynthetic)
    {
      for (nsZipItem* curr = mArchive->mFiles[mSlot]; curr != mItem; curr = curr->next)
      {
        if (curr->isDirectory  && (0 == strcmp(mItem->name, curr->name)))
        {
          // we already found the real item with this name -- skip this item
          found = PR_FALSE;
          break;
        }
      }
    }
  }

  if (!found)
    return ZIP_ERR_FNF;

  *aResult = mItem->name;
  return ZIP_OK;
}

#if defined(XP_UNIX) || defined(XP_BEOS)
//---------------------------------------------
// ResolveSymlink
//---------------------------------------------
static nsresult ResolveSymlink(const char *path)
{
  PRFileDesc * fIn = PR_Open(path, PR_RDONLY, 0000);
  if (!fIn)
    return ZIP_ERR_DISK;

  char buf[PATH_MAX+1];
  PRInt32 length = PR_Read(fIn, (void*)buf, PATH_MAX);
  PR_Close(fIn);

  if ( (length <= 0)
    || ((buf[length] = 0, PR_Delete(path)) != 0)
    || (symlink(buf, path) != 0))
  {
     return ZIP_ERR_DISK;
  }
  return ZIP_OK;
}
#endif

//***********************************************************
//      nsZipArchive  --  private implementation
//***********************************************************

#define BR_BUF_SIZE 1024 /* backward read buffer size */

//---------------------------------------------
//  nsZipArchive::CreateZipItem
//---------------------------------------------
nsZipItem* nsZipArchive::CreateZipItem(PRUint16 namelen)
{
  // sizeof(nsZipItem) includes space for name's null byte
#ifndef STANDALONE
  // Arena allocate the nsZipItem
  void *mem;
  PL_ARENA_ALLOCATE(mem, &mArena, sizeof(nsZipItem)+namelen);
  return (nsZipItem*)mem;
#else
  return (nsZipItem*)malloc(sizeof(nsZipItem)+namelen);
#endif
}

//---------------------------------------------
//  nsZipArchive::BuildFileList
//---------------------------------------------
nsresult nsZipArchive::BuildFileList()
{
  PRUint8   buf[4*BR_BUF_SIZE];

  //-----------------------------------------------------------------------
  // locate the central directory via the End record
  //-----------------------------------------------------------------------

  //-- get archive size using end pos
  PRInt32  pos = PR_Seek(mFd, 0, PR_SEEK_END);
#ifndef STANDALONE
  if (pos <= 0)
#else
  if (pos || ((pos = ftell(mFd)) <= 0))
#endif
    return ZIP_ERR_CORRUPT;

  PRBool bEndsigFound = PR_FALSE;
  while (!bEndsigFound)
  {
    //-- read backwards in 1K-sized chunks (unless file is less than 1K)
    PRInt32  bufsize = pos > BR_BUF_SIZE ? BR_BUF_SIZE : pos;
    pos -= bufsize;

    if (!ZIP_Seek(mFd, pos, PR_SEEK_SET))
      return ZIP_ERR_CORRUPT;

    if (PR_Read(mFd, buf, bufsize) != (READTYPE)bufsize)
      return ZIP_ERR_CORRUPT;

    //-- scan for ENDSIG
    PRUint8 *endp = buf + bufsize;
    for (endp -= ZIPEND_SIZE; endp >= buf; endp--)
    {
      if (xtolong(endp) == ENDSIG)
      { 
        //-- Seek to start of central directory
        PRInt32 central = xtolong(((ZipEnd *) endp)->offset_central_dir);
        if (!ZIP_Seek(mFd, central, PR_SEEK_SET))
          return ZIP_ERR_CORRUPT;

        bEndsigFound = PR_TRUE;
        break;
      }
    }

    if (bEndsigFound)
      break;

    if (pos <= 0)
      //-- We're at the beginning of the file, and still no sign
      //-- of the end signature.  File must be corrupted!
      return ZIP_ERR_CORRUPT;

    //-- backward read must overlap ZipEnd length
    pos += ZIPEND_SIZE;

  } /* while looking for end signature */


  //-------------------------------------------------------
  // read the central directory headers
  //-------------------------------------------------------
  PRInt32 byteCount = PR_Read(mFd, &buf, sizeof(buf));
  pos = 0;
  PRUint32 sig = xtolong(buf);
  while (sig == CENTRALSIG) {
    //-- make sure we've read enough
    if (byteCount - pos < ZIPCENTRAL_SIZE)
      return ZIP_ERR_CORRUPT;

    //-------------------------------------------------------
    // read the fixed-size data
    //-------------------------------------------------------
    ZipCentral* central = (ZipCentral*)(buf+pos);

    PRUint16 namelen = xtoint(central->filename_len);
    PRUint16 extralen = xtoint(central->extrafield_len);
    PRUint16 commentlen = xtoint(central->commentfield_len);

    nsZipItem* item = CreateZipItem(namelen);
    if (!item)
      return ZIP_ERR_MEMORY;

    item->headerOffset  = xtolong(central->localhdr_offset);
    item->dataOffset    = 0;
    item->size          = xtolong(central->size);
    item->realsize      = xtolong(central->orglen);
    item->crc32         = xtolong(central->crc32);
    item->time          = xtoint(central->time);
    item->date          = xtoint(central->date);
    item->isSynthetic   = PR_FALSE;
    item->hasDataOffset = PR_FALSE;
    item->compression   = (PRUint8)xtoint(central->method);
#if defined(DEBUG)
    /* Make sure our space optimization is non lossy. */
    PR_ASSERT(xtoint(central->method) == (PRUint16)item->compression);
#endif

    item->mode = ExtractMode(central->external_attributes);
#if defined(XP_UNIX) || defined(XP_BEOS)
    // Check if item is a symlink
    item->isSymlink = IsSymlink(central->external_attributes);
#endif

    pos += ZIPCENTRAL_SIZE;

    //-------------------------------------------------------
    // Make sure that remainder of this record (name, comments, extra)
    // and the next ZipCentral is all in the buffer
    //-------------------------------------------------------
    PRInt32 leftover = byteCount - pos;
    if (leftover < (namelen + extralen + commentlen + ZIPCENTRAL_SIZE)) {
      //-- not enough data left to process at top of loop.
      //-- move leftover and read more
      memcpy(buf, buf+pos, leftover);
      byteCount = leftover + PR_Read(mFd, buf+leftover, sizeof(buf)-leftover);
      pos = 0;
    }

    //-------------------------------------------------------
    // get the item name
    //-------------------------------------------------------
    memcpy(item->name, buf+pos, namelen);
    item->name[namelen] = 0;

    //-- an item whose name ends with '/' is a directory
    item->isDirectory = ('/' == item->name[namelen - 1]);

    //-- add item to file table
    //-- note that an explicit entry for a directory will override
    //-- a fake entry created for that directory (as in the case
    //-- of processing foo/bar.txt and then foo/) -- this will
    //-- preserve an explicit directory's metadata at the cost of
    //-- an extra nsZipItem (and that only happens if we process a
    //-- file inside that directory before processing the directory
    //-- entry itself)
    PRUint32 hash = HashName(item->name);
    item->next = mFiles[hash];
    mFiles[hash] = item;

    //-- add entries for directories in the current item's path
    //-- go from end to beginning, because then we can stop trying
    //-- to create diritems if we find that the diritem we want to
    //-- create already exists
    //-- start just before the last char so as to not add the item
    //-- twice if it's a directory
    for (char* p = item->name + namelen - 2; p >= item->name; p--)
    {
      if ('/' != *p)
        continue;

      PRUint32 dirnamelen = p + 1 - item->name;

      // See whether we need to create any more implicit directories,
      // because if we don't we can avoid a lot of work.
      // We can even avoid (de)allocating space for a bogus dirname with
      // a little trickery -- save the char at item->name[dirnamelen],
      // set it to 0, compare the strings, and restore the saved
      // char when done
      char savedChar = item->name[dirnamelen];
      item->name[dirnamelen] = 0;

      // Is the directory in the file table?
      PRUint32 hash = HashName(item->name);
      PRBool done = PR_FALSE;
      for (nsZipItem* zi = mFiles[hash]; zi != NULL; zi = zi->next)
      {
        if (0 == strcmp(item->name, zi->name))
        {
          // we've already added this dir and all its parents
          done = PR_TRUE;
          break;
        }
      }

      // restore the char immediately
      item->name[dirnamelen] = savedChar;

      // if the directory was found, break out of the directory
      // creation loop now that we know all implicit directories
      // are there -- otherwise, start creating the zip item
      if (done)
        break;

      nsZipItem* diritem = CreateZipItem(dirnamelen);
      if (!diritem)
        return ZIP_ERR_MEMORY;

      memcpy(diritem->name, item->name, dirnamelen);
      diritem->name[dirnamelen] = 0;

      diritem->isDirectory = PR_TRUE;
      diritem->isSynthetic = PR_TRUE;
      diritem->compression = STORED;
      diritem->size = diritem->realsize = 0;
      diritem->crc32 = 0;
      diritem->mode = 0755;

      // Set an obviously wrong last-modified date/time, because
      // finding something more accurate like the most recent
      // last-modified date/time of the dir's contents is a lot
      // of effort.  The date/time corresponds to 1980-01-01 00:00.
      diritem->time = 0;
      diritem->date = 1 + (1 << 5) + (0 << 9);

      // add diritem to the file table
      diritem->next = mFiles[hash];
      mFiles[hash] = diritem;
    } /* end processing of dirs in item's name */

    //-------------------------------------------------------
    // set up to process the next item at the top of loop
    //-------------------------------------------------------
    pos += namelen + extralen + commentlen;
    sig = xtolong(buf+pos);
  } /* while reading central directory records */

  if (sig != ENDSIG)
    return ZIP_ERR_CORRUPT;

  return ZIP_OK;
}


//---------------------------------------------
// nsZipArchive::SeekToItem
//---------------------------------------------
nsresult  nsZipArchive::SeekToItem(nsZipItem* aItem, PRFileDesc* aFd)
{
  PR_ASSERT (aItem);

  //-- the first time an item is used we need to calculate its offset
  if (!aItem->hasDataOffset)
  {
    //-- read local header to get variable length values and calculate
    //-- the real data offset
    //--
    //-- NOTE: extralen is different in central header and local header
    //--       for archives created using the Unix "zip" utility. To set
    //--       the offset accurately we need the _local_ extralen.
    if (!ZIP_Seek(aFd, aItem->headerOffset, PR_SEEK_SET))
      return ZIP_ERR_CORRUPT;

    ZipLocal   Local;
    if ((PR_Read(aFd, (char*)&Local, ZIPLOCAL_SIZE) != (READTYPE) ZIPLOCAL_SIZE) || 
        (xtolong(Local.signature) != LOCALSIG))
    {
      //-- read error or local header not found
      return ZIP_ERR_CORRUPT;
    }

    aItem->dataOffset = aItem->headerOffset +
                        ZIPLOCAL_SIZE +
                        xtoint(Local.filename_len) +
                        xtoint(Local.extrafield_len);
    aItem->hasDataOffset = PR_TRUE;
  }

  //-- move to start of file in archive
  if (!ZIP_Seek(aFd, aItem->dataOffset, PR_SEEK_SET))
    return  ZIP_ERR_CORRUPT;

  return ZIP_OK;
}

//---------------------------------------------
// nsZipArchive::CopyItemToDisk
//---------------------------------------------
nsresult
nsZipArchive::CopyItemToDisk(PRUint32 itemSize, PRUint32 itemCrc, PRFileDesc* outFD)
/*
 * This function copies an archive item to disk, to the
 * file specified by outFD. If outFD is zero, the extracted data is
 * not written, only checked for CRC, so this is in effect same as 'Test'.
 */
{
  PRUint32    chunk, pos, crc;
  char buf[ZIP_BUFLEN];

  //-- initialize crc
  crc = crc32(0L, Z_NULL, 0);

  //-- copy chunks until file is done
  for (pos = 0; pos < itemSize; pos += chunk)
  {
    chunk = (itemSize - pos < ZIP_BUFLEN) ? (itemSize - pos) : ZIP_BUFLEN;
    
    if (PR_Read(mFd, buf, chunk) != (READTYPE)chunk)
    {
      //-- unexpected end of data in archive
      return ZIP_ERR_CORRUPT;
    }

    //-- incrementally update crc32
    crc = crc32(crc, (const unsigned char*)buf, chunk);

    if (outFD && PR_Write(outFD, buf, chunk) < (READTYPE)chunk)
    {
      //-- Couldn't write all the data (disk full?)
      return ZIP_ERR_DISK;
    }
  }

  //-- verify crc32
  if (crc != itemCrc)
      return ZIP_ERR_CORRUPT;

  return ZIP_OK;
}


//---------------------------------------------
// nsZipArchive::InflateItem
//---------------------------------------------
nsresult nsZipArchive::InflateItem(const nsZipItem* aItem, PRFileDesc* outFD)
/*
 * This function inflates an archive item to disk, to the
 * file specified by outFD. If outFD is zero, the extracted data is
 * not written, only checked for CRC, so this is in effect same as 'Test'.
 */
{
  PR_ASSERT(aItem);

  //-- allocate deflation buffers
  Bytef inbuf[ZIP_BUFLEN];
  Bytef outbuf[ZIP_BUFLEN];

  //-- set up the inflate
  z_stream    zs;
  nsresult status = gZlibInit(&zs);
  if (status != ZIP_OK)
    return ZIP_ERR_GENERAL;

  //-- inflate loop
  zs.next_out = outbuf;
  zs.avail_out = ZIP_BUFLEN;

  PRUint32  size = aItem->size;
  PRUint32  outpos = 0;
  PRUint32  crc = crc32(0L, Z_NULL, 0);
  int       zerr = Z_OK;
  while (zerr == Z_OK)
  {
    PRBool      bRead = PR_FALSE;
    PRBool      bWrote= PR_FALSE;

    if (zs.avail_in == 0 && zs.total_in < size)
    {
      //-- no data to inflate yet still more in file:
      //-- read another chunk of compressed data
      PRUint32 chunk = (size-zs.total_in < ZIP_BUFLEN) ? size-zs.total_in : ZIP_BUFLEN;

      if (PR_Read(mFd, inbuf, chunk) != (READTYPE)chunk)
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
      if (outFD && PR_Write(outFD, outbuf, ZIP_BUFLEN) < ZIP_BUFLEN)
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
      Bytef* old_next_out = zs.next_out;

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
    PRUint32 chunk = zs.total_out - outpos;
    if (outFD && PR_Write(outFD, outbuf, chunk) < (READTYPE)chunk)
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
  //-- free zlib internal state
  inflateEnd(&zs);

  return status;
}

//------------------------------------------
// nsZipArchive constructor and destructor
//------------------------------------------

#ifdef STANDALONE
nsZipArchive::nsZipArchive() : kMagic(ZIP_MAGIC), mFd(0)
#else
nsZipArchive::nsZipArchive() : mFd(0)
#endif
{
  MOZ_COUNT_CTOR(nsZipArchive);

  // initialize the table to NULL
  memset(mFiles, 0, sizeof mFiles);
}

nsZipArchive::~nsZipArchive()
{
  CloseArchive();

  MOZ_COUNT_DTOR(nsZipArchive);
}


//------------------------------------------
// nsZipFind constructor and destructor
//------------------------------------------

nsZipFind::nsZipFind(nsZipArchive* aZip, char* aPattern, PRBool aRegExp) : 
#ifdef STANDALONE
  kMagic(ZIPFIND_MAGIC),
#endif
  mArchive(aZip),
  mPattern(aPattern),
  mItem(0),
  mSlot(0),
  mRegExp(aRegExp)
{
  MOZ_COUNT_CTOR(nsZipFind);
}

nsZipFind::~nsZipFind()
{
  PR_FREEIF(mPattern);

  MOZ_COUNT_DTOR(nsZipFind);
}

//------------------------------------------
// helper functions
//------------------------------------------

/* 
 * HashName 
 *
 * returns a hash key for the entry name 
 */
static PRUint32 HashName(const char* aName)
{
  PR_ASSERT(aName != 0);

  PRUint32 val = 0;
  for (PRUint8* c = (PRUint8*)aName; *c != 0; c++) {
    val = val*37 + *c;
  }

  return (val % ZIP_TABSIZE);
}

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
  return (PRUint32)( (ll [0] <<  0) |
                     (ll [1] <<  8) |
                     (ll [2] << 16) |
                     (ll [3] << 24) );
}

/*
 * ExtractMode
 *
 * Extracts bits 17-24 from a 32-bit unsigned long
 * representation of the external attributes field.
 * Subsequently it tacks on the implicit user-read
 * bit.
 */
static PRUint16 ExtractMode(unsigned char *ll)
{
    return ((PRUint16)(ll[2])) | 0x0100;
}

#if defined(XP_UNIX) || defined(XP_BEOS)
/*
 *
 *  Return true if the attributes are for a symbolic link
 *
 */

static PRBool IsSymlink(unsigned char *ll)
{
  return ((xtoint(ll+2) & S_IFMT) == S_IFLNK);
}
#endif

