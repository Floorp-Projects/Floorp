/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsISoftwareUpdate_h__
#define nsISoftwareUpdate_h__

#include "nsISupports.h"
#include "nsIFactory.h"

//FIX NEED REAL IID
#define NS_ISOFTWAREUPDATE_IID \
 { 0x8648c1e0, 0x938b, 0x11d2, \
  { 0xb0, 0xdd, 0x0, 0x80, 0x5f, 0x8a, 0x88, 0x99 }} 

class nsISoftwareUpdate : public nsISupports 
{
    public:
        static const nsIID& IID() { static nsIID iid = NS_ISOFTWAREUPDATE_IID; return iid; }

            NS_IMETHOD Startup() = 0;
            NS_IMETHOD Shutdown()= 0;
  
};



class nsSoftwareUpdateFactory : public nsIFactory 
{
    public:
        
        nsSoftwareUpdateFactory();
        ~nsSoftwareUpdateFactory();
        
        NS_DECL_ISUPPORTS

              NS_IMETHOD CreateInstance(nsISupports *aOuter,
                                        REFNSIID aIID,
                                        void **aResult);

              NS_IMETHOD LockFactory(PRBool aLock);

};


#endif // nsISoftwareUpdate_h__

