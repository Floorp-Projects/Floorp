/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this Mem are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this Mem except in
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

/* nsMemStream. A memory based stream for use with memory objects 
 *
 * -Gagan Saksena 09/15/98.
 */
#ifndef nsMemStream_h__
#define nsMemStream_h__

//#include "nsISupports.h"
#include "nsStream.h"

class nsMemStream: public nsStream
{

public:
            nsMemStream();
    virtual ~nsMemStream();
/*
    NS_IMETHOD              QueryInterface(const nsIID& aIID, 
                                           void** aInstancePtr);
    NS_IMETHOD_(nsrefcnt)   AddRef(void);
    NS_IMETHOD_(nsrefcnt)   Release(void);
*/
    PRInt32     Read(void* o_Buffer, PRUint32 i_Len);
    void        Reset(void);
    PRInt32     Write(const void* i_Buffer, PRUint32 i_Len);

protected:

private:
    nsMemStream(const nsMemStream& o);
    nsMemStream& operator=(const nsMemStream& o);

    PRUint32    m_AllocSize; 
    PRUint32    m_ReadOffset;
    PRUint32    m_WriteOffset;
    void*       m_pStart;
    PRUint32    m_Size;
};

#endif // nsMemStream_h__

