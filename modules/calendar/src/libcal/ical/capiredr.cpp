/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
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

/* -*- Mode: C++; tab-width: 4; tabs-indent-mode: nil -*- */
/* 
 * capiredr.cpp
 * John Sun
 * 4/16/98 3:42:19 PM
 */

#include "stdafx.h"
#include "capiredr.h"
#include "nspr.h"
//#include "xp_mcom.h"

const t_int32 ICalCAPIReader::m_MAXBUFFERSIZE = 1024;
const t_int32 ICalCAPIReader::m_NOMORECHUNKS = 404;
//---------------------------------------------------------------------

ICalCAPIReader::ICalCAPIReader()
{
    PR_ASSERT(FALSE);
}

//---------------------------------------------------------------------

void
ICalCAPIReader::AddChunk(UnicodeString * u)
{
    if (m_Chunks == 0)
        m_Chunks = new JulianPtrArray();
    PR_ASSERT(m_Chunks != 0);
    if (m_Chunks != 0)
    {
        m_Chunks->Add(u);
    }
}

//---------------------------------------------------------------------

ICalCAPIReader::ICalCAPIReader(PRMonitor * monitor,
                               JulianUtility::MimeEncoding encoding)
{ 
    m_Monitor = monitor;
    m_bFinished = FALSE;
    
    m_ChunkIndex = 0;
    m_Chunks = 0;
    m_Init = TRUE;
    m_Pos = 0;
    m_Mark = 0;
    m_ChunkMark = 0;
    m_Encoding = encoding;
}

//---------------------------------------------------------------------

void ICalCAPIReader::deleteUnicodeStringVector(JulianPtrArray * stringVector)
{
    t_int32 i;
    if (stringVector != 0) 
    {
        for (i = stringVector->GetSize() - 1; i >= 0; i--)
        {
            delete ((UnicodeString *) stringVector->GetAt(i));
        }
    }
}

//---------------------------------------------------------------------

ICalCAPIReader::~ICalCAPIReader()
{
    if (m_Chunks != 0)
    {
        deleteUnicodeStringVector(m_Chunks);
        delete m_Chunks; m_Chunks = 0;
    }
    //if (m_Buffer != 0) { delete [] m_Buffer; m_Buffer = 0; }
    //PR_Exit();
}

//---------------------------------------------------------------------
/*
void ICalCAPIReader::setBuffer(const char * capiChunk)
{
    if (m_Init)
    {
        strcpy(m_Buffer, capiChunk);
        m_BufferSize = strlen(m_Buffer);
        m_Init = FALSE;
    }
    else
    {
        strcat(m_Buffer + m_BufferSize, capiChunk);
        m_BufferSize += strlen(capiChunk);
    }
}
*/
//---------------------------------------------------------------------

t_int8 ICalCAPIReader::read(ErrorCode & status)
{
    // TODO:
   
    t_int32 i = 0;
    
    while (TRUE)
    {
        if (m_Chunks == 0 || m_Chunks->GetSize() == 0 || 
            m_ChunkIndex >= m_Chunks->GetSize())
        {
            status = m_NOMORECHUNKS; // no more chunks, should block
            return -1;
            // block?
        }
        else
        {
            // read from linked list of UnicodeString's
            // delete front string when finished reading from it
        
            UnicodeString string = *((UnicodeString *) m_Chunks->GetAt(m_ChunkIndex));
            if (m_Pos < string.size())
            {  
                // return index of this 
                status = ZERO_ERROR;

                if (m_Encoding == JulianUtility::MimeEncoding_QuotedPrintable)
                {
                    if ((string)[(TextOffset) m_Pos] == '=')
                    {
                        if (string.size() >= m_Pos + 3)
                        {
                            if (ICalReader::isHex(string[(TextOffset)(m_Pos + 1)]) && 
                                ICalReader::isHex(string[(TextOffset)(m_Pos + 2)]))
                            {
                                t_int8 c;
                                c = ICalReader::convertHex(string[(TextOffset) (m_Pos + 1)], 
                                    string[(TextOffset) (m_Pos + 2)]);
                                // replace the contents of string
                                //string/removeBetween(m_Pos, 3);
                                //char sBuf[1];
                                //sBuf[0] = c;
                                //string->insert(m_Pos, sBuf);
                                m_Pos += 3;
                                //m_Pos++;
                                return c;
                            }
                            else
                            {
                                return (t_int8) (string)[(TextOffset) m_Pos++];
                            }
                        }
                        else
                        {
                            t_int32 lenDiff = string.size() - m_Pos;
                            char fToken;
                            char sToken;
                            t_bool bSetFToken = FALSE;
                            t_int32 tempIndex = m_ChunkIndex;
                            UnicodeString token;
                            
                            while (TRUE)
                            {

                                // lenDiff = 1, 2 always
                                // the =XX spans different chunks
                                // if last chunk, return out of chunks status
                                if (tempIndex == m_Chunks->GetSize() - 1)
                                {
                                    status = m_NOMORECHUNKS;
                                    return -1;
                                }
                                else 
                                {
                                    UnicodeString nextstring = *((UnicodeString *) m_Chunks->GetAt(tempIndex + 1));
                                    tempIndex++;

                                    if (nextstring.size() >= 2)
                                    {
                                        t_int8 c;
                                        if (lenDiff == 2)
                                        {
                                            
                                            fToken = string[(TextOffset) (string.size() - 1)];
                                            sToken = nextstring[(TextOffset) 0];                                            

                                            if (ICalReader::isHex(fToken) && ICalReader::isHex(sToken))
                                            {
                                                c = ICalReader::convertHex(fToken, sToken);
                                                
                                                m_Pos = 1;
                                                m_ChunkIndex = tempIndex;
                                                bSetFToken = FALSE;
                                                return c;
                                            }
                                            else
                                            {
                                                return (t_int8) (string)[(TextOffset) m_Pos++];
                                            }
                                        }
                                        else
                                        {
                                            // lenDiff = 1

                                            if (bSetFToken)
                                            {
                                                sToken = nextstring[(TextOffset) 0];
                                            }
                                            else 
                                            {
                                                fToken = nextstring[(TextOffset) 0];
                                                sToken = nextstring[(TextOffset) 1];
                                            }

                                            if (ICalReader::isHex(fToken) && ICalReader::isHex(sToken))
                                            {
                                                c = ICalReader::convertHex(fToken, sToken); 

                                                m_Pos = 2;
                                                m_ChunkIndex = tempIndex;
                                                bSetFToken = FALSE;
                                                return c;
                                            }
                                            else
                                            {
                                                return (t_int8) (string)[(TextOffset) m_Pos++];
                                            }
                                        }
                                    }
                                    else
                                    {
                                        if (nextstring.size() > 0)
                                        {
                                            if (!bSetFToken)
                                            {
                                                fToken = nextstring[(TextOffset) 0];
                                                bSetFToken = TRUE;

                                            }
                                            else
                                            {
                                                sToken = nextstring[(TextOffset) 0];
                                            
                                                if (ICalReader::isHex(fToken) && ICalReader::isHex(sToken))
                                                {
                                                    char c;
                                                    c = ICalReader::convertHex(fToken, sToken); 
    
                                                    m_Pos = 1;
                                                    m_ChunkIndex = tempIndex;
                                                    bSetFToken = FALSE;
                                                    return c;
                                                }
                                                else
                                                {
                                                    return (t_int8) (string)[(TextOffset) m_Pos++];
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                        }
                    }
                    else 
                    {
                        return (t_int8) (string)[(TextOffset) m_Pos++];
                    }

                }
                else
                {
                    return (t_int8) (string)[(TextOffset) m_Pos++];
                }
            }
            else
            {
                // delete front string from list, try reading from next chunk
                m_Pos = 0;
                m_ChunkIndex++;

                //delete ((UnicodeString *) m_Chunks->GetAt(i));
                //m_Chunks->RemoveAt(i);
            }
        }
    }
    status = 1;
    return -1;
    /*
    if (m_BufferSize > 0)
    {
          if (m_Pos >= m_BufferSize)
          {
              status = 1;
              return -1;
          }
          else
          {
              status = ZERO_ERROR;
              return m_Buffer[m_Pos++];
          }
    }
    else
    {
        status = 2;
        return -1;
    }
    */
}

//---------------------------------------------------------------------
UnicodeString &
ICalCAPIReader::createLine(t_int32 oldPos, t_int32 oldChunkIndex,
                           t_int32 newPos, t_int32 newChunkIndex,
                           UnicodeString & aLine)
{
    PR_ASSERT(oldChunkIndex <= newChunkIndex);
    UnicodeString u;
    if (oldChunkIndex == newChunkIndex)
    {
        u = *((UnicodeString *) m_Chunks->GetAt(oldChunkIndex));
        u.extractBetween(oldPos, newPos, aLine);
        return aLine;
    }
    else {
        //(oldChunkIndex < newChunkIndex)
        
        t_int32 i;
        UnicodeString v, temp;
        u = *((UnicodeString *) m_Chunks->GetAt(oldChunkIndex));
        u.extractBetween(oldPos, u.size(), aLine);
        v = *((UnicodeString *) m_Chunks->GetAt(newChunkIndex));
        v.extractBetween(0, newPos, temp);
        i = oldChunkIndex + 1;
        while (i < newChunkIndex)
        {
            v = *((UnicodeString *) m_Chunks->GetAt(i));
            aLine += v;
            i++;
        }
        aLine += temp;
        return aLine;
    }
}

//---------------------------------------------------------------------

UnicodeString & 
ICalCAPIReader::readFullLine(UnicodeString & aLine, ErrorCode & status, t_int32 iTemp)
{
    status = ZERO_ERROR;
    t_int32 i;

#if TESTING_ITIPRIG
    TRACE("ICalCAPIReader: waiting to enter monitor\r\n");
#endif

    PR_EnterMonitor(m_Monitor);

#if TESTING_ITIPRIG
    TRACE("ICalCAPIReader: in monitor\r\n");
#endif
    
    t_int32 oldpos = m_Pos;
    t_int32 oldChunkIndex = m_ChunkIndex;

    readLine(aLine, status);
    if (status == m_NOMORECHUNKS)
    {
        // maybe block on monitor here
        //PR_Notify(m_Monitor);
#if CAPI_READY
        //PR_Yield();
        if (m_bFinished)
        {
            // do nothing.
        }
        else 
        {
#if TESTING_ITIPRIG
            TRACE("ICalCAPIReader: (1) waiting for writeTOCAPIReader to add more chunks\r\n");
#endif
            PR_Wait(m_Monitor, LL_MAXINT);
            //PR_Yield();
        }
#else
        return aLine;
#endif
    }
    UnicodeString aSubLine;
    while (TRUE)
    {
        mark();
        i = read(status);
        if (status == m_NOMORECHUNKS)
        {
            // block()? reset()?;
            //PR_Notify(m_Monitor);
            //PR_ExitMonitor(m_Monitor);
#if CAPI_READY
            if (m_bFinished)
            {
                // do nothing
                break;
            }
            else
            {
#if TESTING_ITIPRIG
                TRACE("ICalCAPIReader: (2) waiting for writeTOCAPIReader to add more chunks\r\n");
#endif
                PR_Wait(m_Monitor, LL_MAXINT);
                //PR_Yield();
            }
#else
            return aLine;
#endif
        }
        else if (i == ' ')
        {
#if TESTING_ITIPRIG
            TRACE("ICalCAPIReader: recursivecall\r\n");
#endif
            aLine += readLine(aSubLine, status);
            //PR_Notify(m_Monitor);
            //if (FALSE) TRACE("rfl(2) %s\r\n", aLine.toCString(""));
        }
        else
        {
#if TESTING_ITIPRIG
            TRACE("ICalCAPIReader: reset() break out of loop\r\n");
#endif
            reset();
            break;
        }
    }
#if TESTING_ITIPRIG
    if (TRUE) TRACE("readFullLine returned: ---%s---\r\n", aLine.toCString(""));
#endif
#if TESTING_ITIPRIG
    TRACE("ICalCAPIReader: leaving monitor\r\n");
#endif
    PR_ExitMonitor(m_Monitor);

    return aLine;
}

//---------------------------------------------------------------------

UnicodeString & 
ICalCAPIReader::readLine(UnicodeString & aLine, ErrorCode & status)
{
    status = ZERO_ERROR;
    t_int8 c = 0;
    t_int32 oldPos = m_Pos;
    t_int32 oldChunkIndex = m_ChunkIndex;

    aLine = "";

    //PR_EnterMonitor(m_Monitor);

    c = read(status);
    while (!(FAILURE(status)))
    {
        if (status == m_NOMORECHUNKS)
        {
            // block
            break;
        }
        /* Break on '\n', '\r\n', and '\r' */
        else if (c == '\n')
        {
            break;
        }
        else if (c == '\r')
        {
            mark();
            c = read(status);
            if (FAILURE(status))
            {
                // block(), reset()?
                break;
            }
            else if (c == '\n')
            {
                break;
            }
            else
            {
                reset();
                break;
            }
        }
#if 1
        aLine += c;
        //if (FALSE) TRACE("aLine = %s: -%c,%d-\r\n", aLine.toCString(""), c, c);
#endif
        c = read(status);

    }
#if 0
    createLine(oldPos, oldChunkIndex, m_Pos, m_ChunkIndex, aLine);
#endif

    /*
    if (FAILURE(status))
    {
        PR_Notify(m_Monitor);
        PR_ExitMonitor(m_Monitor);
    }
    */

    //if (FALSE) TRACE("\treadLine returned %s\r\n", aLine.toCString(""));
    return aLine;

    /*
    t_int32 lineSize = m_Pos - oldpos;
    char * theLine = new char[lineSize];
    PR_ASSERT(theLine != 0);
    strncpy(theLine, m_Buffer + oldpos, lineSize);
    aLine = theLine;
    return aLine;
    */
}

//---------------------------------------------------------------------

