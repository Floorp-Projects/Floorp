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

#include "nsMemStream.h"
#include "prmem.h"
#include "prlog.h" /* Assert */

#ifndef XP_MAC
#include "memory.h"
#else
#include "cstring"
#endif

static const PRUint32 kPageSize = 1024;//4096;
nsMemStream::nsMemStream():m_AllocSize(0),m_Size(0),m_pStart(0),m_ReadOffset(0),m_WriteOffset(0)
{
}

nsMemStream::~nsMemStream()
{
    PR_FREEIF(m_pStart);
}
/*
nsrefcnt nsMemStream::AddRef(void)
{
    return ++m_RefCnt;
}
nsrefcnt nsMemStream::Release(void)
{
    if (--m_RefCnt == 0)
    {
        delete this;
        return 0;
    }
    return m_RefCnt;
}

nsresult nsMemStream::QueryInterface(const nsIID& aIID,
                                        void** aInstancePtrResult)
{
    return NS_OK;
}
*/

PRInt32 nsMemStream::Read(void* o_Buffer, PRUint32 i_Len)
{
    if (m_Size > 0)
    {
        PR_ASSERT(m_pStart); //This has to be there if m_Size > 0

        char* pCurrentRead = (char*) m_pStart + m_ReadOffset;

        unsigned int validLen = m_Size - m_ReadOffset;
    
        if (0 == validLen)
            return 0;

        if (validLen > i_Len)
            validLen = i_Len;

        memcpy(o_Buffer, pCurrentRead, validLen);
        m_ReadOffset += validLen;
        return validLen;
    }
    return 0;
}

void nsMemStream::Reset()
{
    m_ReadOffset = 0;
}

PRInt32 nsMemStream::Write(const void* i_Buffer, PRUint32 i_Len)
{
    if (!m_pStart)
    {
        m_pStart = PR_Calloc(1, kPageSize); 
        if (!m_pStart)
        {
            PR_Free(m_pStart);
            return 0;
        }
        m_WriteOffset = 0;
        m_AllocSize = kPageSize;
    }
    unsigned int validLen = m_AllocSize - m_Size;
    while (validLen < i_Len)
    {
        //Alloc some more
        m_pStart = PR_Realloc(m_pStart, m_AllocSize+kPageSize);
        if (!m_pStart)
        {
            PR_Free(m_pStart);
            m_AllocSize = 0;
            m_WriteOffset = 0;
            m_Size = 0;
            return 0;
        }
        m_AllocSize += kPageSize;
        validLen += kPageSize;
    }
    char* pCurrentWrite = (char*)m_pStart + m_WriteOffset;
    memcpy(pCurrentWrite, i_Buffer, i_Len);
    m_WriteOffset += i_Len;
    m_Size += i_Len;
    return i_Len;
}