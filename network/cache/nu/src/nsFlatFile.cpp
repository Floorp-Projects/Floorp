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

#include "nsFlatFile.h"
#include <plstr.h>
#include <prmem.h>
#include <memory.h>

static const PRUint32 kFLAT_FILE_VERSION = 1;
static const PRUint32 kMAGIC_NUMBER = 3739142906; // Facede de //TODO
static const char kFILL_CHAR = 'G';

nsFlatFile::nsFlatFile(const char* i_pFilename, const PRUint32 i_Size): 
    m_bIsValid(PR_FALSE), 
    m_pFD(0),
    m_Size(i_Size),
    m_HeaderSize(2*sizeof(PRUint32)),
    m_pFilename(new char[PL_strlen(i_pFilename) + 1])

{
    PL_strcpy(m_pFilename, i_pFilename);
    Init();
}

nsFlatFile::~nsFlatFile()
{
    delete[] m_pFilename;
    if (m_pFD)
    {
        PR_Close(m_pFD);
        m_pFD = 0;
    }
}

void nsFlatFile::Init(void)
{
    if (m_pFilename)
    {
        m_pFD = PR_Open(m_pFilename, PR_CREATE_FILE | PR_RDWR, 0600);
        if (!m_pFD)
        {
            return;
        }
        PRFileInfo info;
        // if the file is just being created then write magic number 
        // and flat file version, else verify
        if (PR_SUCCESS == PR_GetOpenFileInfo(m_pFD, &info))
        {
            PRInt32 status;
            if (info.size > 0)
            {
                char* buf = new char[m_HeaderSize];
                //verify
                status = PR_Read(m_pFD, buf, m_HeaderSize);
                if (status > 0)
                {
                    char* cur_ptr = buf;
                    PRUint32 test;
                    COPY_INT32(&test, cur_ptr);
                    if (test != kMAGIC_NUMBER)
                    {
                        PR_Close(m_pFD);
                        m_pFD = 0;
                        //PR_ASSERT(!"Bad flat file!");
                        return;
                    }
                    cur_ptr += sizeof(PRUint32);
                    COPY_INT32(&test, cur_ptr);
                    if (test != kFLAT_FILE_VERSION)
                    {
                        PR_Close(m_pFD);
                        m_pFD = 0;
                        //PR_ASSERT(!"Bad version of flat file format!");
                        return;
                    }
                    //Everything was as expected so
                    m_bIsValid = PR_TRUE;
                }
                delete[] buf;
            }
            else
            {
                //write out
                char* buf = new char[m_HeaderSize]; 
                PRUint32 test = m_HeaderSize;
                char* cur_ptr = buf;
                COPY_INT32((void*) cur_ptr, &kMAGIC_NUMBER);
                cur_ptr += sizeof(PRUint32);
                COPY_INT32((void*) cur_ptr, &kFLAT_FILE_VERSION);
                status = PR_Write(m_pFD, buf, m_HeaderSize);
                delete[] buf;

                PRBool bFillChar = PR_FALSE;

                if (bFillChar)
                {
                    buf = new char[kPAGE_SIZE];
                    for (int i = 0; i< kPAGE_SIZE; ++i)
                        buf[i] = kFILL_CHAR;
                    int j = (int)((m_Size - m_HeaderSize) / kPAGE_SIZE);
                    int k;
                    for (k = j; k > 0; --k)
                    {
                        status = PR_Write(m_pFD, (void*) buf, kPAGE_SIZE);
                    }
                    delete[] buf;
                    int remaining = (m_Size-m_HeaderSize) % kPAGE_SIZE;
                    for (k = remaining; k>0; --k)
                    {
                        status = PR_Write(m_pFD, (void*) &kFILL_CHAR, 1);
                    }
                }
            }
        }
    }
}

/*
nsrefcnt nsFlatFile::AddRef(void)
{
    return ++m_RefCnt;
}
nsrefcnt nsFlatFile::Release(void)
{
    if (--m_RefCnt == 0)
    {
        delete this;
        return 0;
    }
    return m_RefCnt;
}

nsresult nsFlatFile::QueryInterface(const nsIID& aIID,
                                        void** aInstancePtrResult)
{
    return NS_OK;
}
*/
