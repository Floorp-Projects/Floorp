/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *      Gagan Saksena <gagan@netscape.com> (original author)
 */

/******
  This file contains the list of all HTTP atoms 
  See nsHttp.h for access to the atoms

  It is designed to be used as inline input to nsHttp.cpp *only*
  through the magic of C preprocessing.

  All entires must be enclosed in the macro HTTP_ATOM which will have cruel
  and unusual things done to it

  The first argument to HTTP_ATOM is the C++ name of the atom
  The second argument it HTTP_ATOM is the string value of the atom
 ******/

HTTP_ATOM(Accept,                    "Accept")
HTTP_ATOM(Accept_Charset,            "Accept-Charset")
HTTP_ATOM(Accept_Encoding,           "Accept-Encoding")
HTTP_ATOM(Accept_Language,           "Accept-Language")
HTTP_ATOM(Accept_Ranges,             "Accept-Ranges")
HTTP_ATOM(Age,                       "Age")
HTTP_ATOM(Allow,                     "Allow")
HTTP_ATOM(Authentication,            "Authentication")
HTTP_ATOM(Authorization,             "Authorization")
HTTP_ATOM(Cache_Control,             "Cache-Control")
HTTP_ATOM(Connection,                "Connection")
HTTP_ATOM(Content_Base,              "Content-Base")
HTTP_ATOM(Content_Encoding,          "Content-Encoding")
HTTP_ATOM(Content_Language,          "Content-Language")
HTTP_ATOM(Content_Length,            "Content-Length")
HTTP_ATOM(Content_Location,          "Content-Location")
HTTP_ATOM(Content_MD5,               "Content-MD5")
HTTP_ATOM(Content_Range,             "Content-Range")
HTTP_ATOM(Content_Transfer_Encoding, "Content-Transfer-Encoding")
HTTP_ATOM(Content_Type,              "Content-Type")
HTTP_ATOM(Date,                      "Date")
HTTP_ATOM(Derived_From,              "Derived-From")
HTTP_ATOM(ETag,                      "Etag")
HTTP_ATOM(Expect,                    "Expect")
HTTP_ATOM(Expires,                   "Expires")
HTTP_ATOM(From,                      "From")
HTTP_ATOM(Forwarded,                 "Forwarded")
HTTP_ATOM(Host,                      "Host")
HTTP_ATOM(If_Match,                  "If-Match")
HTTP_ATOM(If_Match_Any,              "If-Match-Any")
HTTP_ATOM(If_Modified_Since,         "If-Modified-Since")
HTTP_ATOM(If_None_Match,             "If-None-Match")
HTTP_ATOM(If_None_Match_Any,         "If-None-Match-Any")
HTTP_ATOM(If_Range,                  "If-Range")
HTTP_ATOM(If_Unmodified_Since,       "If-Unmodified-Since")
HTTP_ATOM(Keep_Alive,                "Keep-Alive")
HTTP_ATOM(Last_Modified,             "Last-Modified")
HTTP_ATOM(Link,                      "Link")
HTTP_ATOM(Location,                  "Location")
HTTP_ATOM(Max_Forwards,              "Max-Forwards")
HTTP_ATOM(Message_Id,                "Message-Id")
HTTP_ATOM(Mime,                      "Mime")
HTTP_ATOM(Pragma,                    "Pragma")
HTTP_ATOM(Vary,                      "Vary")
HTTP_ATOM(Proxy_Authenticate,        "Proxy-Authenticate")
HTTP_ATOM(Proxy_Authorization,       "Proxy-Authorization")
HTTP_ATOM(Proxy_Connection,          "Proxy-Connection")
HTTP_ATOM(Range,                     "Range")
HTTP_ATOM(Referer,                   "Referer")
HTTP_ATOM(Retry_After,               "Retry-After")
HTTP_ATOM(Server,                    "Server")
HTTP_ATOM(TE,                        "TE")
HTTP_ATOM(Title,                     "Title")
HTTP_ATOM(Trailer,                   "Trailer")
HTTP_ATOM(Transfer_Encoding,         "Transfer-Encoding")
HTTP_ATOM(Upgrade,                   "Upgrade")
HTTP_ATOM(URI,                       "URI")
HTTP_ATOM(User_Agent,                "User-Agent")
HTTP_ATOM(Version,                   "Version")
HTTP_ATOM(Warning,                   "Warning")
HTTP_ATOM(WWW_Authenticate,          "WWW-Authenticate")
HTTP_ATOM(Set_Cookie,                "Set-Cookie")
HTTP_ATOM(Cookie,                    "Cookie")

// methods are atoms too.
//
// note: an uppercase DELETE causes compilation problems under msvc6, so we'll
// just keep the methods mixedcase even though they're normally written all
// uppercase -- darin

HTTP_ATOM(Options,                   "OPTIONS")
HTTP_ATOM(Head,                      "HEAD")
HTTP_ATOM(Post,                      "POST")
HTTP_ATOM(Put,                       "PUT")
HTTP_ATOM(Get,                       "GET")
HTTP_ATOM(Index,                     "INDEX")
HTTP_ATOM(Delete,                    "DELETE")
HTTP_ATOM(Trace,                     "TRACE")
HTTP_ATOM(Connect,                   "CONNECT")
HTTP_ATOM(M_Post,                    "M-POST")
