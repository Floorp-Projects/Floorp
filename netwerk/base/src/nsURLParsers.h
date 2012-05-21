/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
//     file://foo/bar.txt     (the authority "foo"  is ignored)
//----------------------------------------------------------------------------

class nsNoAuthURLParser : public nsBaseURLParser
{
public: 
#if defined(XP_WIN) || defined(XP_OS2)
    NS_IMETHOD ParseFilePath(const char *, PRInt32,
                             PRUint32 *, PRInt32 *,
                             PRUint32 *, PRInt32 *,
                             PRUint32 *, PRInt32 *);
#endif

    NS_IMETHOD ParseAuthority(const char *auth, PRInt32 authLen,
                              PRUint32 *usernamePos, PRInt32 *usernameLen,
                              PRUint32 *passwordPos, PRInt32 *passwordLen,
                              PRUint32 *hostnamePos, PRInt32 *hostnameLen,
                              PRInt32 *port);

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
