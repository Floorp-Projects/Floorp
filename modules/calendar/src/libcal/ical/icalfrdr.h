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
 * icalfrdr.h
 * John Sun
 * 2/10/98 11:37:48 PM
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistring.h>

#ifndef __ICALFILEREADER_H_
#define __ICALFILEREADER_H_

/**
 * ICalFileReader is a subclass of ICalReader.  It implements the
 * ICalReader interface to work with files.
 * TODO: handle QuotedPrintable
 */
class JULIAN_PUBLIC ICalFileReader
{
private:

    /** filename of file */
    const char * m_filename;
    
    /** ptr to file */
    FILE * m_file;

    /** buffer to contain current line, assumed to be less than 1024 bytes */
    char m_pBuffer[1024];

private:

    /**
     * Default constructor.  Made private to hide from clients.
     */
    ICalFileReader();

public:

    /**
     * Constructor.  Clients should call this to create ICalFileReader 
     * objects to read ICAL objects from a file.
     * Opens file for reading.
     *
     * @param           filename    filename of target file
     * @param           status      set to 1 if can't open file
     */
    ICalFileReader(char * filename, ErrorCode & status);

    /**
     * Destructor.  Also closes file.
     */
    ~ICalFileReader();
    
    /**
     * Read next character from file.
     *
     * @param           status, return 1 if no more characters
     * @return          next character of string
     */
    virtual t_int8 read(ErrorCode & status);

    /**
     * Read next line of file.  A line is defined to be
     * characters terminated by either a '\n', '\r' or "\r\n".
     * @param           aLine, return next line of string
     * @param           status, return 1 if no more lines
     *
     * @return          next line of string
     */
    virtual UnicodeString & readLine(UnicodeString & aLine, ErrorCode & status);
    
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
};

#endif /* __ICALFILEREADER_H_ */
