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
 * capiredr.h
 * John Sun
 * 4/16/98 3:31:51 PM
 */
#ifndef __ICALCAPIREADER_H_
#define __ICALCAPIREADER_H_

#include "icalredr.h"
#include "ptrarray.h"
/*#include "xp_mcom.h"*/
#include "prmon.h"
#include "jutility.h"
/**
 *  ICalCAPIReader is a subclass of ICalReader.  It implements
 *  the ICalReader interface to work with CAPI callback method
 *  to parse from stream.  
 *  Uses multiple threads.
 */
class ICalCAPIReader
{
private:
    /*-----------------------------
    ** MEMBERS
    **---------------------------*/
    /* current buffer of iCal information,
     * when CAPI has information to return to
     * the buffer, it must append to this buffer 
     * when no more full-lines can be made from the
     * buffer, block on the monitor.  Will be
     * notified when CAPI gets back more information
     */
    char * m_Buffer;
    t_int32 m_BufferSize;
    t_bool m_Init;
    t_int32 m_Mark;
    t_int32 m_ChunkMark;
    t_int32 m_Pos;
    t_int32 m_ChunkIndex;

    /** encoding of stream */
    JulianUtility::MimeEncoding m_Encoding;

    JulianPtrArray * m_Chunks;

    static const t_int32 m_MAXBUFFERSIZE;
    static const t_int32 m_NOMORECHUNKS;

    /* finished getting input from CAPI callback */
    t_bool m_bFinished;

    PRMonitor * m_Monitor;
    /*-----------------------------
    ** PRIVATE METHODS
    **---------------------------*/
    
    UnicodeString & createLine(t_int32 oldPos, t_int32 oldChunkIndex,
        t_int32 newPos, t_int32 newChunkIndex, UnicodeString & aLine);

    static void deleteUnicodeStringVector(JulianPtrArray * stringVector);

    ICalCAPIReader();
public:
    /*-----------------------------
    ** CONSTRUCTORS and DESTRUCTORS
    **---------------------------*/
    ICalCAPIReader(PRMonitor * monitor,
        JulianUtility::MimeEncoding encoding = JulianUtility::MimeEncoding_7bit);
    ~ICalCAPIReader();

    /*----------------------------- 
    ** ACCESSORS (GET AND SET) 
    **---------------------------*/

    virtual void * getMonitor() { return m_Monitor; }

    void setFinished() { m_bFinished = TRUE; }
    void setEncoding(JulianUtility::MimeEncoding encoding) { m_Encoding = encoding; }
    t_bool isFinished() const { return m_bFinished; }
    /**
     * Sets a the buffer to read from.  
     * Appends the m_Buffer.  
     */
    //virtual void setBuffer(const char * capiChunk);
    

    /**
     * Don't delete u until this object is deleted.
     * @param           UnicodeString * u
     *
     * @return          void 
     */
    void AddChunk(UnicodeString * u);

    /*----------------------------- 
    ** UTILITIES 
    **---------------------------*/ 

    void mark() { m_Mark = m_Pos; m_ChunkMark = m_ChunkIndex;}

    void reset() { m_Pos = m_Mark; m_ChunkIndex = m_ChunkMark;}

    /**
     * Read next character from file.
     *
     * @param           status, return 1 if no more characters
     * @return          next character of string
     */
    virtual t_int8 read(ErrorCode & status);

    
    
    /**
     * Read the next ICAL full line of the file.  The definition
     * of a full ICAL line can be found in the ICAL spec.
     * Basically, a line followed by a CRLF and a space character
     * signifies that the next line is a continuation of the previous line.
     * Uses the readLine, read methods.
     *
     * @param           aLine, returns next full line of string
     * @param           status, return 1 if no more lines
     *
     * @return          next full line of string
     */
    virtual UnicodeString & readFullLine(UnicodeString & aLine, ErrorCode & status, t_int32 i = 0);
    
    /**
     * Read next line of file.  A line is defined to be
     * characters terminated by either a '\n', '\r' or "\r\n".
     * @param           aLine, return next line of string
     * @param           status, return 1 if no more lines
     *
     * @return          next line of string
     */
    virtual UnicodeString & readLine(UnicodeString & aLine, ErrorCode & status);
public:

    //virtual UnicodeString & readLineZero(UnicodeString & aLine, ErrorCode & status);

    

};

#endif /* __ICALCAPIREADER_H_ */

