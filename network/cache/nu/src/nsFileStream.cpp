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

#include "nsFileStream.h"
#include "plstr.h"
#include "prlog.h"

nsFileStream::nsFileStream(const char* i_Filename):m_pFile(0),m_pFilename(0)
{
    PR_ASSERT(i_Filename);
    if (i_Filename)
    {
        m_pFilename = new char[PL_strlen(i_Filename)+1];
        PL_strcpy(m_pFilename, i_Filename);
        Open();
    }
}

nsFileStream::~nsFileStream()
{
    if (m_pFilename)
    {
        delete[] m_pFilename;
        m_pFilename = 0;
    }
    if (m_pFile)
        PR_Close(m_pFile);
}

/*
nsrefcnt nsFileStream::AddRef(void)
{
    return ++m_RefCnt;
}
nsrefcnt nsFileStream::Release(void)
{
    if (--m_RefCnt == 0)
    {
        delete this;
        return 0;
    }
    return m_RefCnt;
}

nsresult nsFileStream::QueryInterface(const nsIID& aIID,
                                        void** aInstancePtrResult)
{
    return NS_OK;
}
*/

PRBool nsFileStream::Open()
{
    if (m_pFile)
        PR_Close(m_pFile);
    PR_ASSERT(m_pFilename);
    if (m_pFilename)
    {
        m_pFile = PR_Open(
                m_pFilename, 
                PR_CREATE_FILE | PR_RDWR,
                600);// Read and write by owner only

        PR_ASSERT(m_pFile);
        if (m_pFile)
            return PR_TRUE;
    }
    return PR_FALSE;
}

PRInt32 nsFileStream::Read(void* o_Buffer, PRUint32 i_Len)
{
    if (!m_pFile && !Open())
        return 0;
    return PR_Read(m_pFile, o_Buffer, i_Len);
}

void nsFileStream::Reset()
{
    Open();
}

PRInt32 nsFileStream::Write(const void* i_Buffer, PRUint32 i_Len)
{
    if (!m_pFile && !Open())
        return 0;
    return PR_Write(m_pFile, i_Buffer, i_Len);
}