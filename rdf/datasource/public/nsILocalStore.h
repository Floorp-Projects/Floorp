/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsILocalStore_h__
#define nsILocalStore_h__

#include "nsISupports.h"

// {DF71C6F1-EC53-11d2-BDCA-000064657374}
#define NS_ILOCALSTORE_IID \
{ 0xdf71c6f1, 0xec53, 0x11d2, { 0xbd, 0xca, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

class nsILocalStore : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_ILOCALSTORE_IID; return iid; }
};


PR_EXTERN(nsresult)
NS_NewLocalStore(nsILocalStore** aResult);


#endif // nsILocalStore_h__
