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

#include "nsTOC.h"
#include <plstr.h>
#include <memory.h>
#include <prlog.h>

static const PRUint32 kTOC_FLAT_FILE_VERSION = 1;
static const PRUint32 kTOC_MAGIC_NUMBER = 19712809;
static const PRUint32 kTOC_HEADER_SIZE = 2* sizeof(PRUint32);

//TODO change this to a memory mapped file!
nsTOC::nsTOC(const char* i_pFilename, nsFlatFile* i_pFlatFile):
    m_pFilename( new char[PL_strlen(i_pFilename) + 1]),
    m_pFD(0),
    m_bIsValid(PR_FALSE),
    m_pFlatFile(i_pFlatFile),
    m_Entries(1)
{
    PL_strcpy(m_pFilename, i_pFilename);
    Init();
}

nsTOC::~nsTOC()
{
    delete[] m_pFilename;
    if (m_pFD)
    {
        PR_Close(m_pFD);
        m_pFD = 0;
    }
}

/*
nsrefcnt nsTOC::AddRef(void)
{
    return ++m_RefCnt;
}
nsrefcnt nsTOC::Release(void)
{
    if (--m_RefCnt == 0)
    {
        delete this;
        return 0;
    }
    return m_RefCnt;
}

nsresult nsTOC::QueryInterface(const nsIID& aIID,
                                        void** aInstancePtrResult)
{
    return NS_OK;
}

*/

void nsTOC::Init(void)
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
                char buf[kTOC_HEADER_SIZE];
                //verify
                status = PR_Read(m_pFD, &buf, kTOC_HEADER_SIZE);
                if (status > 0)
                {
                    char* cur_ptr = &buf[0];
                    PRUint32 test;
                    COPY_INT32(&test, cur_ptr);
                    if (test != kTOC_MAGIC_NUMBER)
                    {
                        PR_Close(m_pFD);
                        m_pFD = 0;
                        //PR_ASSERT(!"Bad TOC file!");
                        return;
                    }
                    cur_ptr += sizeof(PRUint32);
                    COPY_INT32(&test, cur_ptr);
                    if (test != kTOC_FLAT_FILE_VERSION)
                    {
                        PR_Close(m_pFD);
                        m_pFD = 0;
                        //PR_ASSERT(!"Bad version of TOC file!");
                        return;
                    }

                    // Number of entries in the TOC
                    status = PR_Read(m_pFD, &buf, sizeof(PRUint32));
                    if (status > 0)
                    {
                        cur_ptr = &buf[0];
                        COPY_INT32(&m_Entries, cur_ptr);
                    }
                    //Read in the list of entries 
                    nsFFEntry* pCurrent;
                    Serialize(pCurrent, PR_TRUE);
                    //There has got to be a free entry.
                    PR_ASSERT(pCurrent /*&& pCurrent->IsValid() TODO*/);
                    m_pContents = pCurrent;
                    PRUint32 count = 1; //Definitely one!
                    Serialize(pCurrent, PR_TRUE);
                    while (pCurrent)
                    {
                        count++;
                        m_pContents->AddEntry(pCurrent);
                        Serialize(pCurrent, PR_TRUE);
                    }

                    PR_ASSERT(count == m_Entries);

                    //Everything was as expected so
                    m_bIsValid = PR_TRUE;
                }
            }
            else
            {
                //write out
                char* buf = new char[kTOC_HEADER_SIZE]; 
                char* cur_ptr = buf;
                COPY_INT32((void*) cur_ptr, &kTOC_MAGIC_NUMBER);
                cur_ptr += sizeof(PRUint32);
                COPY_INT32((void*) cur_ptr, &kTOC_FLAT_FILE_VERSION);
                status = PR_Write(m_pFD, buf, kTOC_HEADER_SIZE);

                // Number of entries in the TOC
                COPY_INT32((void*)buf, &m_Entries);
                status = PR_Write(m_pFD, buf, sizeof(PRUint32));

                delete[] buf;

                //Since the TOC is being created afresh, reset the flat file with everything marked as 
                //free. 
                m_pContents = new nsFFEntry(
                                    0,
                                    m_pFlatFile->HeaderSize(), 
                                    m_pFlatFile->Size()-m_pFlatFile->HeaderSize());
                //Serialize the only entry-
                Serialize(m_pContents);

                m_bIsValid = PR_TRUE;
            }
        }
    }
}

nsFFEntry* nsTOC::AddEntry(PRUint32 i_Size)
{
    if (i_Size > FreeEntry()->Size())
    {
        //Garbage collect
    }
    nsFFEntry* pEntry = new nsFFEntry(
            NextID(), 
            FreeEntry()->FirstObject()->Offset(), 
            i_Size);

    return pEntry;
}

PRBool nsTOC::Serialize(nsFFEntry* io_Entry, PRBool bRead )
{
    PR_ASSERT(m_pFD);
    if (bRead)
    {
        io_Entry = new nsFFEntry();
        if (!io_Entry)
            return PR_FALSE;
        char* buf = new char[sizeof(PRUint32)];
        PRUint32 temp, objects, size;
        //Read ID
        PR_Read(m_pFD, buf, sizeof(PRUint32));
        COPY_INT32(&temp, buf);
        io_Entry->ID(temp);
        //Read number of objects
        PR_Read(m_pFD, buf, sizeof(PRUint32));
        COPY_INT32(&objects, buf);
        //Read objects
        for (int i= objects; i>0; --i)
        {
            PR_Read(m_pFD, buf, sizeof(PRUint32));
            COPY_INT32(&temp, buf);
            PR_Read(m_pFD, buf, sizeof(PRUint32));
            COPY_INT32(&size, buf);
            io_Entry->AddObject(new nsFFObject(io_Entry->ID(), temp, size));
        }
        delete[] buf;
        PR_ASSERT(io_Entry->Objects() == objects);
        if (io_Entry->Objects() != objects)
            return PR_FALSE;
        return PR_TRUE;
    }
    else
    {
        PR_ASSERT(io_Entry);
        if (!io_Entry)
            return PR_FALSE;
        
        //TODO speed this up by increasing the buffer size to write out
        //more than one int at a time!

        char* buf = new char[sizeof(PRUint32)];
        PRUint32 temp;
        //Write ID
        temp = io_Entry->ID();
        COPY_INT32((void*) buf, &temp);
        PR_Write(m_pFD, buf, sizeof(PRUint32));
        //Write number of Objects
        temp = io_Entry->Objects();
        COPY_INT32((void*) buf, &temp);
        PR_Write(m_pFD, buf, sizeof(PRUint32));
        //Write Objects
        if (temp > 0)
        {
            nsFFObject* current = io_Entry->FirstObject();
            while (current)
            {
                PR_ASSERT(current->ID() == io_Entry->ID());
                //Offset
                temp = current->Offset();
                COPY_INT32((void*) buf, &temp);
                PR_Write(m_pFD, buf, sizeof(PRUint32));
                //size
                temp = current->Size();
                COPY_INT32((void*) buf, &temp);
                PR_Write(m_pFD, buf, sizeof(PRUint32));

                current = current->Next();
            }
        }
        delete[] buf;
        return PR_TRUE;
    }
    return PR_FALSE;
}