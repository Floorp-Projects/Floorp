/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Don Bragg <dbragg@netscape.com>
 *     Samir Gehani <sgehani@netscape.com>
 *     Mitch Stoltz <mstoltz@netscape.com>
 */


#ifndef nsJAR_h__
#define nsJAR_h__

#include "nscore.h"
#include "pratom.h"
#include "prmem.h"
#include "prio.h"
#include "plstr.h"
#include "prlog.h"
#include "prtypes.h"
#include "xp_regexp.h"

#include "nsRepository.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIFile.h"
#include "nsIEnumerator.h"
#include "nsVoidArray.h"
#include "nsHashtable.h"

#include "nsIZipReader.h"
#include "nsZipArchive.h"
#include "zipfile.h"

class nsIInputStream;
class nsIPrincipal;

/*-------------------------------------------------------------------------
 * Class nsJAR declaration. 
 * nsJAR serves as an XPCOM wrapper for nsZipArchive with the addition of 
 * JAR manifest file parsing. 
 *------------------------------------------------------------------------*/
class nsJAR : public nsIZipReader
{
  // Allows nsJARInputStream to call the verification functions
  friend class nsJARInputStream;

  public:

    nsJAR();
    virtual ~nsJAR();
    
    NS_DEFINE_STATIC_CID_ACCESSOR( NS_ZIPREADER_CID );
  
    NS_DECL_ISUPPORTS

    NS_DECL_NSIZIPREADER
  
  private:
    //-- Private data members
    nsCOMPtr<nsIFile>        mZipFile;      // The zip/jar file on disk
    nsZipArchive             mZip;          // The underlying zip archive
    nsObjectHashtable        mManifestData; // Stores metadata for each entry
    PRBool                   step1Complete; // True if manifest has been parsed

    //-- Private functions
    nsresult CreateInputStream(const char* aFilename, nsJAR* aJAR, 
                               nsIInputStream** is);
    nsresult LoadEntry(const char* aFilename, char** aBuf, 
                       PRUint32* aBufLen = nsnull);
    PRInt32  ReadLine(const char** src); 
    nsresult ParseOneFile(const char* filebuf, PRInt16 aFileType,
                          nsIPrincipal* aPrincipal, PRInt16 aPreStatus);
    nsresult VerifyEntry(const char* aEntryName, char* aEntryData, 
                         PRUint32 aLen);

    nsresult CalculateDigest(const char* aInBuf, PRUint32 aInBufLen,
                             char** digest);
    nsresult VerifySignature(const char* sfBuf, PRUint32 sfBufLen,
                             const char* rsaBuf, PRUint32 rsaBufLen, 
                             nsIPrincipal** aPrincipal, PRInt16* status);

    //-- Debugging
    void DumpMetadata(const char* aMessage);
};

/**
 * nsJARItem
 *
 * An individual JAR entry. A set of nsJARItems macthing a
 * supplied pattern are returned in a nsJAREnumerator.
 */
class nsJARItem : public nsIZipEntry
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIZIPENTRY
    
    void Init(nsZipItem* aZipItem);

    nsJARItem();
    virtual ~nsJARItem();

    private:
    nsZipItem* mZipItem;
};

/**
 * nsJAREnumerator
 *
 * Enumerates a list of files in a zip archive 
 * (based on a pattern match in its member nsZipFind).
 */
class nsJAREnumerator : public nsISimpleEnumerator
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISIMPLEENUMERATOR

    nsJAREnumerator(nsZipFind *aFind);
    virtual ~nsJAREnumerator();

protected:
    nsZipArchive *mArchive; // pointer extracted from mFind for efficiency
    nsZipFind    *mFind;
    nsZipItem    *mCurr;    // raw pointer to an nsZipItem owned by mArchive -- DON'T delete
    PRBool        mIsCurrStale;
};

#endif /* nsJAR_h__ */

