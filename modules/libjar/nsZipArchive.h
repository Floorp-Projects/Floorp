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

#define ZIP_MAGIC     0x5A49505FL   /* "ZIP_" */
#define ZIPFIND_MAGIC 0x5A495046L   /* "ZIPF" */
#define ZIP_TABSIZE   256
// Keep this odd. The -1 is significant.
#define ZIP_BUFLEN    (4 * 1024 - 1)

#ifdef STANDALONE
#define nsZipArchive nsZipArchiveStandalone

#define ZIP_Seek(fd,p,m) (fseek((fd),(p),(m))==0)

#else

#define PL_ARENA_CONST_ALIGN_MASK 7
#include "plarena.h"
#define ZIP_Seek(fd,p,m) (PR_Seek((fd),((PROffset32)p),(m))==((PROffset32)p))

#endif

#define ZIFLAG_SYMLINK      0x01  /* zip item is a symlink */
#define ZIFLAG_DATAOFFSET   0x02  /* zip item offset points to file data */

#include "zlib.h"

class nsZipFind;
class nsZipReadState;
class nsZipItemMetadata;

#ifndef STANDALONE
struct PRFileDesc;
#endif

/**
 * This file defines some of the basic structures used by libjar to
 * read Zip files. It makes use of zlib in order to do the decompression.
 *
 * A few notes on the classes:
 * nsZipArchive   represents a single Zip file, and maintains an index
 *                of all the items in the file.
 * nsZipItem      represents a single item (file) in the Zip archive.
 * nsZipReadState represents a read-in-progress, and maintains all
 *                zlib state
 * nsZipFind      represents the metadata involved in doing a search,
 *                and current state of the iteration of found objects.
 *
 * There is a lot of #ifdef STANDALONE here, and that is so that these
 * basic structures can be reused in a standalone static
 * library. In order for the code to be reused, these structures
 * should never use anything from XPCOM, including such obvious things
 * as NS_ASSERTION(). Instead, use the basic NSPR equivalents.
 *
 * There is one key difference in the way that this code behaves in
 * STANDALONE mode. In STANDALONE mode, the nsZipArchive owns the
 * single file descriptor and that is used to read the ZIP file. Since
 * there is only one nsZipArchive per file, you can only read one file
 * at a time from the Zip file.
 *
 * In non-STANDALONE mode, nsZipReadState owns the file descriptor.
 * Since there can be multiple nsZipReadStates in existence at the
 * same time, there can be multiple reads from the same nsZipArchive
 * happening at the same time.
 */

/**
 * nsZipItem -- a helper class for nsZipArchive
 *
 * each nsZipItem represents one file in the archive and all the
 * information needed to manipulate it.
 */
class nsZipItem
{
public:
  char*       name; /* '\0' terminated */

  PRUint32    offset;
  PRUint32    size;             /* size in original file */
  PRUint32    realsize;         /* inflated size */
  PRUint32    crc32;

  nsZipItem*  next;

  /*
   * Keep small items together, to avoid overhead.
   */
  PRUint16    mode;
  PRUint16    time;
  PRUint16    date;

  /*
   * Keep small items together, to avoid overhead.
   */
  PRUint8      compression;
  PRUint8      flags;

#ifndef STANDALONE
  /**
   * GetModTime
   *
   * Utility to get an NSPR-friendly formatted string
   * representing the last modified time of this item.
   * 
   * @return nsprstr    The modification time in PRTime
   */
  PRTime GetModTime();
#endif

  nsZipItem();
  ~nsZipItem();


private:
  //-- prevent copies and assignments
  nsZipItem& operator=(const nsZipItem& rhs);
  nsZipItem(const nsZipItem& rhs);

};

/** 
 * nsZipArchive -- a class for reading the PKZIP file format.
 *
 */
class nsZipArchive 
{
public:
  /** cookie used to validate supposed objects passed from C code */
  const PRInt32 kMagic;

  /** Block size by which Arena grows. Arena used by filelists */
  const PRUint32 kArenaBlockSize;

  /** constructing does not open the archive. See OpenArchive() */
  nsZipArchive();

  /** destructing the object closes the archive */
  ~nsZipArchive();

#ifdef STANDALONE
  /** helper routine for passing around mFd
   */
  PRFileDesc* GetFd() { return mFd; }
#endif
  
  /** 
   * OpenArchive 
   * 
   * It's an error to call this more than once on the same nsZipArchive
   * object. If we were allowed to use exceptions this would have been 
   * part of the constructor 
   *
   * @param   aArchiveName  full pathname of archive
   * @return  status code
   */
#ifdef STANDALONE
  PRInt32 OpenArchive(const char * aArchiveName);
#else
  PRInt32 OpenArchiveWithFileDesc(PRFileDesc* fd);
#endif

  /**
   * Test the integrity of items in this archive by running
   * a CRC check after extracting each item into a memory 
   * buffer.  If an entry name is supplied only the 
   * specified item is tested.  Else, if null is supplied
   * then all the items in the archive are tested.
   *
   * @return  status code       
   */
  PRInt32 Test(const char *aEntryName, PRFileDesc* aFd);

  /**
   * Closes an open archive.
   */
  PRInt32 CloseArchive();

  /** 
   * GetItem
   *
   * @param   aFilename Name of file in the archive
   * @return  status code
   */  
  PRInt32 GetItem(const char * aFilename, nsZipItem** result);
  
  /** 
   * ReadInit
   * 
   * Prepare to read from an item in the archive. Takes ownership of
   * the file descriptor, and will retain responsibility for closing
   * it property upon destruction.
   *
   * @param   zipEntry name of item in file
   * @param   aRead is filled with appropriate values
   * @return  status code
   */
  PRInt32 ReadInit(const char* zipEntry, nsZipReadState* aRead, PRFileDesc* aFd);


  /**
   * ExtractFile 
   *
   * @param   zipEntry  name of file in archive to extract
   * @param   aOutname   where to extract on disk
   * @return  status code
   */
  PRInt32 ExtractFile(const char * zipEntry, const char * aOutname,
                      PRFileDesc* aFd);

  PRInt32 ExtractItemToFileDesc(nsZipItem* item, PRFileDesc* outFD,
                                PRFileDesc* aFd);

  /**
   * FindInit
   *
   * Initializes a search for files in the archive. FindNext() returns
   * the actual matches. FindFree() must be called when you're done
   *
   * @param   aPattern    a string or RegExp pattern to search for
   *                      (may be NULL to find all files in archive)
   * @return  a structure used in FindNext. NULL indicates error
   */
  nsZipFind* FindInit(const char * aPattern);

  /**
   * FindNext
   *
   * @param   aFind   the Find structure returned by FindInit
   * @param   aResult the next item that matches the pattern
   */
  PRInt32 FindNext(nsZipFind* aFind, nsZipItem** aResult);

  PRInt32 FindFree(nsZipFind *aFind);

#ifdef XP_UNIX
  /**
   * ResolveSymLinks
   * @param path      where the file is located
   * @param zipItem   the zipItem related to "path" 
   */
  PRInt32  ResolveSymlink(const char *path, nsZipItem *zipItem);
#endif

private:
  //--- private members ---

  nsZipItem*    mFiles[ZIP_TABSIZE];
#ifndef STANDALONE
  PLArenaPool   mArena;
#else
  // in standalone, each nsZipArchive has its own mFd
  PRFileDesc    *mFd;
#endif

  //--- private methods ---
  
  nsZipArchive& operator=(const nsZipArchive& rhs); // prevent assignments
  nsZipArchive(const nsZipArchive& rhs);            // prevent copies

  PRInt32           BuildFileList(PRFileDesc* aFd);
  nsZipItem*        GetFileItem(const char * zipEntry);
  PRUint32          HashName(const char* aName);

  PRInt32           SeekToItem(const nsZipItem* aItem,
                               PRFileDesc* aFd);
  PRInt32           CopyItemToDisk(const nsZipItem* aItem, PRFileDesc* outFD,
                                   PRFileDesc* aFd);
  PRInt32           InflateItem(const nsZipItem* aItem, PRFileDesc* outFD,
                                PRFileDesc* aFd);
  PRInt32           TestItem(const nsZipItem *aItem,
                             PRFileDesc* aFd);

};

/** 
 * nsZipReadState
 *
 * a helper class for nsZipArchive, representing a read in progress
 */

class nsZipReadState
{
public:

  nsZipReadState() :
#ifndef STANDALONE
    mFd(0),
#endif
    mCurPos(0)
  { MOZ_COUNT_CTOR(nsZipReadState); }
  ~nsZipReadState()
  {
#ifndef STANDALONE
    if (mFd)
      PR_Close(mFd);
#endif
    MOZ_COUNT_DTOR(nsZipReadState);
  }

  void Init(nsZipItem* aZipItem, PRFileDesc* aFd);

#ifndef STANDALONE
  /** 
   * Read 
   * 
   * Read from the item specified to ReadInit. ReadInit must be 
   * called first.
   *
   * @param  aRead the structure returned by ReadInit
   * @param  buf buffer to write data into.
   * @param  count number of bytes to read
   * @param  actual (out) number of bytes read
   * @return  status code
   */
  PRInt32 Read(char* buf, PRUint32 count, PRUint32* actual);
#endif
  
  /**
   * Available
   *
   * Returns the number of bytes left to be read from the
   * item specified to ReadInit. ReadInit must be called first.
   *
   * @param aRead the structure returned by ReadInit
   * @return the number of bytes still to be read
   */
   PRUint32 Available();

#ifndef STANDALONE
  PRFileDesc*   mFd;
#endif

  nsZipItem*    mItem;
  PRUint32      mCurPos;
  unsigned char mReadBuf[ZIP_BUFLEN];
  z_stream      mZs;
  PRUint32      mCrc;

private:
#ifndef STANDALONE
  PRInt32 ContinueInflate(char* aBuf,
                          PRUint32 aCount,
                          PRUint32* aBytesRead);
  PRInt32 ContinueCopy(char* aBuf,
                       PRUint32 aCount,
                       PRUint32* aBytesRead);
#endif
  
  //-- prevent copies and assignments
  nsZipReadState& operator=(const nsZipReadState& rhs);
  nsZipReadState(const nsZipFind& rhs);
};

/** 
 * nsZipFind 
 *
 * a helper class for nsZipArchive, representing a search
 */
class nsZipFind
{
  friend class nsZipArchive;

public:
  const PRInt32       kMagic;

  nsZipFind(nsZipArchive* aZip, char* aPattern, PRBool regExp);
  ~nsZipFind();

  nsZipArchive* GetArchive();

private:
  nsZipArchive* mArchive;
  char*         mPattern;
  PRUint16      mSlot;
  nsZipItem*    mItem;
  PRBool        mRegExp;

  //-- prevent copies and assignments
  nsZipFind& operator=(const nsZipFind& rhs);
  nsZipFind(const nsZipFind& rhs);
};

#endif /* nsZipArchive_h_ */
