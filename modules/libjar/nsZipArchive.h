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
 */

#include "prtypes.h"

#define ZIP_MAGIC   0x5F5A4950L   /* "_ZIP" */
#define ZIP_TABSIZE 256
#define ZIP_BUFLEN  32767

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

  PRInt32 FindInit( const char * aPattern );
  PRInt32 FindNext( char * aBuf, PRUint16 aSize );

private:
  //--- private members ---
  
  PRFileDesc    *mFd;

  nsZipItem*    mFiles[ZIP_TABSIZE];

  char*         mPattern;
  PRUint16      mPatternSlot;
  nsZipItem*    mPatternItem;
  PRBool        mPatternIsRegExp;

  //--- private methods ---
  
  nsZipArchive& operator=(const nsZipArchive& rhs); // prevent assignments
  nsZipArchive(const nsZipArchive& rhs);            // prevent copies

  PRInt32           BuildFileList();
  PRInt32           CopyItemToDisk( const nsZipItem* aItem, const char* aOutname );
  const nsZipItem*  GetFileItem( const char * aFilename );
  PRUint16          HashName( const char* aName );
  PRInt32           InflateItemToDisk( const nsZipItem* aItem, const char* aOutname );
};

