/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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

#ifndef nsIObserver_h__
#define nsIObserver_h__

#include "nsISupports.h"
#include "nscore.h"


// {DB242E01-E4D9-11d2-9DDE-000064657374}
#define NS_IOBSERVER_IID \
{ 0xdb242e01, 0xe4d9, 0x11d2, { 0x9d, 0xde, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } };


// {DB242E03-E4D9-11d2-9DDE-000064657374}
#define NS_OBSERVER_CID \
{ 0xdb242e03, 0xe4d9, 0x11d2, { 0x9d, 0xde, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } };

class nsIObserver : public nsISupports {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IOBSERVER_IID; return iid; }
    NS_IMETHOD   Notify(nsISupports** result) = 0;
};

extern NS_BASE nsresult NS_NewObserver(nsIObserver** anObserver);

#define NS_OBSERVER_PROGID             "component:||netscape|Observer"

#endif /* nsIObserver_h__ */
