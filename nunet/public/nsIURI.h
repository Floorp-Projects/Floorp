/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
        Writes a string representation of the URI. 
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
