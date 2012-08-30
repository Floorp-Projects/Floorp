/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIDirIndex.h"
#include "nsString.h"
#include "mozilla/Attributes.h"

/* CID: {f6913e2e-1dd1-11b2-84be-f455dee342af} */

class nsDirIndex MOZ_FINAL : public nsIDirIndex {
public:
    nsDirIndex();
    ~nsDirIndex();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIDIRINDEX

protected:
    uint32_t mType;
    nsXPIDLCString mContentType;
    nsXPIDLCString mLocation;
    nsString mDescription;
    int64_t mSize;
    PRTime mLastModified;
};
