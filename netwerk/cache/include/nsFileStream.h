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

/* nsFileStream. The file/disk based implementation of the nsStream class.
 * It basically maps the read write functions to the file equivalents.
 * 
 * -Gagan Saksena 09/15/98
 */
#ifndef nsFileStream_h__
#define nsFileStream_h__

//#include "nsISupports.h"
#include "nsStream.h"
#include "prio.h" // PRFileDesc

class nsFileStream: public nsStream
{

public:
            nsFileStream(const char* i_Filename);
    virtual ~nsFileStream();
/*
    NS_IMETHOD              QueryInterface(const nsIID& aIID, 
                                           void** aInstancePtr);
    NS_IMETHOD_(nsrefcnt)   AddRef(void);
    NS_IMETHOD_(nsrefcnt)   Release(void);
*/
    PRFileDesc* FileDesc(void);

    PRInt32     Read(void* o_Buffer, PRUint32 i_Len);
    void        Reset(void);
    PRInt32     Write(const void* i_Buffer, PRUint32 i_Len);

protected:

    PRBool      Open(void);

private:
    nsFileStream(const nsFileStream& o);
    nsFileStream& operator=(const nsFileStream& o);
    PRFileDesc* m_pFile;    
    char* m_pFilename;
};

inline
PRFileDesc* nsFileStream::FileDesc(void)
{
    return m_pFile;
}

#endif // nsFileStream_h__

