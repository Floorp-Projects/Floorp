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

#ifndef nsFlatFile_h__
#define nsFlatFile_h__

#include <prtypes.h>
#include <prio.h>

//#include "nsISupports.h"

#ifdef IS_LITTLE_ENDIAN
#ifndef COPY_INT32
#define COPY_INT32(_a,_b)  memcpy(_a, _b, sizeof(int32))
#else
#define COPY_INT32(_a,_b)  /* swap */                   \
    do {                                                \
    ((char *)(_a))[0] = ((char *)(_b))[3];              \
    ((char *)(_a))[1] = ((char *)(_b))[2];              \
    ((char *)(_a))[2] = ((char *)(_b))[1];              \
    ((char *)(_a))[3] = ((char *)(_b))[0];              \
    } while(0)
#endif
#endif

/** A class to handle flat file storage mechanism */
static const PRUint32 kPAGE_SIZE =4096; 

class nsFlatFile //: public nsISupports
{
public:

            nsFlatFile(const char* i_pFilename, 
                const PRUint32 i_FileSize= 5242880); // 5 meg
            // TODO think if we want to substract the toc size from this?

    virtual ~nsFlatFile();

    const char*     Filename(void) const;

    PRUint32        HeaderSize(void) const;

    void            Init(void);

    PRBool          IsValid(void) const;
    
    PRUint32        Size(void) const;
    /*
    NS_IMETHOD              QueryInterface(const nsIID& aIID, 
                                           void** aInstancePtr);
    NS_IMETHOD_(nsrefcnt)   AddRef(void);
    NS_IMETHOD_(nsrefcnt)   Release(void);
    */
protected:

private:
    nsFlatFile(const nsFlatFile& o);
    nsFlatFile& operator=(const nsFlatFile& o);

    PRBool      m_bIsValid;
    char*       m_pFilename;
    PRUint32    m_Size;

    PRFileDesc* m_pFD;
    PRUint32    m_HeaderSize;
};

inline
const char* nsFlatFile::Filename(void) const
{
    return m_pFilename;
}

inline
PRUint32 nsFlatFile::HeaderSize(void) const
{
    return m_HeaderSize;
}

inline
PRBool nsFlatFile::IsValid(void) const
{
    return m_bIsValid;
}

inline
PRUint32 nsFlatFile::Size(void) const
{
    return m_Size;
}
#endif // nsFlatFile_h__

