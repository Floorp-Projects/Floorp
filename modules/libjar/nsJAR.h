/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Don Bragg <dbragg@netscape.com>
 *     Samir Gehani <sgehani@netscape.com>
 */


#ifndef nsJAR_h__
#define nsJAR_h__

#include "prtypes.h"
#include "nsIJAR.h"
#include "nsIEnumerator.h"
#include "nsZipArchive.h"
#include "zipfile.h"


/*-------------------------------------------------------------------------
 * Class nsJAR declaration.  This is basically a "wrapper" class to 
 * take care of the XPCOM stuff for nsZipArchive.  You see, nsZipArchive
 * has a requirement to be standalone as well as an XPCOM component. 
 * This class allows nsZipArchive to remain non-XPCOM yet still be loadable
 * by Mozilla.
 *------------------------------------------------------------------------*/
class nsJAR : public nsIJAR
{
  public:

    nsJAR();
    virtual ~nsJAR();
    
    NS_DEFINE_STATIC_CID_ACCESSOR( NS_JAR_CID );
  
    NS_DECL_ISUPPORTS

    NS_DECL_NSIZIP
  
  private:

    nsZipArchive zip;
};




/**
 * nsJARItem
 *
 * An individual JAR entry. A set of nsJARItems macthing a
 * supplied pattern are returned in a nsJAREnumerator.
 */
class nsJARItem : public nsZipItem, public nsIJARItem
{
public:
    NS_DECL_ISUPPORTS

    //NS_DEFINE_STATIC_CID_ACCESSOR( NS_JARITEM_CID );

    NS_DECL_NSIJARITEM

    nsJARItem(nsZipItem* aOther);
    nsJARItem();
    virtual ~nsJARItem();
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

    //NS_DEFINE_STATIC_CID_ACCESSOR( NS_JARENUMERATOR_CID );

    // nsISimpleEnumerator methods
    NS_IMETHOD HasMoreElements(PRBool* aResult);
    NS_IMETHOD GetNext(nsISupports** aResult);

    nsJAREnumerator(nsZipFind *aFind);
    virtual ~nsJAREnumerator();

protected:
    nsZipArchive *mArchive; // pointer extracted from mFind for efficiency
    nsZipFind    *mFind;
    nsZipItem    *mCurr;    // raw pointer to an nsZipItem owned by mArchive -- DON'T delete
    PRBool        mIsCurrStale;
};

#endif /* nsJAR_h__ */

