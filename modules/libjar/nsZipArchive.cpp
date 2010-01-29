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
 *   Taras Glek <tglek@mozilla.com>
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

#define READTYPE  PRInt32
#include "zlib.h"
#include "nsISupportsUtils.h"
#include "nsRecyclingAllocator.h"
#include "prio.h"
#include "plstr.h"
#include "prlog.h"
#include "stdlib.h"
#include "nsWildCard.h"
#include "nsZipArchive.h"

/**
 * Global allocator used with zlib. Destroyed in module shutdown.
 */
#define NBUCKETS 6
nsRecyclingAllocator *gZlibAllocator = NULL;

// For placement new used for arena allocations of zip file list
#include NEW_H
#define ZIP_ARENABLOCKSIZE (1*1024)

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

#ifdef __SYMBIAN32__
    #include <sys/syslimits.h>
#endif /*__SYMBIAN32__*/


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


static const PRUint32 kMaxNameLength = PATH_MAX; /* Maximum name length */
// For synthetic zip entries. Date/time corresponds to 1980-01-01 00:00.
static const PRUint16 kSyntheticTime = 0;
static const PRUint16 kSyntheticDate = (1 + (1 << 5) + (0 << 9));

static PRUint16 xtoint(const PRUint8 *ii);
static PRUint32 xtolong(const PRUint8 *ll);
static PRUint32 HashName(const char* aName, PRUint16 nameLen);
#if defined(XP_UNIX) || defined(XP_BEOS)
static nsresult ResolveSymlink(const char *path);
#endif

//***********************************************************
// Allocators for use with zlib
//
// Use a recycling allocator, for re-use of of the zlib buffers.
// For every inflation the following allocations are done:
// zlibAlloc(1, 9520)
// zlibAlloc(32768, 1)
//***********************************************************

static void *
zlibAlloc(void *opaque, uInt items, uInt size)
{
  nsRecyclingAllocator *zallocator = (nsRecyclingAllocator *)opaque;
  if (zallocator) {
    return gZlibAllocator->Malloc(items * size);
  }
  return malloc(items * size);
}

static void
zlibFree(void *opaque, void *ptr)
{
  nsRecyclingAllocator *zallocator = (nsRecyclingAllocator *)opaque;
  if (zallocator)
    zallocator->Free(ptr);
  else
    free(ptr);
}

nsresult gZlibInit(z_stream *zs)
{
  memset(zs, 0, sizeof(z_stream));
  //-- ensure we have our zlib allocator for better performance
  if (!gZlibAllocator) {
    gZlibAllocator = new nsRecyclingAllocator(NBUCKETS, NS_DEFAULT_RECYCLE_TIMEOUT, "libjar");
  }
  if (gZlibAllocator) {
    zs->zalloc = zlibAlloc;
    zs->zfree = zlibFree;
    zs->opaque = gZlibAllocator;
  }
  int zerr = inflateInit2(zs, -MAX_WBITS);
  if (zerr != Z_OK) return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

nsZipHandle::nsZipHandle()
  : mFileData(nsnull)
  , mLen(0)
  , mMap(nsnull)
  , mRefCnt(0)
{
  MOZ_COUNT_CTOR(nsZipHandle);
}

NS_IMPL_THREADSAFE_ADDREF(nsZipHandle)
NS_IMPL_THREADSAFE_RELEASE(nsZipHandle)

nsresult nsZipHandle::Init(PRFileDesc *fd, nsZipHandle **ret)
{
  PRInt64 size = PR_Available64(fd);
  if (size >= PR_INT32_MAX)
    return NS_ERROR_FILE_TOO_BIG;

  PRFileMap *map = PR_CreateFileMap(fd, size, PR_PROT_READONLY);
  if (!map)
    return NS_ERROR_FAILURE;
  
  PRUint8 *buf = (PRUint8*) PR_MemMap(map, 0, (PRUint32) size);
  // Bug 525755: PR_MemMap fails when fd points at something other than a normal file.
  if (!buf) {
    PR_CloseFileMap(map);
    return NS_ERROR_FAILURE;
  }

  nsZipHandle *handle = new nsZipHandle();
  if (!handle) {
    PR_MemUnmap(buf, size);
    PR_CloseFileMap(map);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  handle->mMap = map;
  handle->mLen = (PRUint32) size;
  handle->mFileData = buf;
  handle->AddRef();
  *ret = handle;
  return NS_OK;
}

nsZipHandle::~nsZipHandle()
{
  if (mFileData) {
    PR_MemUnmap(mFileData, mLen);
    PR_CloseFileMap(mMap);
    mFileData = nsnull;
    mMap = nsnull;
  }
  MOZ_COUNT_DTOR(nsZipHandle);
}

//***********************************************************
//      nsZipArchive  --  public methods
//***********************************************************


//---------------------------------------------
//  nsZipArchive::OpenArchive
//---------------------------------------------
nsresult nsZipArchive::OpenArchive(nsIFile *aZipFile)
{
  nsresult rv;
  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(aZipFile, &rv);
  if (NS_FAILED(rv)) return rv;

  PRFileDesc* fd;
  rv = localFile->OpenNSPRFileDesc(PR_RDONLY, 0000, &fd);
  if (NS_FAILED(rv)) return rv;

  rv = nsZipHandle::Init(fd, getter_AddRefs(mFd));
  PR_Close(fd);
  if (NS_FAILED(rv))
    return rv;

  // Initialize our arena
  PL_INIT_ARENA_POOL(&mArena, "ZipArena", ZIP_ARENABLOCKSIZE);

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
      return NS_ERROR_FILE_TARGET_DOES_NOT_EXIST;
    //-- don't test (synthetic) directory items
    if (currItem->IsDirectory())
      return NS_OK;
    return ExtractFile(currItem, 0, 0);
  }

  // test all items in archive
  for (int i = 0; i < ZIP_TABSIZE; i++) {
    for (currItem = mFiles[i]; currItem; currItem = currItem->next) {
      //-- don't test (synthetic) directory items
      if (currItem->IsDirectory())
        continue;
      nsresult rv = ExtractFile(currItem, 0, 0);
      if (rv != NS_OK)
        return rv;
    }
  }

  return NS_OK;
}

//---------------------------------------------
//  nsZipArchive::CloseArchive
//---------------------------------------------
nsresult nsZipArchive::CloseArchive()
{
  if (mFd) {
    PL_FinishArenaPool(&mArena);
    mFd = NULL;
  }

  // CAUTION:
  // We don't need to delete each of the nsZipItem as the memory for
  // the zip item and the filename it holds are both allocated from the Arena.
  // Hence, destroying the Arena is like destroying all the memory
  // for all the nsZipItem in one shot. But if the ~nsZipItem is doing
  // anything more than cleaning up memory, we should start calling it.
  // Let us also cleanup the mFiles table for re-use on the next 'open' call
  memset(mFiles, 0, sizeof(mFiles));
  mBuiltSynthetics = false;
  return NS_OK;
}

//---------------------------------------------
// nsZipArchive::GetItem
//---------------------------------------------
nsZipItem*  nsZipArchive::GetItem(const char * aEntryName)
{
  if (aEntryName) {
    PRUint32 len = strlen(aEntryName);
    //-- If the request is for a directory, make sure that synthetic entries 
    //-- are created for the directories without their own entry.
    if (!mBuiltSynthetics) {
        if ((len > 0) && (aEntryName[len-1] == '/')) {
            if (BuildSynthetics() != NS_OK)
                return 0;
        }
    }

    nsZipItem* item = mFiles[ HashName(aEntryName, len) ];
    while (item) {
      if ((len == item->nameLength) && 
         (!memcmp(aEntryName, item->Name(), len)))
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
    return NS_ERROR_ILLEGAL_VALUE;
  if (!mFd)
    return NS_ERROR_FAILURE;

  // Directory extraction is handled in nsJAR::Extract,
  // so the item to be extracted should never be a directory
  PR_ASSERT(!item->IsDirectory());

  nsresult rv;

  //-- extract the file using the appropriate method
  switch(item->Compression())
  {
    case STORED:
      rv = CopyItemToDisk(item, aFd);
      break;

    case DEFLATED:
      rv = InflateItem(item, aFd);
      break;

    default:
      //-- unsupported compression type
      rv = NS_ERROR_NOT_IMPLEMENTED;
  }

  //-- delete the file on errors, or resolve symlink if needed
  if (aFd) {
    PR_Close(aFd);
    if (rv != NS_OK)
      PR_Delete(outname);
#if defined(XP_UNIX) || defined(XP_BEOS)
    else if (item->IsSymlink())
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
    return NS_ERROR_ILLEGAL_VALUE;

  // null out param in case an error happens
  *aFind = NULL;

  PRBool  regExp = PR_FALSE;
  char*   pattern = 0;

  // Create synthetic directory entries on demand
  nsresult rv = BuildSynthetics();
  if (rv != NS_OK)
    return rv;

  // validate the pattern
  if (aPattern)
  {
    switch (NS_WildCardValid((char*)aPattern))
    {
      case INVALID_SXP:
        return NS_ERROR_ILLEGAL_VALUE;

      case NON_SXP:
        regExp = PR_FALSE;
        break;

      case VALID_SXP:
        regExp = PR_TRUE;
        break;

      default:
        // undocumented return value from RegExpValid!
        PR_ASSERT(PR_FALSE);
        return NS_ERROR_ILLEGAL_VALUE;
    }

    pattern = PL_strdup(aPattern);
    if (!pattern)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  *aFind = new nsZipFind(this, pattern, regExp);
  if (!*aFind) {
    PL_strfree(pattern);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}



//---------------------------------------------
// nsZipFind::FindNext
//---------------------------------------------
nsresult nsZipFind::FindNext(const char ** aResult, PRUint16 *aNameLen)
{
  if (!mArchive || !aResult || !aNameLen)
    return NS_ERROR_ILLEGAL_VALUE;

  *aResult = 0;
  *aNameLen = 0;

  // we start from last match, look for next
  while (mSlot < ZIP_TABSIZE)
  {
    // move to next in current chain, or move to new slot
    mItem = mItem ? mItem->next : mArchive->mFiles[mSlot];

    PRBool found = PR_FALSE;
    if (!mItem)
      ++mSlot;                          // no more in this chain, move to next slot
    else if (!mPattern)
      found = PR_TRUE;            // always match
    else if (mRegExp)
    {
      char buf[kMaxNameLength+1];
      memcpy(buf, mItem->Name(), mItem->nameLength);
      buf[mItem->nameLength]='\0';
      found = (NS_WildCardMatch(buf, mPattern, PR_FALSE) == MATCH);
    }
    else
      found = ((mItem->nameLength == strlen(mPattern)) &&
               (memcmp(mItem->Name(), mPattern, mItem->nameLength) == 0));
    if (found) {
      // Need also to return the name length, as it is NOT zero-terminatdd...
      *aResult = mItem->Name();
      *aNameLen = mItem->nameLength;
      return NS_OK;
    }
  }

  return NS_ERROR_FILE_TARGET_DOES_NOT_EXIST;
}

#if defined(XP_UNIX) || defined(XP_BEOS)
//---------------------------------------------
// ResolveSymlink
//---------------------------------------------
static nsresult ResolveSymlink(const char *path)
{
  PRFileDesc * fIn = PR_Open(path, PR_RDONLY, 0000);
  if (!fIn)
    return NS_ERROR_FILE_DISK_FULL;

  char buf[PATH_MAX+1];
  PRInt32 length = PR_Read(fIn, (void*)buf, PATH_MAX);
  PR_Close(fIn);

  if ( (length <= 0)
    || ((buf[length] = 0, PR_Delete(path)) != 0)
    || (symlink(buf, path) != 0))
  {
     return NS_ERROR_FILE_DISK_FULL;
  }
  return NS_OK;
}
#endif

//***********************************************************
//      nsZipArchive  --  private implementation
//***********************************************************

//---------------------------------------------
//  nsZipArchive::CreateZipItem
//---------------------------------------------
nsZipItem* nsZipArchive::CreateZipItem()
{
  // Arena allocate the nsZipItem
  void *mem;
  PL_ARENA_ALLOCATE(mem, &mArena, sizeof(nsZipItem));
  return (nsZipItem*)mem;
}

//---------------------------------------------
//  nsZipArchive::BuildFileList
//---------------------------------------------
nsresult nsZipArchive::BuildFileList()
{
  // Get archive size using end pos
  PRUint8* buf;
  PRUint8* startp = mFd->mFileData;
  PRUint8* endp = startp + mFd->mLen;

  for (buf = endp - ZIPEND_SIZE; xtolong(buf) != ENDSIG; buf--)
  {
    if (buf == startp) {
      // We're at the beginning of the file, and still no sign
      // of the end signature.  File must be corrupted!
      return NS_ERROR_FILE_CORRUPTED;
    }
  }
  PRUint32 centralOffset = xtolong(((ZipEnd *)buf)->offset_central_dir);

  //-- Read the central directory headers
  buf = startp + centralOffset;
  if (endp - buf < PRInt32(sizeof(PRUint32)))
      return NS_ERROR_FILE_CORRUPTED;
  PRUint32 sig = xtolong(buf);
  while (sig == CENTRALSIG) {
    // Make sure there is enough data available.
    if (endp - buf < ZIPCENTRAL_SIZE)
      return NS_ERROR_FILE_CORRUPTED;

    // Read the fixed-size data.
    ZipCentral* central = (ZipCentral*)buf;

    PRUint16 namelen = xtoint(central->filename_len);
    PRUint16 extralen = xtoint(central->extrafield_len);
    PRUint16 commentlen = xtoint(central->commentfield_len);

    // Point to the next item at the top of loop
    buf += ZIPCENTRAL_SIZE + namelen + extralen + commentlen;

    // Sanity check variable sizes and refuse to deal with
    // anything too big: it's likely a corrupt archive.
    if (namelen > kMaxNameLength || buf >= endp)
      return NS_ERROR_FILE_CORRUPTED;

    nsZipItem* item = CreateZipItem();
    if (!item)
      return NS_ERROR_OUT_OF_MEMORY;

    item->central = central;
    item->nameLength = namelen;
    item->isSynthetic = false;

    // Add item to file table
    PRUint32 hash = HashName(item->Name(), namelen);
    item->next = mFiles[hash];
    mFiles[hash] = item;

    sig = xtolong(buf);
  } /* while reading central directory records */

  if (sig != ENDSIG)
    return NS_ERROR_FILE_CORRUPTED;
  return NS_OK;
}

//---------------------------------------------
//  nsZipArchive::BuildSynthetics
//---------------------------------------------
nsresult nsZipArchive::BuildSynthetics()
{
  if (mBuiltSynthetics)
    return NS_OK;
  mBuiltSynthetics = true;

  // Create synthetic entries for any missing directories.
  // Do this when all ziptable has scanned to prevent double entries.
  for (int i = 0; i < ZIP_TABSIZE; ++i)
  {
    for (nsZipItem* item = mFiles[i]; item != 0; item = item->next)
    {
      if (item->isSynthetic)
        continue;
    
      //-- add entries for directories in the current item's path
      //-- go from end to beginning, because then we can stop trying
      //-- to create diritems if we find that the diritem we want to
      //-- create already exists
      //-- start just before the last char so as to not add the item
      //-- twice if it's a directory
      PRUint16 namelen = item->nameLength;
      const char *name = item->Name();
      for (PRUint16 dirlen = namelen - 1; dirlen > 0; dirlen--)
      {
        if (name[dirlen-1] != '/')
          continue;

        // Is the directory already in the file table?
        PRUint32 hash = HashName(item->Name(), dirlen);
        PRBool found = PR_FALSE;
        for (nsZipItem* zi = mFiles[hash]; zi != NULL; zi = zi->next)
        {
          if ((dirlen == zi->nameLength) &&
              (0 == memcmp(item->Name(), zi->Name(), dirlen)))
          {
            // we've already added this dir and all its parents
            found = PR_TRUE;
            break;
          }
        }
        // if the directory was found, break out of the directory
        // creation loop now that we know all implicit directories
        // are there -- otherwise, start creating the zip item
        if (found)
          break;

        nsZipItem* diritem = CreateZipItem();
        if (!diritem)
          return NS_ERROR_OUT_OF_MEMORY;

        // Point to the central record of the original item for the name part.
        diritem->central =  item->central;
        diritem->nameLength = dirlen;
        diritem->isSynthetic = true;

        // add diritem to the file table
        diritem->next = mFiles[hash];
        mFiles[hash] = diritem;
      } /* end processing of dirs in item's name */
    }
  }
  return NS_OK;
}

nsZipHandle* nsZipArchive::GetFD()
{
  if (!mFd)
    return NULL;
  return mFd.get();
}

//---------------------------------------------
// nsZipArchive::GetData
//---------------------------------------------
PRUint8* nsZipArchive::GetData(nsZipItem* aItem)
{
  PR_ASSERT (aItem);

  //-- read local header to get variable length values and calculate
  //-- the real data offset
  PRUint32 len = mFd->mLen;
  PRUint8* data = mFd->mFileData;
  PRUint32 offset = aItem->LocalOffset();
  if (offset + ZIPLOCAL_SIZE > len)
    return nsnull;

  // -- check signature before using the structure, in case the zip file is corrupt
  ZipLocal* Local = (ZipLocal*)(data + offset);
  if ((xtolong(Local->signature) != LOCALSIG))
    return nsnull;

  //-- NOTE: extralen is different in central header and local header
  //--       for archives created using the Unix "zip" utility. To set
  //--       the offset accurately we need the _local_ extralen.
  offset += ZIPLOCAL_SIZE +
            xtoint(Local->filename_len) +
            xtoint(Local->extrafield_len);

  // -- check if there is enough source data in the file
  if (offset + aItem->Size() > len)
    return nsnull;

  return data + offset;
}

//---------------------------------------------
// nsZipArchive::CopyItemToDisk
//---------------------------------------------
nsresult
nsZipArchive::CopyItemToDisk(nsZipItem *item, PRFileDesc* outFD)
{
  PR_ASSERT(item);

  //-- get to the start of file's data
  const PRUint8* itemData = GetData(item);
  if (!itemData)
    return NS_ERROR_FILE_CORRUPTED;

  if (outFD && PR_Write(outFD, itemData, item->Size()) < (READTYPE)item->Size())
  {
    //-- Couldn't write all the data (disk full?)
    return NS_ERROR_FILE_DISK_FULL;
  }

  //-- Calculate crc
  PRUint32 crc = crc32(0L, (const unsigned char*)itemData, item->Size());
  //-- verify crc32
  if (crc != item->CRC32())
      return NS_ERROR_FILE_CORRUPTED;

  return NS_OK;
}


//---------------------------------------------
// nsZipArchive::InflateItem
//---------------------------------------------
nsresult nsZipArchive::InflateItem(nsZipItem * item, PRFileDesc* outFD)
/*
 * This function inflates an archive item to disk, to the
 * file specified by outFD. If outFD is zero, the extracted data is
 * not written, only checked for CRC, so this is in effect same as 'Test'.
 */
{
  PR_ASSERT(item);
  //-- allocate deflation buffers
  Bytef outbuf[ZIP_BUFLEN];

  //-- set up the inflate
  z_stream    zs;
  nsresult status = gZlibInit(&zs);
  if (status != NS_OK)
    return NS_ERROR_FAILURE;

  //-- inflate loop
  zs.avail_in = item->Size();
  zs.next_in = (Bytef*)GetData(item);
  if (!zs.next_in)
    return NS_ERROR_FILE_CORRUPTED;

  PRUint32  crc = crc32(0L, Z_NULL, 0);
  int zerr = Z_OK;
  while (zerr == Z_OK)
  {
    zs.next_out = outbuf;
    zs.avail_out = ZIP_BUFLEN;

    zerr = inflate(&zs, Z_PARTIAL_FLUSH);
    if (zerr != Z_OK && zerr != Z_STREAM_END)
    {
      status = (zerr == Z_MEM_ERROR) ? NS_ERROR_OUT_OF_MEMORY : NS_ERROR_FILE_CORRUPTED;
      break;
    }
    PRUint32 count = zs.next_out - outbuf;

    //-- incrementally update crc32
    crc = crc32(crc, (const unsigned char*)outbuf, count);

    if (outFD && PR_Write(outFD, outbuf, count) < (READTYPE)count)
    {
      status = NS_ERROR_FILE_DISK_FULL;
      break;
    }
  } // while

  //-- free zlib internal state
  inflateEnd(&zs);

  //-- verify crc32
  if ((status == NS_OK) && (crc != item->CRC32()))
  {
    status = NS_ERROR_FILE_CORRUPTED;
  }
  return status;
}

//------------------------------------------
// nsZipArchive constructor and destructor
//------------------------------------------

nsZipArchive::nsZipArchive() :
  mBuiltSynthetics(false)
{
  MOZ_COUNT_CTOR(nsZipArchive);

  // initialize the table to NULL
  memset(mFiles, 0, sizeof(mFiles));
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
  PL_strfree(mPattern);

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
static PRUint32 HashName(const char* aName, PRUint16 len)
{
  PR_ASSERT(aName != 0);

  const PRUint8* p = (const PRUint8*)aName;
  const PRUint8* endp = p + len;
  PRUint32 val = 0;
  while (p != endp) {
    val = val*37 + *p++;
  }

  return (val % ZIP_TABSIZE);
}

/*
 *  x t o i n t
 *
 *  Converts a two byte ugly endianed integer
 *  to our platform's integer.
 */
static PRUint16 xtoint (const PRUint8 *ii)
{
  return (PRUint16) ((ii [0]) | (ii [1] << 8));
}

/*
 *  x t o l o n g
 *
 *  Converts a four byte ugly endianed integer
 *  to our platform's integer.
 */
static PRUint32 xtolong (const PRUint8 *ll)
{
  return (PRUint32)( (ll [0] <<  0) |
                     (ll [1] <<  8) |
                     (ll [2] << 16) |
                     (ll [3] << 24) );
}

/*
 * GetModTime
 *
 * returns last modification time in microseconds
 */
static PRTime GetModTime(PRUint16 aDate, PRUint16 aTime)
{
  // Note that on DST shift we can't handle correctly the hour that is valid
  // in both DST zones
  PRExplodedTime time;

  time.tm_usec = 0;

  time.tm_hour = (aTime >> 11) & 0x1F;
  time.tm_min = (aTime >> 5) & 0x3F;
  time.tm_sec = (aTime & 0x1F) * 2;

  time.tm_year = (aDate >> 9) + 1980;
  time.tm_month = ((aDate >> 5) & 0x0F) - 1;
  time.tm_mday = aDate & 0x1F;

  time.tm_params.tp_gmt_offset = 0;
  time.tm_params.tp_dst_offset = 0;

  PR_NormalizeTime(&time, PR_GMTParameters);
  time.tm_params.tp_gmt_offset = PR_LocalTimeParameters(&time).tp_gmt_offset;
  PR_NormalizeTime(&time, PR_GMTParameters);
  time.tm_params.tp_dst_offset = PR_LocalTimeParameters(&time).tp_dst_offset;

  return PR_ImplodeTime(&time);
}

PRUint32 nsZipItem::LocalOffset()
{
  return xtolong(central->localhdr_offset);
}

PRUint32 nsZipItem::Size()
{
  return isSynthetic ? 0 : xtolong(central->size);
}

PRUint32 nsZipItem::RealSize()
{
  return isSynthetic ? 0 : xtolong(central->orglen);
}

PRUint32 nsZipItem::CRC32()
{
  return isSynthetic ? 0 : xtolong(central->crc32);
}

PRUint16 nsZipItem::Date()
{
  return isSynthetic ? kSyntheticDate : xtoint(central->date);
}

PRUint16 nsZipItem::Time()
{
  return isSynthetic ? kSyntheticTime : xtoint(central->time);
}

PRUint16 nsZipItem::Compression()
{
  return isSynthetic ? STORED : xtoint(central->method);
}

bool nsZipItem::IsDirectory()
{
  return isSynthetic || ((nameLength > 0) && ('/' == Name()[nameLength - 1]));
}

PRUint16 nsZipItem::Mode()
{
  if (isSynthetic) return 0755;
  return ((PRUint16)(central->external_attributes[2]) | 0x100);
}

const PRUint8 * nsZipItem::GetExtraField(PRUint16 aTag, PRUint16 *aBlockSize)
{
  if (isSynthetic) return NULL;

  const unsigned char *buf = ((const unsigned char*)central) + ZIPCENTRAL_SIZE +
                             nameLength;
  PRUint32 buflen = (PRUint32)xtoint(central->extrafield_len);
  PRUint32 pos = 0;
  PRUint16 tag, blocksize;

  while (buf && (pos + 4) <= buflen) {
    tag = xtoint(buf + pos);
    blocksize = xtoint(buf + pos + 2);

    if (aTag == tag && (pos + 4 + blocksize) <= buflen) {
      *aBlockSize = blocksize;
      return buf + pos;
    }

    pos += blocksize + 4;
  }

  return NULL;
}


PRTime nsZipItem::LastModTime()
{
  if (isSynthetic) return GetModTime(kSyntheticDate, kSyntheticTime);

  // Try to read timestamp from extra field
  PRUint16 blocksize;
  const PRUint8 *tsField = GetExtraField(EXTENDED_TIMESTAMP_FIELD, &blocksize);
  if (tsField && blocksize >= 5 && tsField[4] & EXTENDED_TIMESTAMP_MODTIME) {
    return (PRTime)(xtolong(tsField + 5)) * PR_USEC_PER_SEC;
  }

  return GetModTime(Date(), Time());
}

#if defined(XP_UNIX) || defined(XP_BEOS)
bool nsZipItem::IsSymlink()
{
  if (isSynthetic) return false;
  return (xtoint(central->external_attributes+2) & S_IFMT) == S_IFLNK;
}
#endif

