/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module implements a simple archive extractor for the PKZIP format.
 *
 * The underlying nsZipArchive is NOT thread-safe. Do not pass references
 * or pointers to it across thread boundaries.
 */

#define READTYPE  PRInt32
#include "zlib.h"
#include "nsISupportsUtils.h"
#include "prio.h"
#include "plstr.h"
#include "prlog.h"
#include "stdlib.h"
#include "nsWildCard.h"
#include "nsZipArchive.h"
#include "nsString.h"
#include "mozilla/FunctionTimer.h"
#include "prenv.h"
#if defined(XP_WIN)
#include <windows.h>
#endif

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


using namespace mozilla;

static const PRUint32 kMaxNameLength = PATH_MAX; /* Maximum name length */
// For synthetic zip entries. Date/time corresponds to 1980-01-01 00:00.
static const PRUint16 kSyntheticTime = 0;
static const PRUint16 kSyntheticDate = (1 + (1 << 5) + (0 << 9));

static PRUint16 xtoint(const PRUint8 *ii);
static PRUint32 xtolong(const PRUint8 *ll);
static PRUint32 HashName(const char* aName, PRUint16 nameLen);
#ifdef XP_UNIX
static nsresult ResolveSymlink(const char *path);
#endif

//***********************************************************
// For every inflation the following allocations are done:
// malloc(1 * 9520)
// malloc(32768 * 1)
//***********************************************************

nsresult gZlibInit(z_stream *zs)
{
  memset(zs, 0, sizeof(z_stream));
  int zerr = inflateInit2(zs, -MAX_WBITS);
  if (zerr != Z_OK) return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

nsZipHandle::nsZipHandle()
  : mFileData(nullptr)
  , mLen(0)
  , mMap(nullptr)
  , mRefCnt(0)
{
  MOZ_COUNT_CTOR(nsZipHandle);
}

NS_IMPL_THREADSAFE_ADDREF(nsZipHandle)
NS_IMPL_THREADSAFE_RELEASE(nsZipHandle)

nsresult nsZipHandle::Init(nsIFile *file, nsZipHandle **ret)
{
  mozilla::AutoFDClose fd;
  nsresult rv = file->OpenNSPRFileDesc(PR_RDONLY, 0000, &fd.rwget());
  if (NS_FAILED(rv))
    return rv;

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

  nsRefPtr<nsZipHandle> handle = new nsZipHandle();
  if (!handle) {
    PR_MemUnmap(buf, (PRUint32) size);
    PR_CloseFileMap(map);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  handle->mMap = map;
  handle->mFile.Init(file);
  handle->mLen = (PRUint32) size;
  handle->mFileData = buf;
  *ret = handle.forget().get();
  return NS_OK;
}

nsresult nsZipHandle::Init(nsZipArchive *zip, const char *entry,
                           nsZipHandle **ret)
{
  nsRefPtr<nsZipHandle> handle = new nsZipHandle();
  if (!handle)
    return NS_ERROR_OUT_OF_MEMORY;

  handle->mBuf = new nsZipItemPtr<PRUint8>(zip, entry);
  if (!handle->mBuf)
    return NS_ERROR_OUT_OF_MEMORY;

  if (!handle->mBuf->Buffer())
    return NS_ERROR_UNEXPECTED;

  handle->mMap = nullptr;
  handle->mFile.Init(zip, entry);
  handle->mLen = handle->mBuf->Length();
  handle->mFileData = handle->mBuf->Buffer();
  *ret = handle.forget().get();
  return NS_OK;
}

PRInt64 nsZipHandle::SizeOfMapping()
{
    return mLen;
}

nsZipHandle::~nsZipHandle()
{
  if (mMap) {
    PR_MemUnmap((void *)mFileData, mLen);
    PR_CloseFileMap(mMap);
  }
  mFileData = nullptr;
  mMap = nullptr;
  mBuf = nullptr;
  MOZ_COUNT_DTOR(nsZipHandle);
}

//***********************************************************
//      nsZipArchive  --  public methods
//***********************************************************

//---------------------------------------------
//  nsZipArchive::OpenArchive
//---------------------------------------------
nsresult nsZipArchive::OpenArchive(nsZipHandle *aZipHandle)
{
  mFd = aZipHandle;

  // Initialize our arena
  PL_INIT_ARENA_POOL(&mArena, "ZipArena", ZIP_ARENABLOCKSIZE);

  //-- get table of contents for archive
  nsresult rv = BuildFileList();
  char *env = PR_GetEnv("MOZ_JAR_LOG_DIR");
  if (env && NS_SUCCEEDED(rv) && aZipHandle->mFile) {
    nsCOMPtr<nsIFile> logFile;
    nsresult rv2 = NS_NewLocalFile(NS_ConvertUTF8toUTF16(env), false, getter_AddRefs(logFile));
    
    if (!NS_SUCCEEDED(rv2))
      return rv;

    // Create a directory for the log (in case it doesn't exist)
    logFile->Create(nsIFile::DIRECTORY_TYPE, 0700);

    nsAutoString name;
    nsCOMPtr<nsIFile> file = aZipHandle->mFile.GetBaseFile();
    file->GetLeafName(name);
    name.Append(NS_LITERAL_STRING(".log"));
    logFile->Append(name);

    PRFileDesc* fd;
    rv2 = logFile->OpenNSPRFileDesc(PR_WRONLY|PR_CREATE_FILE|PR_APPEND, 0644, &fd);
    if (NS_SUCCEEDED(rv2))
      mLog = fd;
  }
  return rv;
}

nsresult nsZipArchive::OpenArchive(nsIFile *aFile)
{
  nsRefPtr<nsZipHandle> handle;
  nsresult rv = nsZipHandle::Init(aFile, getter_AddRefs(handle));
  if (NS_FAILED(rv))
    return rv;

  return OpenArchive(handle);
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
MOZ_WIN_MEM_TRY_BEGIN
    nsZipItem* item = mFiles[ HashName(aEntryName, len) ];
    while (item) {
      if ((len == item->nameLength) && 
          (!memcmp(aEntryName, item->Name(), len))) {
        
        if (mLog) {
          // Successful GetItem() is a good indicator that the file is about to be read
          char *tmp = PL_strdup(aEntryName);
          tmp[len]='\n';
          PR_Write(mLog, tmp, len+1);
          PL_strfree(tmp);
        }
        return item; //-- found it
      }
      item = item->next;
    }
MOZ_WIN_MEM_TRY_CATCH(return nullptr)
  }
  return nullptr;
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

  Bytef outbuf[ZIP_BUFLEN];

  nsZipCursor cursor(item, this, outbuf, ZIP_BUFLEN, true);

  nsresult rv = NS_OK;

  while (true) {
    PRUint32 count = 0;
    PRUint8* buf = cursor.Read(&count);
    if (!buf) {
      rv = NS_ERROR_FILE_CORRUPTED;
      break;
    } else if (count == 0) {
      break;
    }

    if (aFd && PR_Write(aFd, buf, count) < (READTYPE)count) {
      rv = NS_ERROR_FILE_DISK_FULL;
      break;
    }
  }

  //-- delete the file on errors, or resolve symlink if needed
  if (aFd) {
    PR_Close(aFd);
    if (rv != NS_OK)
      PR_Delete(outname);
#ifdef XP_UNIX
    else if (item->IsSymlink())
      rv = ResolveSymlink(outname);
#endif
  }

  return rv;
}

//---------------------------------------------
// nsZipArchive::FindInit
//---------------------------------------------
nsresult
nsZipArchive::FindInit(const char * aPattern, nsZipFind **aFind)
{
  if (!aFind)
    return NS_ERROR_ILLEGAL_VALUE;

  // null out param in case an error happens
  *aFind = NULL;

  bool    regExp = false;
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
        regExp = false;
        break;

      case VALID_SXP:
        regExp = true;
        break;

      default:
        // undocumented return value from RegExpValid!
        PR_ASSERT(false);
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
MOZ_WIN_MEM_TRY_BEGIN
  // we start from last match, look for next
  while (mSlot < ZIP_TABSIZE)
  {
    // move to next in current chain, or move to new slot
    mItem = mItem ? mItem->next : mArchive->mFiles[mSlot];

    bool found = false;
    if (!mItem)
      ++mSlot;                          // no more in this chain, move to next slot
    else if (!mPattern)
      found = true;            // always match
    else if (mRegExp)
    {
      char buf[kMaxNameLength+1];
      memcpy(buf, mItem->Name(), mItem->nameLength);
      buf[mItem->nameLength]='\0';
      found = (NS_WildCardMatch(buf, mPattern, false) == MATCH);
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
MOZ_WIN_MEM_TRY_CATCH(return NS_ERROR_FAILURE)
  return NS_ERROR_FILE_TARGET_DOES_NOT_EXIST;
}

#ifdef XP_UNIX
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
#ifndef XP_WIN
  NS_TIME_FUNCTION;
#endif
  // Get archive size using end pos
  const PRUint8* buf;
  const PRUint8* startp = mFd->mFileData;
  const PRUint8* endp = startp + mFd->mLen;
MOZ_WIN_MEM_TRY_BEGIN
  PRUint32 centralOffset = 4;
  if (mFd->mLen > ZIPCENTRAL_SIZE && xtolong(startp + centralOffset) == CENTRALSIG) {
    // Success means optimized jar layout from bug 559961 is in effect
  } else {
    for (buf = endp - ZIPEND_SIZE; buf > startp; buf--)
      {
        if (xtolong(buf) == ENDSIG) {
          centralOffset = xtolong(((ZipEnd *)buf)->offset_central_dir);
          break;
        }
      }
  }

  if (!centralOffset)
    return NS_ERROR_FILE_CORRUPTED;

  //-- Read the central directory headers
  buf = startp + centralOffset;
  PRUint32 sig = 0;
  while (buf + PRInt32(sizeof(PRUint32)) <= endp &&
         (sig = xtolong(buf)) == CENTRALSIG) {
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

    sig = 0;
  } /* while reading central directory records */

  if (sig != ENDSIG)
    return NS_ERROR_FILE_CORRUPTED;

  // Make the comment available for consumers.
  if (endp - buf >= ZIPEND_SIZE) {
    ZipEnd *zipend = (ZipEnd *)buf;

    buf += ZIPEND_SIZE;
    PRUint16 commentlen = xtoint(zipend->commentfield_len);
    if (endp - buf >= commentlen) {
      mCommentPtr = (const char *)buf;
      mCommentLen = commentlen;
    }
  }

MOZ_WIN_MEM_TRY_CATCH(return NS_ERROR_FAILURE)
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

MOZ_WIN_MEM_TRY_BEGIN
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
        bool found = false;
        for (nsZipItem* zi = mFiles[hash]; zi != NULL; zi = zi->next)
        {
          if ((dirlen == zi->nameLength) &&
              (0 == memcmp(item->Name(), zi->Name(), dirlen)))
          {
            // we've already added this dir and all its parents
            found = true;
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
MOZ_WIN_MEM_TRY_CATCH(return NS_ERROR_FAILURE)
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
const PRUint8* nsZipArchive::GetData(nsZipItem* aItem)
{
  PR_ASSERT (aItem);
MOZ_WIN_MEM_TRY_BEGIN
  //-- read local header to get variable length values and calculate
  //-- the real data offset
  PRUint32 len = mFd->mLen;
  const PRUint8* data = mFd->mFileData;
  PRUint32 offset = aItem->LocalOffset();
  if (offset + ZIPLOCAL_SIZE > len)
    return nullptr;

  // -- check signature before using the structure, in case the zip file is corrupt
  ZipLocal* Local = (ZipLocal*)(data + offset);
  if ((xtolong(Local->signature) != LOCALSIG))
    return nullptr;

  //-- NOTE: extralen is different in central header and local header
  //--       for archives created using the Unix "zip" utility. To set
  //--       the offset accurately we need the _local_ extralen.
  offset += ZIPLOCAL_SIZE +
            xtoint(Local->filename_len) +
            xtoint(Local->extrafield_len);

  // -- check if there is enough source data in the file
  if (offset + aItem->Size() > len)
    return nullptr;

  return data + offset;
MOZ_WIN_MEM_TRY_CATCH(return nullptr)
}

// nsZipArchive::GetComment
bool nsZipArchive::GetComment(nsACString &aComment)
{
MOZ_WIN_MEM_TRY_BEGIN
  aComment.Assign(mCommentPtr, mCommentLen);
MOZ_WIN_MEM_TRY_CATCH(return false)
  return true;
}

//---------------------------------------------
// nsZipArchive::SizeOfMapping
//---------------------------------------------
PRInt64 nsZipArchive::SizeOfMapping()
{
    return mFd ? mFd->SizeOfMapping() : 0;
}

//------------------------------------------
// nsZipArchive constructor and destructor
//------------------------------------------

nsZipArchive::nsZipArchive()
  : mRefCnt(0)
  , mBuiltSynthetics(false)
{
  MOZ_COUNT_CTOR(nsZipArchive);

  // initialize the table to NULL
  memset(mFiles, 0, sizeof(mFiles));
}

NS_IMPL_THREADSAFE_ADDREF(nsZipArchive)
NS_IMPL_THREADSAFE_RELEASE(nsZipArchive)

nsZipArchive::~nsZipArchive()
{
  CloseArchive();

  MOZ_COUNT_DTOR(nsZipArchive);
}


//------------------------------------------
// nsZipFind constructor and destructor
//------------------------------------------

nsZipFind::nsZipFind(nsZipArchive* aZip, char* aPattern, bool aRegExp) : 
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
  if (isSynthetic) return nullptr;
MOZ_WIN_MEM_TRY_BEGIN
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

MOZ_WIN_MEM_TRY_CATCH(return nullptr)
  return nullptr;
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

#ifdef XP_UNIX
bool nsZipItem::IsSymlink()
{
  if (isSynthetic) return false;
  return (xtoint(central->external_attributes+2) & S_IFMT) == S_IFLNK;
}
#endif

nsZipCursor::nsZipCursor(nsZipItem *item, nsZipArchive *aZip, PRUint8* aBuf, PRUint32 aBufSize, bool doCRC) :
  mItem(item),
  mBuf(aBuf),
  mBufSize(aBufSize),
  mDoCRC(doCRC)
{
  if (mItem->Compression() == DEFLATED) {
#ifdef DEBUG
    nsresult status =
#endif
      gZlibInit(&mZs);
    NS_ASSERTION(status == NS_OK, "Zlib failed to initialize");
    NS_ASSERTION(aBuf, "Must pass in a buffer for DEFLATED nsZipItem");
  }
  
  mZs.avail_in = item->Size();
  mZs.next_in = (Bytef*)aZip->GetData(item);
  
  if (doCRC)
    mCRC = crc32(0L, Z_NULL, 0);
}

nsZipCursor::~nsZipCursor()
{
  if (mItem->Compression() == DEFLATED) {
    inflateEnd(&mZs);
  }
}

PRUint8* nsZipCursor::ReadOrCopy(PRUint32 *aBytesRead, bool aCopy) {
  int zerr;
  PRUint8 *buf = nullptr;
  bool verifyCRC = true;

  if (!mZs.next_in)
    return nullptr;
MOZ_WIN_MEM_TRY_BEGIN
  switch (mItem->Compression()) {
  case STORED:
    if (!aCopy) {
      *aBytesRead = mZs.avail_in;
      buf = mZs.next_in;
      mZs.next_in += mZs.avail_in;
      mZs.avail_in = 0;
    } else {
      *aBytesRead = mZs.avail_in > mBufSize ? mBufSize : mZs.avail_in;
      memcpy(mBuf, mZs.next_in, *aBytesRead);
      mZs.avail_in -= *aBytesRead;
      mZs.next_in += *aBytesRead;
    }
    break;
  case DEFLATED:
    buf = mBuf;
    mZs.next_out = buf;
    mZs.avail_out = mBufSize;
    
    zerr = inflate(&mZs, Z_PARTIAL_FLUSH);
    if (zerr != Z_OK && zerr != Z_STREAM_END)
      return nullptr;
    
    *aBytesRead = mZs.next_out - buf;
    verifyCRC = (zerr == Z_STREAM_END);
    break;
  default:
    return nullptr;
  }

  if (mDoCRC) {
    mCRC = crc32(mCRC, (const unsigned char*)buf, *aBytesRead);
    if (verifyCRC && mCRC != mItem->CRC32())
      return nullptr;
  }
MOZ_WIN_MEM_TRY_CATCH(return nullptr)
  return buf;
}

nsZipItemPtr_base::nsZipItemPtr_base(nsZipArchive *aZip, const char * aEntryName, bool doCRC) :
  mReturnBuf(nullptr)
{
  // make sure the ziparchive hangs around
  mZipHandle = aZip->GetFD();

  nsZipItem* item = aZip->GetItem(aEntryName);
  if (!item)
    return;

  PRUint32 size = 0;
  if (item->Compression() == DEFLATED) {
    size = item->RealSize();
    mAutoBuf = new PRUint8[size];
  }

  nsZipCursor cursor(item, aZip, mAutoBuf, size, doCRC);
  mReturnBuf = cursor.Read(&mReadlen);
  if (!mReturnBuf) {
    return;
  }

  if (mReadlen != item->RealSize()) {
    NS_ASSERTION(mReadlen == item->RealSize(), "nsZipCursor underflow");
    mReturnBuf = nullptr;
    return;
  }
}
