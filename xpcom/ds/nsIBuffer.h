/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIBuffer_h___
#define nsIBuffer_h___

#include "nsISupports.h"
#include "nscore.h"

class nsIInputStream;
class nsIAllocator;
class nsIInputStream;
class nsIOutputStream;

#define NS_IBUFFER_IID                               \
{ /* 1eebb300-fb8b-11d2-9324-00104ba0fd40 */         \
    0x1eebb300,                                      \
    0xfb8b,                                          \
    0x11d2,                                          \
    {0x93, 0x24, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

#define NS_BUFFER_CID                                \
{ /* 5dbe4de0-fbab-11d2-9324-00104ba0fd40 */         \
    0x5dbe4de0,                                      \
    0xfbab,                                          \
    0x11d2,                                          \
    {0x93, 0x24, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

class nsIBuffer : public nsISupports {
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IBUFFER_IID);

    NS_IMETHOD Init(PRUint32 growBySize, PRUint32 maxSize,
                    nsIAllocator* allocator) = 0;

    NS_IMETHOD Read(char* toBuf, PRUint32 bufLen, PRUint32 *readCount) = 0;
    NS_IMETHOD GetReadBuffer(PRUint32 *readBufferLength, char* *result) = 0;

    NS_IMETHOD Write(const char* fromBuf, PRUint32 bufLen, PRUint32 *writeCount) = 0;
    NS_IMETHOD Write(nsIInputStream* fromStream, PRUint32 *writeCount) = 0;
    NS_IMETHOD GetWriteBuffer(PRUint32 *writeBufferLength, char* *result) = 0;
    NS_IMETHOD SetEOF() = 0;
};

extern NS_COM nsresult
NS_NewBuffer(nsIBuffer* *result,
             PRUint32 growBySize, PRUint32 maxSize);

extern NS_COM nsresult
NS_NewPageBuffer(nsIBuffer* *result,
                 PRUint32 growBySize, PRUint32 maxSize);

extern NS_COM nsresult
NS_NewBufferInputStream(nsIInputStream* *result,
                        nsIBuffer* buffer, PRBool blocking = PR_FALSE);

extern NS_COM nsresult
NS_NewBufferOutputStream(nsIOutputStream* *result,
                         nsIBuffer* buffer, PRBool blocking = PR_FALSE);

extern NS_COM nsresult
NS_NewPipe2(nsIInputStream* *inStrResult,
           nsIOutputStream* *outStrResult,
           PRUint32 growBySize, PRUint32 maxSize);

#endif // nsIBuffer_h___
