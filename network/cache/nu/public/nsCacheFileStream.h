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

/* nsCacheFileStream. The file/disk based implementation of the nsStream class.
 * It basically maps the read write functions to the file equivalents.
 * 
 * -Gagan Saksena 09/15/98
 */
#ifndef nsCacheFileStream_h__
#define nsCacheFileStream_h__

//#include "nsISupports.h"
#include "nsStream.h"
#include "prio.h" // PRFileDesc

/* now using the _real_ nsFileStream */
#include "nsFileSpec.h"
#include "nsFileStream.h"

class nsCacheFileStream: public nsStream
{

public:
            nsCacheFileStream(const char* i_Filename);
    virtual ~nsCacheFileStream();
/*
    NS_IMETHOD              QueryInterface(const nsIID& aIID, 
                                           void** aInstancePtr);
    NS_IMETHOD_(nsrefcnt)   AddRef(void);
    NS_IMETHOD_(nsrefcnt)   Release(void);
*/
#ifdef NU_CACHE_FILE_STREAM
    PRFileDesc* FileDesc(void);
#endif

    PRInt32     Read(void* o_Buffer, PRUint32 i_Len);
    void        Reset(void);
    PRInt32     Write(const void* i_Buffer, PRUint32 i_Len);

protected:

    PRBool      Open(void);

private:
    nsCacheFileStream(const nsCacheFileStream& o);
    nsCacheFileStream& operator=(const nsCacheFileStream& o);
#ifdef NU_CACHE_FILE_STREAM
    PRFileDesc* m_pFile;    
#else
    nsIOFileStream *m_pFile;
#endif
    char* m_pFilename;
};

#ifdef NU_CACHE_FILE_STREAM
inline
PRFileDesc* nsCacheFileStream::FileDesc(void)
{
    return m_pFile;
}
#endif

#endif // nsCacheFileStream_h__

