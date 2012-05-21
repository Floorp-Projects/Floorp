/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsErrorService_h__
#define nsErrorService_h__

#include "mozilla/Attributes.h"

#include "nsIErrorService.h"
#include "nsHashtable.h"

class nsInt2StrHashtable
{
public:
    nsInt2StrHashtable();

    nsresult  Put(PRUint32 key, const char* aData);
    char*     Get(PRUint32 key);
    nsresult  Remove(PRUint32 key);

protected:
    nsObjectHashtable mHashtable;
};

class nsErrorService MOZ_FINAL : public nsIErrorService
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIERRORSERVICE

    nsErrorService() {}

    static nsresult
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

private:
    ~nsErrorService() {}

protected:
    nsInt2StrHashtable mErrorStringBundleURLMap;
    nsInt2StrHashtable mErrorStringBundleKeyMap;
};

#endif // nsErrorService_h__
