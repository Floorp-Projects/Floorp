/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsFileStreams_h__
#define nsFileStreams_h__

#include "nsIFileStreams.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIStreamIO.h"
#include "nsCOMPtr.h"
#include "prlog.h"

////////////////////////////////////////////////////////////////////////////////

class nsFileIO : public nsIFileIO
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMIO
    NS_DECL_NSIFILEIO

    nsFileIO();
    virtual ~nsFileIO();

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
    nsCOMPtr<nsIFile>   mFile;
    PRInt32             mIOFlags;
    PRInt32             mPerm;
    nsresult            mStatus;
#ifdef PR_LOGGING
    char*               mSpec;
#endif
};

////////////////////////////////////////////////////////////////////////////////

class nsFileStream : public nsISeekableStream
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISEEKABLESTREAM

    nsFileStream();
    virtual ~nsFileStream();

    nsresult Close();

protected:
    PRFileDesc*         mFD;
};

////////////////////////////////////////////////////////////////////////////////

class nsFileInputStream : public nsFileStream,
                          public nsIFileInputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSIFILEINPUTSTREAM

    nsFileInputStream() : nsFileStream() {}
    virtual ~nsFileInputStream() {}

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);
};

////////////////////////////////////////////////////////////////////////////////

class nsFileOutputStream : public nsFileStream,
                           public nsIFileOutputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIOUTPUTSTREAM
    NS_DECL_NSIFILEOUTPUTSTREAM

    nsFileOutputStream() : nsFileStream() {}
    virtual ~nsFileOutputStream() {}

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);
};

////////////////////////////////////////////////////////////////////////////////

#endif // nsFileStreams_h__
