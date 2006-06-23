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
 * A few notes on the classes/structs:
 * nsZipArchive   represents a single Zip file, and maintains an index
 *                of all the items in the file.
 * nsZipItem      represents a single item (file) in the Zip archive.
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
 * STANDALONE mode. The nsZipArchive owns a single file descriptor and 
 * that is used to read the ZIP file index, and for 'Test' and 'Extract'. 
 * Since there is only one nsZipArchive per file, you can only Test/Extract 
 * one file at a time from the Zip file.
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
struct nsZipItem
{
  nsZipItem*  next;

  PRUint32    headerOffset;
  PRUint32    dataOffset;
  PRUint32    size;             /* size in original file */
  PRUint32    realsize;         /* inflated size */
  PRUint32    crc32;

  /*
   * Keep small items together, to avoid overhead.
   */
  PRUint16     time;
  PRUint16     date;
  PRUint16     mode;
  PRUint8      compression;
  PRPackedBool hasDataOffset : 1;
  PRPackedBool isDirectory : 1; 
  PRPackedBool isSynthetic : 1;  /* whether item is an actual zip entry or was
                                    generated as part of a real entry's path,
                                    e.g. foo/ in a zip containing only foo/a.txt
                                    and no foo/ entry is synthetic */
#if defined(XP_UNIX) || defined(XP_BEOS)
  PRPackedBool isSymlink : 1;
#endif

  char        name[1]; // actually, bigger than 1
};

/** 
 * nsZipArchive -- a class for reading the PKZIP file format.
 *
 */
class nsZipArchive 
{
  friend class nsZipFind;

public:
#ifdef STANDALONE
  /** cookie used to validate supposed objects passed from C code */
  const PRInt32 kMagic;
#endif

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
   * @param   fd            File descriptor of file to open
   * @return  status code
   */
  nsresult OpenArchive(PRFileDesc* fd);

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

  /**
   * Moves the filepointer aFd to the start of data of the aItem.
   * @param   aItem       Pointer to nsZipItem
   * @param   aFd         The filepointer to move
   */
  nsresult  SeekToItem(nsZipItem* aItem, PRFileDesc* aFd);

private:
  //--- private members ---

  nsZipItem*    mFiles[ZIP_TABSIZE];
#ifndef STANDALONE
  PLArenaPool   mArena;
#endif

  // Used for central directory reading, and for Test and Extract
  PRFileDesc    *mFd;

  //--- private methods ---
  
  nsZipArchive& operator=(const nsZipArchive& rhs); // prevent assignments
  nsZipArchive(const nsZipArchive& rhs);            // prevent copies

  nsZipItem*        CreateZipItem(PRUint16 namelen);
  nsresult          BuildFileList();

  nsresult  CopyItemToDisk(PRUint32 size, PRUint32 crc, PRFileDesc* outFD);
  nsresult  InflateItem(const nsZipItem* aItem, PRFileDesc* outFD);
};


/** 
 * nsZipFind 
 *
 * a helper class for nsZipArchive, representing a search
 */
class nsZipFind
{
public:
#ifdef STANDALONE
  const PRInt32       kMagic;
#endif

  nsZipFind(nsZipArchive* aZip, char* aPattern, PRBool regExp);
  ~nsZipFind();

  nsresult      FindNext(const char ** aResult);

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

nsresult gZlibInit(z_stream *zs);

#endif /* nsZipArchive_h_ */
