/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Daniel Veditz <dveditz@netscape.com>
 *     Samir Gehani <sgehani@netscape.com>
 */

#ifndef nsZipArchive_h_
#define nsZipArchive_h_

#define ZIP_MAGIC     0x5A49505FL   /* "ZIP_" */
#define ZIPFIND_MAGIC 0x5A495046L   /* "ZIPF" */
#define ZIP_TABSIZE   256
#define ZIP_BUFLEN    32767



class nsZipFind;

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

  nsZipItem*  next;

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
  PRInt32 OpenArchive( const char * aArchiveName );

  /**
   * ExtractFile 
   *
   * @param   aFilename  name of file in archive to extract
   * @param   aOutname   where to extract on disk
   * @return  status code
   */
  PRInt32 ExtractFile( const char * aFilename, const char * aOutname );

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
  PRInt32           CopyItemToDisk( const nsZipItem* aItem, const char* aOutname );
  const nsZipItem*  GetFileItem( const char * aFilename );
  PRUint32          HashName( const char* aName );
  PRInt32           InflateItemToDisk( const nsZipItem* aItem, const char* aOutname );
};




/** 
 * nsZipFind 
 *
 * a helper class for nsZipArchive, representing a search
 */
class nsZipFind
{
  friend class nsZipArchive;
#ifdef STANDALONE
  friend PRInt32 ZIP_FindNext( void*, char*, PRUint16 );
  friend PRInt32 ZIP_FindFree( void* );
#endif

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
