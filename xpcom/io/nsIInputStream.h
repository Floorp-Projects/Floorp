/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef nsIInputStream_h___
#define nsIInputStream_h___

#include "nsIBaseStream.h"

#define NS_IINPUTSTREAM_IID   \
{ 0x022396f0, 0x93b5, 0x11d1, \
  {0x89, 0x5b, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

/** Abstract byte input stream */
class nsIInputStream : public nsIBaseStream {
public:

    static const nsIID& GetIID() { static nsIID iid = NS_IINPUTSTREAM_IID; return iid; }

    /** Return the number of bytes in the stream
     *  @param aLength out parameter to hold the length
     *         of the stream. if an error occurs, the length
     *         will be undefined
     *  @return error status
     */
    NS_IMETHOD
    GetLength(PRUint32 *aLength) = 0;

    /** Read data from the stream.
     *  @param aBuf the buffer into which the data is read
     *  @param aCount the maximum number of bytes to read
     *  @param aReadCount out parameter to hold the number of
     *         bytes read, eof if 0. if an error occurs, the
     *         read count will be undefined
     *  @return error status
     */   
    NS_IMETHOD
    Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount) = 0; 
};

#endif /* nsInputStream_h___ */
