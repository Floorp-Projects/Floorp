/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

#ifndef nsBinaryStream_h___
#define nsBinaryStream_h___

#include "nsCOMPtr.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsIStreamBufferAccess.h"

// Derive from nsIObjectOutputStream so this class can be used as a superclass
// by nsObjectOutputStream.
class nsBinaryOutputStream : public nsIObjectOutputStream
{
public:
    nsBinaryOutputStream(nsIOutputStream *aStream);
    virtual ~nsBinaryOutputStream() {};

protected:
    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIOutputStream methods
    NS_DECL_NSIOUTPUTSTREAM

    // nsIBinaryOutputStream methods
    NS_DECL_NSIBINARYOUTPUTSTREAM

    // nsIObjectOutputStream methods
    NS_DECL_NSIOBJECTOUTPUTSTREAM

    // Call Write(), ensuring that all proffered data is written
    nsresult WriteFully(const char *aBuf, PRUint32 aCount);

    nsCOMPtr<nsIOutputStream>       mOutputStream;
    nsCOMPtr<nsIStreamBufferAccess> mBufferAccess;
};

// Derive from nsIObjectInputStream so this class can be used as a superclass
// by nsObjectInputStream.
class nsBinaryInputStream : public nsIObjectInputStream
{
public:
    nsBinaryInputStream(nsIInputStream *aStream);
    virtual ~nsBinaryInputStream() {};

protected:
    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIInputStream methods
    NS_DECL_NSIINPUTSTREAM

    // nsIBinaryInputStream methods
    NS_DECL_NSIBINARYINPUTSTREAM

    // nsIObjectInputStream methods
    NS_DECL_NSIOBJECTINPUTSTREAM

    nsCOMPtr<nsIInputStream>        mInputStream;
    nsCOMPtr<nsIStreamBufferAccess> mBufferAccess;
};

#endif // nsBinaryStream_h___
