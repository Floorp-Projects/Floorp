/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Darin Fisher (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsURLParsers_h__
#define nsURLParsers_h__

#include "nsIURLParser.h"

//----------------------------------------------------------------------------
// base class for url parsers
//----------------------------------------------------------------------------

class nsBaseURLParser : public nsIURLParser
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIURLPARSER

    nsBaseURLParser() { }

protected:
    // implemented by subclasses
    virtual void ParseAfterScheme(const char *spec, PRInt32 specLen,
                                  PRUint32 *authPos, PRInt32 *authLen,
                                  PRUint32 *pathPos, PRInt32 *pathLen) = 0;
};

//----------------------------------------------------------------------------
// an url parser for urls that do not have an authority section
//
// eg. file:foo/bar.txt
//     file:/foo/bar.txt      (treated equivalently)
//     file:///foo/bar.txt
//
// eg. file:////foo/bar.txt   (UNC-filepath = \\foo\bar.txt)
//
// XXX except in this case:
//     file://foo/bar.txt     (foo is authority)
//----------------------------------------------------------------------------

class nsNoAuthURLParser : public nsBaseURLParser
{
public: 
#if defined(XP_PC)
    NS_IMETHOD ParseFilePath(const char *, PRInt32,
                             PRUint32 *, PRInt32 *,
                             PRUint32 *, PRInt32 *,
                             PRUint32 *, PRInt32 *);
#endif

    void ParseAfterScheme(const char *spec, PRInt32 specLen,
                          PRUint32 *authPos, PRInt32 *authLen,
                          PRUint32 *pathPos, PRInt32 *pathLen);
};

//----------------------------------------------------------------------------
// an url parser for urls that must have an authority section
//
// eg. http:www.foo.com/bar.html
//     http:/www.foo.com/bar.html
//     http://www.foo.com/bar.html    (treated equivalently)
//     http:///www.foo.com/bar.html
//----------------------------------------------------------------------------

class nsAuthURLParser : public nsBaseURLParser
{
public: 
    NS_IMETHOD ParseAuthority(const char *auth, PRInt32 authLen,
                              PRUint32 *usernamePos, PRInt32 *usernameLen,
                              PRUint32 *passwordPos, PRInt32 *passwordLen,
                              PRUint32 *hostnamePos, PRInt32 *hostnameLen,
                              PRInt32 *port);

    NS_IMETHOD ParseUserInfo(const char *userinfo, PRInt32 userinfoLen,
                             PRUint32 *usernamePos, PRInt32 *usernameLen,
                             PRUint32 *passwordPos, PRInt32 *passwordLen);

    NS_IMETHOD ParseServerInfo(const char *serverinfo, PRInt32 serverinfoLen,
                               PRUint32 *hostnamePos, PRInt32 *hostnameLen,
                               PRInt32 *port);

    void ParseAfterScheme(const char *spec, PRInt32 specLen,
                          PRUint32 *authPos, PRInt32 *authLen,
                          PRUint32 *pathPos, PRInt32 *pathLen);
};

//----------------------------------------------------------------------------
// an url parser for urls that may or may not have an authority section
//
// eg. http:www.foo.com              (www.foo.com is authority)
//     http:www.foo.com/bar.html     (www.foo.com is authority)
//     http:/www.foo.com/bar.html    (www.foo.com is part of file path)
//     http://www.foo.com/bar.html   (www.foo.com is authority)
//     http:///www.foo.com/bar.html  (www.foo.com is part of file path)
//----------------------------------------------------------------------------

class nsStdURLParser : public nsAuthURLParser
{
public: 
    void ParseAfterScheme(const char *spec, PRInt32 specLen,
                          PRUint32 *authPos, PRInt32 *authLen,
                          PRUint32 *pathPos, PRInt32 *pathLen);
};

#endif // nsURLParsers_h__
