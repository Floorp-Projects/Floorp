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
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsDirectoryIndexStream_h__
#define nsDirectoryIndexStream_h__

#include "nsIFile.h"
#include "nsString.h"
#include "nsIInputStream.h"
#include "nsCOMPtr.h"

class nsDirectoryIndexStream : public nsIInputStream
{
protected:
    nsCAutoString mBuf;
    PRInt32 mOffset;

    nsCOMPtr<nsIFile> mDir;
    nsCOMPtr<nsISimpleEnumerator> mIter;

    nsDirectoryIndexStream();
    nsresult Init(nsIFile* aDir);
    virtual ~nsDirectoryIndexStream();

public:
    static nsresult
    Create(nsIFile* aDir, nsIInputStream** aStreamResult);

    // nsISupportsInterface
    NS_DECL_ISUPPORTS

    // nsIInputStream interface
    NS_DECL_NSIINPUTSTREAM
};

#endif // nsDirectoryIndexStream_h__

