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
 * Copyright (C) 1998 Netscape Communications Corporation. All
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
#include "nsString2.h"
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
 * Supported secure digest algorithms
 *------------------------------------------------------------------------*/
#define JAR_DIGEST_COUNT 2
#define JAR_SHA1  0
#define JAR_MD5   1

/*-------------------------------------------------------------------------
 * Class nsJAR declaration. 
 * nsJAR serves as an XPCOM wrapper for nsZipArchive with the addition of 
 * JAR manifest file parsing. 
 *------------------------------------------------------------------------*/
class nsJAR : public nsIZipReader
{
  // Allows nsJARInputStream to call the digest functions
  friend class nsJARInputStream;

  public:

    nsJAR();
    virtual ~nsJAR();
    
    NS_DEFINE_STATIC_CID_ACCESSOR( NS_ZIPREADER_CID );
  
    NS_DECL_ISUPPORTS

    NS_DECL_NSIZIPREADER
  
  private:
    //-- Private data members
    nsCOMPtr<nsIFile>   mZipFile;
    nsZipArchive        mZip;
    nsObjectHashtable   mManifestData;
    PRBool              manifestParsed;
    nsISupports*        mVerificationService;

    //-- Private functions
    nsresult CreateInputStream(const char* aFilename, nsJAR* aJAR, nsIInputStream** is);
    nsresult LoadEntry(const char* aFilename, const char** aBuf, 
                       PRUint32* aBufLen = nsnull);
    PRInt32  ReadLine(const char** src); 
    nsresult ParseOneFile(const char* filebuf, PRInt16 aFileType, 
                          nsIPrincipal* aPrincipal = nsnull, 
                          PRBool aLastSFFile = PR_TRUE);

    //-- The following private functions are implemented in nsJARStubs.cpp
    static PRBool SupportsRSAVerification();
    nsresult DigestBegin(PRUint32* id, PRInt32 alg);
    nsresult DigestData(PRUint32 id, const char* data, PRUint32 length);
    nsresult CalculateDigest(PRUint32 id, char** digest);
    nsresult VerifySignature(const char* sfBuf, const char* rsaBuf,
                             PRUint32 rsaBufLen, nsIPrincipal** aPrincipal);

    void DumpMetadata(); // Debugging
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
 * nsJARManifestItem
 *
 * Meta-information pertaining to an individual JAR entry, taken
 * from the META-INF/MANIFEST.MF and META-INF/ *.SF files. Ideally,
 * this would be part of nsJARItem, but nsJARItems are stored in the 
 * underlying nsZipArchive which doesn't know about metadata.
 */
typedef enum
{
  JAR_INVALID       = 1,
  JAR_GLOBAL        = 2,
  JAR_INTERNAL      = 3,
  JAR_EXTERNAL      = 4
  // Maybe we'll want jar files to sign URL's eventually.
} JARManifestItemType;

// Use this macro to look up global data (from the manifest file header)
#define JAR_GLOBALMETA "" 

class nsJARManifestItem
{
public:
    JARManifestItemType mType;

    // True if both steps of verification have taken place:
    // ParseManifest and reading the item into memory via GetInputStream
    PRBool              verifyComplete; 

    // True unless one or more verification steps failed
    PRBool              valid;

    // Internal  storage of digests
    char*               calculatedSectionDigests[JAR_DIGEST_COUNT];
    char*               calculatedEntryDigests[JAR_DIGEST_COUNT];
    char*               storedEntryDigests[JAR_DIGEST_COUNT];
    PRBool              entryDigestsCalculated;

    void setPrincipal(nsIPrincipal *aPrincipal);
    
    void getPrincipal(nsIPrincipal **result);

    PRBool hasPrincipal();

    nsJARManifestItem();
    virtual ~nsJARManifestItem();

private:
      // the entity which signed this item
    nsIPrincipal*       mPrincipal;

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

