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
 * uidrgntr.h
 * John Sun
 * 3/19/98 5:35:59 PM
 */

#ifndef __JULIANUIDRANDOMGENERATOR_H_
#define __JULIANUIDRANDOMGENERATOR_H_

#include <unistring.h>

/**
 *  Class that contains method to generate random UID strings.
 */
class JulianUIDRandomGenerator
{
public:
    /** default constructor.  It's of no use */
    JulianUIDRandomGenerator();

    /** destructor.  It's of no use */
    ~JulianUIDRandomGenerator();

    /**
     * generates random UID strings by appending 
     * current date time value is UTC ISO8601 format with
     * a random hexadecimal number.
     *
     * @return          new random UID string
     */
    static UnicodeString generate();

    /**
     * generates random UID strings by appending 
     * current date time value is UTC ISO8601 format with
     * a random hexadecimal number and input string.
     * Usually this input string would be a user@hostname or
     * e-mail address.
     *
     * @param           us  string to append to output
     *
     * @return          new random UID string
     */
    static UnicodeString generate(UnicodeString us);
};

#endif /*__JULIANUIDRANDOMGENERATOR_H_ */

