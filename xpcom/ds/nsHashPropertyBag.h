/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHashPropertyBag_h___
#define nsHashPropertyBag_h___

#include "nsCOMPtr.h"
#include "nsCOMArray.h"

#include "nsIVariant.h"
#include "nsIWritablePropertyBag.h"
#include "nsIWritablePropertyBag2.h"
#include "nsInterfaceHashtable.h"

class nsHashPropertyBag : public nsIWritablePropertyBag
                               , public nsIWritablePropertyBag2
{
public:
    nsHashPropertyBag() { }
    virtual ~nsHashPropertyBag() {}

    nsresult Init();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIPROPERTYBAG

    NS_DECL_NSIPROPERTYBAG2

    NS_DECL_NSIWRITABLEPROPERTYBAG

    NS_DECL_NSIWRITABLEPROPERTYBAG2

protected:
    // a hash table of string -> nsIVariant
    nsInterfaceHashtable<nsStringHashKey, nsIVariant> mPropertyHash;
};

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY NS_VISIBILITY_HIDDEN

// Note: NS_NewHashPropertyBag returns a HPB that
// uses a non-thread-safe internal hash
extern "C" nsresult
NS_NewHashPropertyBag(nsIWritablePropertyBag* *_retval);

#endif /* nsHashPropertyBag_h___ */
