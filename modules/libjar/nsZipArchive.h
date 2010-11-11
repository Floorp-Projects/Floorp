/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Veditz <dveditz@netscape.com>
 *   Samir Gehani <sgehani@netscape.com>
 *   Mitch Stoltz <mstoltz@netscape.com>
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

#ifndef nsZipArchive_h_
#define nsZipArchive_h_

#define ZIP_TABSIZE   256
#define ZIP_BUFLEN    (4*1024)      /* Used as output buffer when deflating items to a file */

#ifndef PL_ARENA_CONST_ALIGN_MASK
#define PL_ARENA_CONST_ALIGN_MASK  (sizeof(void*)-1)
#endif
#include "plarena.h"

#include "zlib.h"
#include "zipstruct.h"
#include "nsAutoPtr.h"
#include "nsILocalFile.h"
#include "mozilla/FileUtils.h"

#if defined(XP_WIN)
#define MOZ_WIN_MEM_TRY_BEGIN __try {
#define MOZ_WIN_MEM_TRY_CATCH(cmd) }                                \
  __except(GetExceptionCode()==EXCEPTION_IN_PAGE_ERROR ?            \
           EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)   \
  {                                                                 \
    NS_WARNING("EXCEPTION_IN_PAGE_ERROR in " __FUNCTION__);         \
    cmd;                                                            \
  }
#else
#define MOZ_WIN_MEM_TRY_BEGIN {
#define MOZ_WIN_MEM_TRY_CATCH(cmd) }
#endif

class nsZipFind;
struct PRFileDesc;

/**
 * This file defines some of the basic structures used by libjar to
 * read Zip files. It makes use of zlib in order to do the decompression.
 *
 * A few notes on the classes/structs:
 * nsZipArchive   represents a single Zip file, and maintains an index
 *                of all the items in the file.
 * nsZipItem      represents a single item (file) in the Zip archive.
 * nsZipFind      represents the metadata involved in doing a search,
 *                and current state of the iteration of found objects.
 * 'MT''safe' reading from the zipfile is performed through JARInputStream,
 * which maintains its own file descriptor, allowing for multiple reads 
 * concurrently from the same zip file.
 */

/**
 * nsZipItem -- a helper struct for nsZipArchive
 *
 * each nsZipItem represents one file in the archive and all the
 * information needed to manipulate it.
 */
class nsZipItem
{
public:
  const char* Name() { return ((const char*)central) + ZIPCENTRAL_SIZE; }

  PRUint32 LocalOffset();
  PRUint32 Size();
  PRUint32 RealSize();
  PRUint32 CRC32();
  PRUint16 Date();
  PRUint16 Time();
  PRUint16 Compression();
  bool     IsDirectory();
  PRUint16 Mode();
  const PRUint8* GetExtraField(PRUint16 aTag, PRUint16 *aBlockSize);
  PRTime   LastModTime();

#if defined(XP_UNIX) || defined(XP_BEOS)
  bool     IsSymlink();
#endif

  nsZipItem*         next;
  const ZipCentral*  central;
  PRUint16           nameLength;
  bool               isSynthetic;
};

class nsZipHandle;

/** 
 * nsZipArchive -- a class for reading the PKZIP file format.
 *
 */
class nsZipArchive 
{
  friend class nsZipFind;

public:
  /** constructing does not open the archive. See OpenArchive() */
  nsZipArchive();

  /** destructing the object closes the archive */
  ~nsZipArchive();

  /** 
   * OpenArchive 
   * 
   * It's an error to call this more than once on the same nsZipArchive
   * object. If we were allowed to use exceptions this would have been 
   * part of the constructor 
   *
   * @param   aZipHandle  The nsZipHandle used to access the zip
   * @return  status code
   */
  nsresult OpenArchive(nsZipHandle *aZipHandle);

  /** 
   * OpenArchive 
   * 
   * Convenience function that generates nsZipHandle
   *
   * @param   aFile  The file used to access the zip
   * @return  status code
   */
  nsresult OpenArchive(nsIFile *aFile);

  /**
   * Test the integrity of items in this archive by running
   * a CRC check after extracting each item into a memory 
   * buffer.  If an entry name is supplied only the 
   * specified item is tested.  Else, if null is supplied
   * then all the items in the archive are tested.
   *
   * @return  status code       
   */
  nsresult Test(const char *aEntryName);

  /**
   * Closes an open archive.
   */
  nsresult CloseArchive();

  /** 
   * GetItem
   * @param   aEntryName Name of file in the archive
   * @return  pointer to nsZipItem
   */  
  nsZipItem* GetItem(const char * aEntryName);
  
  /** 
   * ExtractFile
   *
   * @param   zipEntry   Name of file in archive to extract
   * @param   outFD      Filedescriptor to write contents to
   * @param   outname    Name of file to write to
   * @return  status code
   */
  nsresult ExtractFile(nsZipItem * zipEntry, const char *outname, PRFileDesc * outFD);

  /**
   * FindInit
   *
   * Initializes a search for files in the archive. FindNext() returns
   * the actual matches. The nsZipFind must be deleted when you're done
   *
   * @param   aPattern    a string or RegExp pattern to search for
   *                      (may be NULL to find all files in archive)
   * @param   aFind       a pointer to a pointer to a structure used
   *                      in FindNext.  In the case of an error this
   *                      will be set to NULL.
   * @return  status code
   */
  PRInt32 FindInit(const char * aPattern, nsZipFind** aFind);

  /*
   * Gets an undependent handle to the mapped file.
   */
  nsZipHandle* GetFD();

  /**
   * Get pointer to the data of the item.
   * @param   aItem       Pointer to nsZipItem
   * reutrns null when zip file is corrupt.
   */
  const PRUint8* GetData(nsZipItem* aItem);

private:
  //--- private members ---

  nsZipItem*    mFiles[ZIP_TABSIZE];
  PLArenaPool   mArena;

  // Whether we synthesized the directory entries
  bool          mBuiltSynthetics;

  // file handle
  nsRefPtr<nsZipHandle> mFd;

  // logging handle
  mozilla::AutoFDClose mLog;

  //--- private methods ---
  
  nsZipArchive& operator=(const nsZipArchive& rhs); // prevent assignments
  nsZipArchive(const nsZipArchive& rhs);            // prevent copies

  nsZipItem*        CreateZipItem();
  nsresult          BuildFileList();
  nsresult          BuildSynthetics();
};

/** 
 * nsZipFind 
 *
 * a helper class for nsZipArchive, representing a search
 */
class nsZipFind
{
public:
  nsZipFind(nsZipArchive* aZip, char* aPattern, PRBool regExp);
  ~nsZipFind();

  nsresult      FindNext(const char** aResult, PRUint16* aNameLen);

private:
  nsZipArchive* mArchive;
  char*         mPattern;
  nsZipItem*    mItem;
  PRUint16      mSlot;
  PRPackedBool  mRegExp;

  //-- prevent copies and assignments
  nsZipFind& operator=(const nsZipFind& rhs);
  nsZipFind(const nsZipFind& rhs);
};

/** 
 * nsZipCursor -- a low-level class for reading the individual items in a zip.
 */
class nsZipCursor {
public:
  /**
   * Initializes the cursor
   *
   * @param   aItem       Item of interest
   * @param   aZip        Archive
   * @param   aBuf        Buffer used for decompression.
   *                      This determines the maximum Read() size in the compressed case.
   * @param   aBufSize    Buffer size
   * @param   doCRC       When set to true Read() will check crc
   */
  nsZipCursor(nsZipItem *aItem, nsZipArchive *aZip, PRUint8* aBuf = NULL, PRUint32 aBufSize = 0, bool doCRC = false);

  ~nsZipCursor();

  /**
   * Performs reads. In the compressed case it uses aBuf(passed in constructor), for stored files
   * it returns a zero-copy buffer.
   *
   * @param   aBytesRead  Outparam for number of bytes read.
   * @return  data read or NULL if item is corrupted.
   */
  PRUint8* Read(PRUint32 *aBytesRead);

private:
  nsZipItem *mItem; 
  PRUint8  *mBuf; 
  PRUint32  mBufSize; 
  z_stream  mZs;
  PRUint32 mCRC;
  bool mDoCRC;
};

/**
 * nsZipItemPtr - a RAII convenience class for reading the individual items in a zip.
 * It reads whole files and does zero-copy IO for stored files. A buffer is allocated
 * for decompression.
 * Do not use when the file may be very large.
 */
class nsZipItemPtr_base {
public:
  /**
   * Initializes the reader
   *
   * @param   aZip        Archive
   * @param   aEntryName  Archive membername
   * @param   doCRC       When set to true Read() will check crc
   */
  nsZipItemPtr_base(nsZipArchive *aZip, const char *aEntryName, bool doCRC);

  PRUint32 Length() const {
    return mReadlen;
  }

protected:
  nsRefPtr<nsZipHandle> mZipHandle;
  nsAutoArrayPtr<PRUint8> mAutoBuf;
  PRUint8 *mReturnBuf;
  PRUint32 mReadlen;
};

template <class T>
class nsZipItemPtr : public nsZipItemPtr_base {
public:
  nsZipItemPtr(nsZipArchive *aZip, const char *aEntryName, bool doCRC = false) : nsZipItemPtr_base(aZip, aEntryName, doCRC) { }
  /**
   * @return buffer containing the whole zip member or NULL on error.
   * The returned buffer is owned by nsZipItemReader.
   */
  const T* Buffer() const {
    return (const T*)mReturnBuf;
  }

  operator const T*() const {
    return Buffer();
  }

  /**
   * Relinquish ownership of zip member if compressed.
   * Copy member into a new buffer if uncompressed.
   * @return a buffer with whole zip member. It is caller's responsibility to free() it.
   */
  T* Forget() {
    if (!mReturnBuf)
      return NULL;
    // In uncompressed mmap case, give up buffer
    if (mAutoBuf.get() == mReturnBuf) {
      mReturnBuf = NULL;
      return (T*) mAutoBuf.forget();
    }
    T *ret = (T*) malloc(Length());
    memcpy(ret, mReturnBuf, Length());
    mReturnBuf = NULL;
    return ret;
  }
};

class nsZipHandle {
friend class nsZipArchive;
public:
  static nsresult Init(nsILocalFile *file, nsZipHandle **ret NS_OUTPARAM);
  static nsresult Init(nsZipArchive *zip, const char *entry,
                       nsZipHandle **ret NS_OUTPARAM);

  NS_METHOD_(nsrefcnt) AddRef(void);
  NS_METHOD_(nsrefcnt) Release(void);

protected:
  const PRUint8 * mFileData; /* pointer to mmaped file */
  PRUint32        mLen;      /* length of file and memory mapped area */
  nsCOMPtr<nsILocalFile> mFile; /* source file if any, for logging */

private:
  nsZipHandle();
  ~nsZipHandle();

  PRFileMap *                       mMap;    /* nspr datastructure for mmap */
  nsAutoPtr<nsZipItemPtr<PRUint8> > mBuf;
  nsrefcnt                          mRefCnt; /* ref count */
};

nsresult gZlibInit(z_stream *zs);

#endif /* nsZipArchive_h_ */
