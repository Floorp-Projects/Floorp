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
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

#include "nsIBinaryInputStream.h"
#include "nsIBinaryOutputStream.h"
#include "nsCOMPtr.h"

class nsBinaryOutputStream : public nsIBinaryOutputStream
{
public:
    nsBinaryOutputStream(nsIOutputStream *aStream);
    virtual ~nsBinaryOutputStream() {};

private:
    
    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIOutputStream methods
    NS_DECL_NSIOUTPUTSTREAM

    // nsIBinaryOutputStream methods
    NS_DECL_NSIBINARYOUTPUTSTREAM

    // Call Write(), ensuring that all proffered data is written
    nsresult WriteFully(const char *aBuf, PRUint32 aCount);

protected:
    nsCOMPtr<nsIOutputStream> mOutputStream;
};

class nsBinaryInputStream : public nsIBinaryInputStream
{
public:
    nsBinaryInputStream(nsIInputStream *aStream);
    virtual ~nsBinaryInputStream() {};

private:
    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIInputStream methods
    NS_DECL_NSIINPUTSTREAM

    // nsIBinaryInputStream methods
    NS_DECL_NSIBINARYINPUTSTREAM
    
protected:
    nsCOMPtr<nsIInputStream> mInputStream;
};

