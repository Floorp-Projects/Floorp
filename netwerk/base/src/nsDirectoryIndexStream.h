/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDirectoryIndexStream_h__
#define nsDirectoryIndexStream_h__

#include "mozilla/Attributes.h"

#include "nsString.h"
#include "nsIInputStream.h"
#include "nsCOMArray.h"

class nsIFile;

class nsDirectoryIndexStream MOZ_FINAL : public nsIInputStream
{
private:
    nsCString mBuf;
    int32_t mOffset;
    nsresult mStatus;

    int32_t             mPos;   // position within mArray
    nsCOMArray<nsIFile> mArray; // file objects within the directory

    nsDirectoryIndexStream();
    /**
     * aDir will only be used on the calling thread.
     */ 
    nsresult Init(nsIFile* aDir);
    ~nsDirectoryIndexStream();

public:
    /**
     * aDir will only be used on the calling thread.
     */ 
    static nsresult
    Create(nsIFile* aDir, nsIInputStream** aStreamResult);

    // nsISupportsInterface
    NS_DECL_THREADSAFE_ISUPPORTS

    // nsIInputStream interface
    NS_DECL_NSIINPUTSTREAM
};

#endif // nsDirectoryIndexStream_h__

