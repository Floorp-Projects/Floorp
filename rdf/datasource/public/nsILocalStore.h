/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsILocalStore_h__
#define nsILocalStore_h__

#include "rdf.h"
#include "nsISupports.h"

// {DF71C6F1-EC53-11d2-BDCA-000064657374}
#define NS_ILOCALSTORE_IID \
{ 0xdf71c6f1, 0xec53, 0x11d2, { 0xbd, 0xca, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

// {DF71C6F0-EC53-11d2-BDCA-000064657374}
#define NS_LOCALSTORE_CID \
{ 0xdf71c6f0, 0xec53, 0x11d2, { 0xbd, 0xca, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

#define NS_LOCALSTORE_CONTRACTID NS_RDF_DATASOURCE_CONTRACTID_PREFIX "local-store"

class nsILocalStore : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_ILOCALSTORE_IID; return iid; }
};


extern NS_IMETHODIMP
NS_NewLocalStore(nsISupports* aOuter, REFNSIID aIID, void** aResult);


#endif // nsILocalStore_h__
