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
 *     Daniel Veditz <dveditz@netscape.com>
 *     Don Bragg <dbragg@netscape.com>
 */


#ifndef nsIZip_h__
#define nsIZip_h__

#include "nsZipIIDs.h"
#include "nsISupports.h"
#include "nsIFactory.h"

#include "nsZipArchive.h"


class nsIZip : public nsISupports 
{
  public:
    static const nsIID& IID() { static nsIID iid = NS_IZip_IID; return iid; }


    NS_IMETHOD Open(const char * zipFileName, PRInt32 *result)=0;
   
    NS_IMETHOD Extract(const char * aFilename,const char * aOutname, PRInt32 *result)=0;
   
    NS_IMETHOD Find(  const char * aPattern, nsZipFind *aResult )=0;
};

#endif // nsIZip_h__
