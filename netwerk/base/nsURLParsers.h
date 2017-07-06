/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsURLParsers_h__
#define nsURLParsers_h__

#include "nsIURLParser.h"
#include "mozilla/Attributes.h"

//----------------------------------------------------------------------------
// base class for url parsers
//----------------------------------------------------------------------------

class nsBaseURLParser : public nsIURLParser
{
public:
    NS_DECL_NSIURLPARSER

    nsBaseURLParser() { }

protected:
    // implemented by subclasses
    virtual void ParseAfterScheme(const char *spec, int32_t specLen,
                                  uint32_t *authPos, int32_t *authLen,
                                  uint32_t *pathPos, int32_t *pathLen) = 0;
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

class nsNoAuthURLParser final : public nsBaseURLParser
{
    ~nsNoAuthURLParser() {}

public:
    NS_DECL_THREADSAFE_ISUPPORTS

#if defined(XP_WIN)
    NS_IMETHOD ParseFilePath(const char *, int32_t,
                             uint32_t *, int32_t *,
                             uint32_t *, int32_t *,
                             uint32_t *, int32_t *) override;
#endif

    NS_IMETHOD ParseAuthority(const char *auth, int32_t authLen,
                              uint32_t *usernamePos, int32_t *usernameLen,
                              uint32_t *passwordPos, int32_t *passwordLen,
                              uint32_t *hostnamePos, int32_t *hostnameLen,
                              int32_t *port) override;

    void ParseAfterScheme(const char *spec, int32_t specLen,
                          uint32_t *authPos, int32_t *authLen,
                          uint32_t *pathPos, int32_t *pathLen) override;
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
protected:
    virtual ~nsAuthURLParser() {}

public:
    NS_DECL_THREADSAFE_ISUPPORTS

    NS_IMETHOD ParseAuthority(const char *auth, int32_t authLen,
                              uint32_t *usernamePos, int32_t *usernameLen,
                              uint32_t *passwordPos, int32_t *passwordLen,
                              uint32_t *hostnamePos, int32_t *hostnameLen,
                              int32_t *port) override;

    NS_IMETHOD ParseUserInfo(const char *userinfo, int32_t userinfoLen,
                             uint32_t *usernamePos, int32_t *usernameLen,
                             uint32_t *passwordPos, int32_t *passwordLen) override;

    NS_IMETHOD ParseServerInfo(const char *serverinfo, int32_t serverinfoLen,
                               uint32_t *hostnamePos, int32_t *hostnameLen,
                               int32_t *port) override;

    void ParseAfterScheme(const char *spec, int32_t specLen,
                          uint32_t *authPos, int32_t *authLen,
                          uint32_t *pathPos, int32_t *pathLen) override;
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
    virtual ~nsStdURLParser() {}

public:
    void ParseAfterScheme(const char *spec, int32_t specLen,
                          uint32_t *authPos, int32_t *authLen,
                          uint32_t *pathPos, int32_t *pathLen);
};

#endif // nsURLParsers_h__
