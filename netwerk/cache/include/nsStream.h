/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/* A placeholder for the actual professional strength streams class. Works
 * ok for me for now.
 *
 * -Gagan Saksena 09/15/98
 */

#ifndef nsStream_h__
#define nsStream_h__

//#include "nsISupports.h"

#include "prtypes.h"

class nsStream//: public nsISupports
{

public:
            nsStream();
    virtual ~nsStream();
/*
    NS_IMETHOD              QueryInterface(const nsIID& aIID, 
                                           void** aInstancePtr);
    NS_IMETHOD_(nsrefcnt)   AddRef(void);
    NS_IMETHOD_(nsrefcnt)   Release(void);
*/
    virtual
        PRInt32     Read(void* o_Buffer, PRUint32 i_Len) = 0;
    virtual
        void        Reset(void) = 0;
    virtual
        PRInt32     Write(const void* i_Buffer, PRUint32 i_Len) = 0;

protected:

private:
    nsStream(const nsStream& o);
    nsStream& operator=(const nsStream& o);
};

inline
nsStream::nsStream(void)
{
    //noop
}

inline
nsStream::~nsStream()
{
    //noop
}
#endif // nsStream_h__

