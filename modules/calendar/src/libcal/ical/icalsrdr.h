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
 * icalsrdr.h
 * John Sun
 * 2/10/98 11:37:39 AM
 */

#include <unistring.h>
#include "icalredr.h"
#include "jutility.h"

#ifndef __ICALSTRINGREADER_H_
#define __ICALSTRINGREADER_H_

/**
 * ICalStringReader is a subclass of ICalReader.  It implements the
 * ICalReader interface to work with strings (const char *).
 * DO NOT delete the char * object for which the object is constructed with.
   TODO: remove Unistring dependency.  There is a bug if the
   target string is encoded with 2byte character.  If so, then the
   m_pos and m_length variables are wrong.  Currently, I will assume that
   all const char * passed in will be us-ascii 8-bit chars
*/
class JULIAN_PUBLIC ICalStringReader: public ICalReader
{
private:

    /** BUG:?? used to extract substring.  Dangerous if encoded */
    UnicodeString m_unistring;

    /** string: do not delete outside */
    const char * m_string;

    /** length of string */
    t_int32 m_length;
    
    /** current position in string */
    t_int32 m_pos;
    
    /** last marked position of string */
    t_int32 m_mark;

    /** encoding of stream */
    JulianUtility::MimeEncoding m_Encoding;

    /**
     * Default constructor.  Made private to hide from clients.
     */
    ICalStringReader();

    

public:
    
    /**
     * Creates object to read ICAL objects from a string.
     * @param           string  string to read from
     */
    ICalStringReader(const char * string, 
        JulianUtility::MimeEncoding encoding = JulianUtility::MimeEncoding_7bit);

    /**
     * Mark current position in string 
     */
    void mark();

    /**
     * Reset position to last mark
     */
    void reset();

    /**
     * Read next character of string.
     *
     * @param           status, return 1 if no more characters
     * @return          next character of string
     */
    virtual t_int8 read(ErrorCode & status);

    /**
     * Read next line of string.  A line is defined to be
     * characters terminated by either a '\n', '\r' or "\r\n".
     * @param           aLine, return next line of string
     * @param           status, return 1 if no more lines
     *
     * @return          next line of string
     */
    virtual UnicodeString & readLine(UnicodeString & aLine, ErrorCode & status);
    

    /**
     * Read the next ICAL full line of the string.  The definition
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
};

#endif /* __ICALSTRINGREADER_H_ */
