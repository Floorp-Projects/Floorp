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

#ifndef _nsIURI_h_
#define _nsIURI_h_

#include "nsISupports.h"

/* 
    The nsIURI class is an interface to the URI behaviour for parsing
	portions out of a URI. This follows Tim Berners-Lee's URI spec at-
    
         http://www.w3.org/Addressing/URI/URI_Overview.html
    
    For the purpose of this class, here is the most elaborate form of a URI
    and its corresponding parts-
    
         ftp://username:password@hostname:portnumber/pathname
         \ /   \               / \      / \        /\       /
          -     ---------------   ------   --------  -------
          |            |             |        |         |
          |            |             |        |        Path
          |            |             |       Port         
          |            |            Host
          |         PreHost            
        Scheme

    Note that this class does not assume knowledge of search/query portions 
    embedded within the path portion of the URI.

	This class pretty much "final" and there shouldn't be anything added.
	If you do feel something belongs here, please do send me a mail. Thanks!

	-Gagan Saksena 03/15/99
 */

class nsIURI : public nsISupports
{

public:
    
    //Core parsing functions

    /* 
        The Scheme is the protocol that this URI refers to. 
    */
    NS_IMETHOD          GetScheme(const char* *o_Scheme) const = 0;
    NS_IMETHOD          SetScheme(const char* i_Scheme) = 0;

    /* 
        The PreHost portion includes elements like the optional 
        username:password, or maybe other scheme specific items. 
    */
    NS_IMETHOD          GetPreHost(const char* *o_PreHost) const = 0;
    NS_IMETHOD          SetPreHost(const char* i_PreHost) = 0;

    /* 
        The Host is the internet domain name to which this URI refers. 
        Note that it could be an IP address as well. 
    */
    NS_IMETHOD          GetHost(const char* *o_Host) const = 0;
    NS_IMETHOD          SetHost(const char* i_Host) = 0;

    /* 
        A return value of -1 indicates that no port value is set and the 
        implementor of the specific scheme will use its default port. 
        Similarly setting a value of -1 indicates that the default is to be used.
        Thus as an example-
            for HTTP, Port 80 is same as a return value of -1. 
        However after setting a port (even if its default), the port number will
        appear in the ToString function.
    */
    NS_IMETHOD_(PRInt32)
                        GetPort(void) const = 0;
    NS_IMETHOD          SetPort(PRInt32 i_Port) = 0;

    /* 
        Note that the path includes the leading '/' Thus if no path is 
        available the GetPath will return a "/" 
        For SetPath if none is provided, one would be prefixed to the path. 
    */
    NS_IMETHOD          GetPath(const char* *o_Path) const = 0;
    NS_IMETHOD          SetPath(const char* i_Path) = 0;

    //Other utility functions
    /* 
        Note that this comparison is only on char* level. Use 
        the scheme specific URI to do a more thorough check. For example--
        in HTTP-
            http://foo.com:80 == http://foo.com
        but this function through nsIURI alone will not return equality
		for this case.
    */
    NS_IMETHOD_(PRBool) Equals(const nsIURI* i_URI) const = 0;

    /* 
        Writes a string representation of the URI. If *o_URIString is null,
        a new one will be created. Free it with delete[]
    */
    NS_IMETHOD          ToString(const char* *o_URIString) const = 0;

    static const nsIID& GetIID() { 
        // {EF4B5380-C07A-11d2-A1BA-00609794CF59}
        static const nsIID NS_IURI_IID = 
        { 0xef4b5380, 0xc07a, 0x11d2, { 0xa1, 0xba, 0x0, 0x60, 0x97, 0x94, 0xcf, 0x59 } };
        return NS_IURI_IID; 
    };

};

#endif /* _nsIURI_h_ */
