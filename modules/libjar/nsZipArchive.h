/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsZipArchive_h_
#define nsZipArchive_h_

#include "mozilla/Attributes.h"

#define ZIP_TABSIZE   256
#define ZIP_BUFLEN    (4*1024)      /* Used as output buffer when deflating items to a file */

#include "plarena.h"
#include "zlib.h"
#include "zipstruct.h"
#include "nsAutoPtr.h"
#include "nsIFile.h"
#include "nsISupportsImpl.h" // For mozilla::ThreadSafeAutoRefCnt
#include "mozilla/FileUtils.h"
#include "mozilla/FileLocation.h"

#ifdef HAVE_SEH_EXCEPTIONS
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

  uint32_t LocalOffset();
  uint32_t Size();
  uint32_t RealSize();
  uint32_t CRC32();
  uint16_t Date();
  uint16_t Time();
  uint16_t Compression();
  bool     IsDirectory();
  uint16_t Mode();
  const uint8_t* GetExtraField(uint16_t aTag, uint16_t *aBlockSize);
  PRTime   LastModTime();

#ifdef XP_UNIX
  bool     IsSymlink();
#endif

  nsZipItem*         next;
  const ZipCentral*  central;
  uint16_t           nameLength;
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

  /** destructing the object closes the archive */
  ~nsZipArchive();

public:
  /** constructing does not open the archive. See OpenArchive() */
  nsZipArchive();

  /** 
   * OpenArchive 
   * 
   * It's an error to call this more than once on the same nsZipArchive
   * object. If we were allowed to use exceptions this would have been 
   * part of the constructor 
   *
   * @param   aZipHandle  The nsZipHandle used to access the zip
   * @param   aFd         Optional PRFileDesc for Windows readahead optimization
   * @return  status code
   */
  nsresult OpenArchive(nsZipHandle *aZipHandle, PRFileDesc *aFd = nullptr);

  /** 
   * OpenArchive 
   * 
   * Convenience function that generates nsZipHandle
   *
   * @param   aFile         The file used to access the zip
   * @param   aMustCacheFd  Optional flag to keep the PRFileDesc in nsZipHandle
   * @return  status code
   */
  nsresult OpenArchive(nsIFile *aFile, bool aMustCacheFd = false);

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
   *                      (may be nullptr to find all files in archive)
   * @param   aFind       a pointer to a pointer to a structure used
   *                      in FindNext.  In the case of an error this
   *                      will be set to nullptr.
   * @return  status code
   */
  nsresult FindInit(const char * aPattern, nsZipFind** aFind);

  /*
   * Gets an undependent handle to the mapped file.
   */
  nsZipHandle* GetFD();

  /**
   * Gets the data offset.
   * @param   aItem       Pointer to nsZipItem
   * returns 0 on failure.
   */
  uint32_t GetDataOffset(nsZipItem* aItem);

  /**
   * Get pointer to the data of the item.
   * @param   aItem       Pointer to nsZipItem
   * reutrns null when zip file is corrupt.
   */
  const uint8_t* GetData(nsZipItem* aItem);

  bool GetComment(nsACString &aComment);

  /**
   * Gets the amount of memory taken up by the archive's mapping.
   * @return the size
   */
  int64_t SizeOfMapping();

  /*
   * Refcounting
   */
  NS_METHOD_(MozExternalRefCountType) AddRef(void);
  NS_METHOD_(MozExternalRefCountType) Release(void);

private:
  //--- private members ---
  mozilla::ThreadSafeAutoRefCnt mRefCnt; /* ref count */
  NS_DECL_OWNINGTHREAD

  nsZipItem*    mFiles[ZIP_TABSIZE];
  PLArenaPool   mArena;

  const char*   mCommentPtr;
  uint16_t      mCommentLen;

  // Whether we synthesized the directory entries
  bool          mBuiltSynthetics;

  // file handle
  nsRefPtr<nsZipHandle> mFd;

  // file URI, for logging
  nsCString mURI;

private:
  //--- private methods ---
  nsZipItem*        CreateZipItem();
  nsresult          BuildFileList(PRFileDesc *aFd = nullptr);
  nsresult          BuildSynthetics();

  nsZipArchive& operator=(const nsZipArchive& rhs) MOZ_DELETE;
  nsZipArchive(const nsZipArchive& rhs) MOZ_DELETE;
};

/** 
 * nsZipFind 
 *
 * a helper class for nsZipArchive, representing a search
 */
class nsZipFind
{
public:
  nsZipFind(nsZipArchive* aZip, char* aPattern, bool regExp);
  ~nsZipFind();

  nsresult      FindNext(const char** aResult, uint16_t* aNameLen);

private:
  nsRefPtr<nsZipArchive> mArchive;
  char*         mPattern;
  nsZipItem*    mItem;
  uint16_t      mSlot;
  bool          mRegExp;

  nsZipFind& operator=(const nsZipFind& rhs) MOZ_DELETE;
  nsZipFind(const nsZipFind& rhs) MOZ_DELETE;
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
  nsZipCursor(nsZipItem *aItem, nsZipArchive *aZip, uint8_t* aBuf = nullptr, uint32_t aBufSize = 0, bool doCRC = false);

  ~nsZipCursor();

  /**
   * Performs reads. In the compressed case it uses aBuf(passed in constructor), for stored files
   * it returns a zero-copy buffer.
   *
   * @param   aBytesRead  Outparam for number of bytes read.
   * @return  data read or nullptr if item is corrupted.
   */
  uint8_t* Read(uint32_t *aBytesRead) {
    return ReadOrCopy(aBytesRead, false);
  }

  /**
   * Performs a copy. It always uses aBuf(passed in constructor).
   *
   * @param   aBytesRead  Outparam for number of bytes read.
   * @return  data read or nullptr if item is corrupted.
   */
  uint8_t* Copy(uint32_t *aBytesRead) {
    return ReadOrCopy(aBytesRead, true);
  }

private:
  /* Actual implementation for both Read and Copy above */
  uint8_t* ReadOrCopy(uint32_t *aBytesRead, bool aCopy);

  nsZipItem *mItem; 
  uint8_t  *mBuf; 
  uint32_t  mBufSize; 
  z_stream  mZs;
  uint32_t mCRC;
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

  uint32_t Length() const {
    return mReadlen;
  }

protected:
  nsRefPtr<nsZipHandle> mZipHandle;
  nsAutoArrayPtr<uint8_t> mAutoBuf;
  uint8_t *mReturnBuf;
  uint32_t mReadlen;
};

template <class T>
class nsZipItemPtr : public nsZipItemPtr_base {
public:
  nsZipItemPtr(nsZipArchive *aZip, const char *aEntryName, bool doCRC = false) : nsZipItemPtr_base(aZip, aEntryName, doCRC) { }
  /**
   * @return buffer containing the whole zip member or nullptr on error.
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
      return nullptr;
    // In uncompressed mmap case, give up buffer
    if (mAutoBuf.get() == mReturnBuf) {
      mReturnBuf = nullptr;
      return (T*) mAutoBuf.forget();
    }
    T *ret = (T*) malloc(Length());
    memcpy(ret, mReturnBuf, Length());
    mReturnBuf = nullptr;
    return ret;
  }
};

class nsZipHandle {
friend class nsZipArchive;
friend class mozilla::FileLocation;
public:
  static nsresult Init(nsIFile *file, bool aMustCacheFd, nsZipHandle **ret,
                       PRFileDesc **aFd = nullptr);
  static nsresult Init(nsZipArchive *zip, const char *entry,
                       nsZipHandle **ret);

  NS_METHOD_(MozExternalRefCountType) AddRef(void);
  NS_METHOD_(MozExternalRefCountType) Release(void);

  int64_t SizeOfMapping();

  nsresult GetNSPRFileDesc(PRFileDesc** aNSPRFileDesc);

protected:
  const uint8_t * mFileData; /* pointer to mmaped file */
  uint32_t        mLen;      /* length of file and memory mapped area */
  mozilla::FileLocation mFile; /* source file if any, for logging */

private:
  nsZipHandle();
  ~nsZipHandle();

  PRFileMap *                       mMap;    /* nspr datastructure for mmap */
  mozilla::AutoFDClose              mNSPRFileDesc;
  nsAutoPtr<nsZipItemPtr<uint8_t> > mBuf;
  mozilla::ThreadSafeAutoRefCnt     mRefCnt; /* ref count */
  NS_DECL_OWNINGTHREAD
};

nsresult gZlibInit(z_stream *zs);

#endif /* nsZipArchive_h_ */
