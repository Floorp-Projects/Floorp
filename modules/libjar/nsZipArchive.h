/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Copyright (C) 1998-1999 Netscape Communications  Corporation.. All
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

#ifndef nsZipArchive_h_
#define nsZipArchive_h_

#define ZIP_MAGIC     0x5A49505FL   /* "ZIP_" */
#define ZIPFIND_MAGIC 0x5A495046L   /* "ZIPF" */
#define ZIP_TABSIZE   256
#define ZIP_BUFLEN    32767

#ifdef STANDALONE
#define nsZipArchive nsZipArchiveStandalone

#define ZIP_Seek(fd,p,m) (fseek((fd),(p),(m))==0)
#else
#define ZIP_Seek(fd,p,m) (PR_Seek((fd),(p),(m))==(p))
#endif

class nsZipFind;
class nsZipRead;
class nsZipItemMetadata;

/**
 * nsZipItem -- a helper class for nsZipArchive
 *
 * each nsZipItem represents one file in the archive and all the
 * information needed to manipulate it.
 */
class nsZipItem
{
public:
  char*       name;
  PRUint32    namelen;

  PRUint32    offset;
  PRUint32    headerloc;
  PRUint16    compression;
  PRUint32    size;
  PRUint32    realsize;
  PRUint32    crc32;
  PRUint16    mode;
  PRUint16    time;
  PRUint16    date;

  nsZipItem*  next;

  /**
   * GetModTime
   *
   * Utility to get an NSPR-friendly formatted string
   * representing the last modified time of this item.
   * 
   * @return nsprstr    an NSPR-friendly string representation 
   *                    of the modification time
   */
  char *GetModTime();

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
   * @param   aArchiveName  full pathname of archive
   * @return  status code
   */
  PRInt32 OpenArchive(const char * aArchiveName);

  PRInt32 OpenArchiveWithFileDesc(PRFileDesc* fd);

  /**
   * Test the integrity of items in this archive by running
   * a CRC check after extracting each item into a memory 
   * buffer.  If an entry name is supplied only the 
   * specified item is tested.  Else, if null is supplied
   * then all the items in the archive are tested.
   *
   * @return  status code       
   */
  PRInt32 Test(const char *aEntryName);

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
   * Prepare to read from an item in the archive. Must be called
   * before any calls to Read or Available
   *
   * @param   zipEntry name of item in file
   * @param   (out) a structure used by Read and Available
   * @return  status code
   */
  PRInt32 ReadInit(const char* zipEntry, nsZipRead** aRead);

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
  PRInt32 Read(nsZipRead* aRead, char* buf, PRUint32 count, PRUint32* actual );

  /**
   * Available
   *
   * Returns the number of bytes left to be read from the
   * item specified to ReadInit. ReadInit must be called first.
   *
   * @param aRead the structure returned by ReadInit
   * @return the number of bytes still to be read
   */
   PRUint32 Available(nsZipRead* aRead);

  /**
   * ExtractFile 
   *
   * @param   zipEntry  name of file in archive to extract
   * @param   aOutname   where to extract on disk
   * @return  status code
   */
  PRInt32 ExtractFile( const char * zipEntry, const char * aOutname );

  PRInt32 ExtractFileToFileDesc(const char * zipEntry, PRFileDesc* outFD,
                                nsZipItem **outItem);

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
  nsZipFind* FindInit( const char * aPattern );

  /**
   * FindNext
   *
   * @param   aFind   the Find structure returned by FindInit
   * @param   aResult the next item that matches the pattern
   */
  PRInt32 FindNext( nsZipFind* aFind, nsZipItem** aResult);

  PRInt32 FindFree( nsZipFind *aFind );

private:
  //--- private members ---
  
  PRFileDesc    *mFd;
  nsZipItem*    mFiles[ZIP_TABSIZE];

  //--- private methods ---
  
  nsZipArchive& operator=(const nsZipArchive& rhs); // prevent assignments
  nsZipArchive(const nsZipArchive& rhs);            // prevent copies

  PRInt32           BuildFileList();
  nsZipItem*        GetFileItem( const char * zipEntry );
  PRUint32          HashName( const char* aName );

  PRInt32           SeekToItem(const nsZipItem* aItem);
  PRInt32           CopyItemToBuffer(const nsZipItem* aItem, char* aBuf);
  PRInt32           CopyItemToDisk( const nsZipItem* aItem, PRFileDesc* outFD );
  PRInt32           InflateItem( const nsZipItem* aItem, 
                                 PRFileDesc* outFD,
                                 char* buf );
  PRInt32           TestItem( const nsZipItem *aItem );
};

/** 
 * nsZipRead
 *
 * a helper class for nsZipArchive, representing a read in progress
 */
class nsZipRead
{
public:

  nsZipRead( nsZipArchive* aZip, nsZipItem* item );
  ~nsZipRead();

  nsZipArchive* mArchive;
  nsZipItem*    mItem;
  PRUint32      mCurPos;
  char*         mFileBuffer;

private:
  //-- prevent copies and assignments
  nsZipRead& operator=(const nsZipRead& rhs);
  nsZipRead(const nsZipFind& rhs);
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

  nsZipFind( nsZipArchive* aZip, char* aPattern, PRBool regExp );
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
