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
 */


#ifndef nsJAR_h__
#define nsJAR_h__

#include "prtypes.h"
#include "nsJARIIDs.h"
#include "nsIJAR.h"


/*-------------------------------------------------------------------------
 * Class nsJAR declaration.  This is basically a "wrapper" class to 
 * take care of the XPCOM stuff for nsZipArchive.  You see, nsZipArchive
 * has a requirement to be standalone as well as an XPCOM component. 
 * This class allows nsZipArchive to remain non-XPCOM yet still be loadable
 * by Mozilla.
 *------------------------------------------------------------------------*/
class nsJAR : public nsIJAR
{
  //private:

    nsZipArchive zip;

  public:

    nsJAR();
    ~nsJAR();
    
    static const nsIID& IID() { static nsIID iid = NS_JAR_CID; return iid; }
  
    NS_DECL_ISUPPORTS
    
    NS_IMETHODIMP Open(const char* zipFileName, PRInt32 *aResult);
   
    NS_IMETHODIMP Extract(const char * aFilename, const char * aOutname, PRInt32 *aResult);

    NS_IMETHODIMP Find( const char * aPattern, nsZipFind *aResult);

};

#endif /* nsJAR_h__ */

