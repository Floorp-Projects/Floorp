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
 * uri.h
 * John Sun
 * 4/3/98 11:27:52 AM
 */
#ifndef __URI_H_
#define __URI_H_

#include <unistring.h>

/**
 *  URI encapsulates the ICAL URI data-type.  The URI data-type
 *  is used to identify values that contain a uniform resource
 *  identifier (URI) type of reference to the property value.
 */
class URI
{
private:
    /*-----------------------------
    ** MEMBERS
    **---------------------------*/

    /** URL string */
    UnicodeString m_URI;
public:

    /*-----------------------------
    ** CONSTRUCTORS and DESTRUCTORS
    **---------------------------*/

    /**
     * Default constructor makes string "".
     */
    URI();

    /** creates a URI with full URI */
    URI(UnicodeString fulluri);

    URI(char * fulluri);
    /*----------------------------- 
    ** ACCESSORS (GET AND SET) 
    **---------------------------*/ 

    /** 
     * returns full uri string 
     * (i.e. http://host1.com/my-report.txt, mailto:a@acme.com)
     *
     * @return          the full uri string
     */
    UnicodeString getFullURI() { return m_URI; }

    /** return protocol name (i.e. Mailto:, http:, ftp:) */
    /*UnicodeString getProtocol();*/

    /** 
     * return name (right side of ':' in full uri) 
     * (i.e. host1.com/my-report.txt, a@acme.com) 
     *
     * @return          the return name
     */
    UnicodeString getName();
    
    /** 
     * set full uri string 
     * @param           uri     new full URI string
     */
    void setFullURI(UnicodeString uri) { m_URI = uri; }

    /**
     * Checks whether string is a URI.  
     * For now just checks if a colon exists in string and
     * it does not start with a colon.
     * Thus: yoadfs:fdsaf is a valid URL.
     * @param           UnicodeString & s
     *
     * @return          static t_bool 
     */
    static t_bool IsValidURI(UnicodeString & s);
};

#endif /* __URI_H_ */


