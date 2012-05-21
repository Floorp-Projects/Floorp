/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_ILOCALSTORE_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsILocalStore, NS_ILOCALSTORE_IID)

extern nsresult
NS_NewLocalStore(nsISupports* aOuter, REFNSIID aIID, void** aResult);


#endif // nsILocalStore_h__
