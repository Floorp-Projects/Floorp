/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* This class is the actual implementation of the original class nsBkgThread, 
 * but I am too lazy to move the comments from nsBkgThread's 
 * header file to here. :-)
 * 
 * -Gagan Saksena 09/15/98
 */
#ifndef nsCacheBkgThd_h__
#define nsCacheBkgThd_h__

#include "nsBkgThread.h"

class nsCacheBkgThd: public nsBkgThread
{

public:
            nsCacheBkgThd(PRIntervalTime iSleepTime);
    virtual ~nsCacheBkgThd();
/*
    NS_IMETHOD              QueryInterface(const nsIID& aIID, 
                                           void** aInstancePtr);
    NS_IMETHOD_(nsrefcnt)   AddRef(void);
    NS_IMETHOD_(nsrefcnt)   Release(void);
*/
    void        Run(void);

protected:

private:
    nsCacheBkgThd(const nsCacheBkgThd& o);
    nsCacheBkgThd& operator=(const nsCacheBkgThd& o);
};

#endif // nsCacheBkgThd_h__

