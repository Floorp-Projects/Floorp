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
 * unistrto.h
 * John Sun
 * 2/3/98 12:07:00 PM
 */

#ifndef __UNICODESTRINGTOKENIZER_H_
#define __UNICODESTRINGTOKENIZER_H_

#include <unistring.h>
#include "ptypes.h"

/**
 * A simple string tokenizer class for UnicodeString.
 * It is modeled after the java.util.StringTokenizer class
 * and uses a similar subset of Java's API.
 */
class UnicodeStringTokenizer
{
private:

    /** current position in string */
    t_int32 m_CurrentPosition;

    /** maximum position in string */
    t_int32 m_MaxPosition;

    /** the string to tokenize */
    UnicodeString m_String;

    /** the string delimeters for seperating tokens */
    UnicodeString m_StringDelimeters;

    /** Skips delimeters. */
    void skipDelimeters();

public:

    /**
     * Constructor.  
     * @param           str     string to tokenize
     * @param           delim   string delimeters
     */
    UnicodeStringTokenizer(UnicodeString & str, UnicodeString & delim);

    /**
     * Constructor.  Sets default delimeters to whitespace characters.
     * Strongly recommend using other constructor.
     * @param           str     string to tokenize
     */
    UnicodeStringTokenizer(UnicodeString & str);

    /**
     * Tests if there are more tokens available from this string
     *
     * @return          TRUE if more tokens, FALSE otherwise
     */
    t_bool hasMoreTokens();


    /**
     * Returns next token in out if available.  If no more tokens
     * status is set to 1.
     * @param           out         next available token output
     * @param           status      status error, 1 is ran-out-of-tokens, 0 is OK
     *
     * @return          next available token (out)
     */
    UnicodeString & nextToken(UnicodeString & out, ErrorCode & status);
    
    /**
     * Returns next token in out if available.  If no more tokens
     * status is set to 1.  Also sets delimeters to delim for further tokenizing.
     * @param           out         next available token output
     * @param           delim       the new delimeters
     * @param           status      status error, 1 is ran-out-of-tokens, 0 is OK
     *
     * @return          next available token (out)
     */
    UnicodeString & nextToken(UnicodeString & out, UnicodeString & sDelim, ErrorCode & status);
};

#endif /* __UNICODESTRINGTOKENIZER_H_ */


