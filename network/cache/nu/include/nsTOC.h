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

#ifndef nsTOC_h__
#define nsTOC_h__

//#include "nsISupports.h"

#include "nsFlatFile.h"
#include "nsFFEntry.h"

class nsTOC//: public nsISupports
{

public:
            nsTOC(const char* i_pFilename, nsFlatFile* i_pFlatFile);
    virtual ~nsTOC();
/*
    NS_IMETHOD              QueryInterface(const nsIID& aIID, 
                                           void** aInstancePtr);
    NS_IMETHOD_(nsrefcnt)   AddRef(void);
    NS_IMETHOD_(nsrefcnt)   Release(void);
*/

    nsFFEntry*  AddEntry(PRUint32 i_size = kPAGE_SIZE);

    PRUint32    Entries(void) const;

    const char* Filename(void) const;
    
    nsFFEntry*  FirstDataEntry(void) const;
    
    nsFFEntry*  FreeEntry(void) const;

    void        Init(void);

    PRBool      IsValid(void) const;
    
protected:
    PRUint32    NextID(void) const;
    
    PRBool      Serialize(nsFFEntry* io_Entry, PRBool bRead = PR_FALSE);
    

private:
    nsTOC(const nsTOC& o);
    nsTOC& operator=(const nsTOC& o);

    nsFFEntry*      m_pContents; // The first entry is reserved for free space list
    PRUint32        m_Entries;
    char*           m_pFilename;
    nsFlatFile*     m_pFlatFile;
    PRFileDesc*     m_pFD;
    PRBool          m_bIsValid;
};

inline
PRUint32 nsTOC::Entries(void) const
{
    return m_Entries;
}

inline
const char* nsTOC::Filename(void) const
{
    return m_pFilename;
}

inline
nsFFEntry*  nsTOC::FirstDataEntry(void) const
{
    return m_pContents ? m_pContents->NextEntry() : 0;
}
    
inline
nsFFEntry* nsTOC::FreeEntry(void) const
{
    return m_pContents;
}

inline
PRBool nsTOC::IsValid(void) const
{
    return m_bIsValid;
}

inline
PRUint32 nsTOC::NextID(void) const
{
    return m_Entries + 1;
}
#endif // nsTOC_h__

