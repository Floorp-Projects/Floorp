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

nsFileStream::nsFileStream(XP_File* i_pFile):m_pFile(i_pFile)
{
    PR_ASSERT(m_pFile);
}

nsFileStream::~nsFileStream()
{
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

PRInt32 nsFileStream::Read(void* o_Buffer, PRUint32 i_Len)
{
    if (m_pFile)
    {
        int status = XP_FileRead(o_Buffer, i_Len, *m_pFile);
        return status;
    }
    return 0;
}

PRInt32 nsFileStream::Write(const void* i_Buffer, PRUint32 i_Len)
{
    if (m_pFile)
    {
        int status = XP_FileWrite(i_Buffer, i_Len, *m_pFile);
        return status;
    }
    return 0;
}