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

/*
 * Updated to use mozilla/base nsFileStream class - spence 02/10/99
 */

#include "nsCacheFileStream.h"
#include "plstr.h"
#include "prlog.h"
#ifdef XP_UNIX
#include "prio.h"
#include "sys/param.h"
#include "prsystem.h"
#endif

nsCacheFileStream::nsCacheFileStream(const char* i_Filename):m_pFile(0),m_pFilename(0)
{
    PR_ASSERT(i_Filename);
    if (i_Filename)
    {
        m_pFilename = new char[PL_strlen(i_Filename)+1];
        PL_strcpy(m_pFilename, i_Filename);
        Open();
    }
}

nsCacheFileStream::~nsCacheFileStream()
{
    if (m_pFilename)
    {
        delete[] m_pFilename;
        m_pFilename = 0;
    }
    if (m_pFile)
        m_pFile->close();
}

/*
nsrefcnt nsCacheFileStream::AddRef(void)
{
    return ++m_RefCnt;
}
nsrefcnt nsCacheFileStream::Release(void)
{
    if (--m_RefCnt == 0)
    {
        delete this;
        return 0;
    }
    return m_RefCnt;
}

nsresult nsCacheFileStream::QueryInterface(const nsIID& aIID,
                                        void** aInstancePtrResult)
{
    return NS_OK;
}
*/

PRBool nsCacheFileStream::Open()
{
    if (m_pFile)
        m_pFile->close();
    PR_ASSERT(m_pFilename);
    if (m_pFilename)
    {
	/* does 'browser.cache.directory/<subdir>' exist? */
	char *cacheDir = new char [MAXPATHLEN];
	PRFileInfo dir;

	int slash = PL_strrchr (m_pFilename, PR_GetDirectorySeparator())
							- m_pFilename + 1;
	PL_strncpyz (cacheDir, m_pFilename, slash);

	PRStatus status = PR_GetFileInfo (cacheDir, &dir);
	if (PR_FAILURE == status) {
	    status = PR_MkDir(cacheDir, 0600);
	    if (PR_SUCCESS != status) {
		return PR_FALSE;
	    }
	}

	nsFilePath ioPath (m_pFilename);
	m_pFile = new nsIOFileStream (ioPath, PR_CREATE_FILE | PR_RDWR, 0600);

        PR_ASSERT(m_pFile);
        if (m_pFile)
            return PR_TRUE;
    }
    return PR_FALSE;
}

PRInt32 nsCacheFileStream::Read(void* o_Buffer, PRUint32 i_Len)
{
    if (!m_pFile && !Open())
        return 0;
    return m_pFile->read((void *)o_Buffer, i_Len);
}

void nsCacheFileStream::Reset()
{
    Open();
}

PRInt32 nsCacheFileStream::Write(const void* i_Buffer, PRUint32 i_Len)
{
    if (!m_pFile && !Open())
        return 0;
    return m_pFile->write((void *)i_Buffer, i_Len);
}

