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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/******

  This file contains the list of all HTTP atoms 
  See nsHTTPAtoms.h for access to the atoms

  It is designed to be used as inline input to nsHTTPAtoms.cpp *only*
  through the magic of C preprocessing.

  All entires must be enclosed in the macro HTTP_ATOM which will have cruel
  and unusual things done to it

  The first argument to HTTP_ATOM is the C++ name of the atom
  The second argument it HTTP_ATOM is the string value of the atom

 ******/

HTTP_ATOM(Accept,                    "accept")
HTTP_ATOM(Accept_Charset,            "accept-charset")
HTTP_ATOM(Accept_Encoding,           "accept-encoding")
HTTP_ATOM(Accept_Language,           "accept-language")
HTTP_ATOM(Accept_Ranges,             "accept-ranges")
HTTP_ATOM(Age,                       "age")
HTTP_ATOM(Allow,                     "allow")
HTTP_ATOM(Authentication,            "authentication")
HTTP_ATOM(Authorization,             "authorization")
HTTP_ATOM(Connection,                "connection")
HTTP_ATOM(Content_Base,              "content-base")
HTTP_ATOM(Content_Encoding,          "content-encoding")
HTTP_ATOM(Content_Language,          "content-language")
HTTP_ATOM(Content_Length,            "content-length")
HTTP_ATOM(Content_Location,          "content-location")
HTTP_ATOM(Content_MD5,               "content-md5")
HTTP_ATOM(Content_Range,             "content-range")
HTTP_ATOM(Content_Transfer_Encoding, "content-transfer-encoding")
HTTP_ATOM(Content_Type,              "content-type")
HTTP_ATOM(Date,                      "date")
HTTP_ATOM(Derived_From,              "derived-from")
HTTP_ATOM(ETag,                      "etag")
HTTP_ATOM(Expect,                    "expect")
HTTP_ATOM(Expires,                   "expires")
HTTP_ATOM(From,                      "from")
HTTP_ATOM(Forwarded,                 "forwarded")
HTTP_ATOM(Host,                      "host")
HTTP_ATOM(If_Match,                  "if-match")
HTTP_ATOM(If_Match_Any,              "if-match-any")
HTTP_ATOM(If_Modified_Since,         "if-modified-since")
HTTP_ATOM(If_None_Match,             "if-mone-match")
HTTP_ATOM(If_None_Match_Any,         "if-none-match-any")
HTTP_ATOM(If_Range,                  "if-range")
HTTP_ATOM(If_Unmodified_Since,       "if-unmodified-since")
HTTP_ATOM(Last_Modified,             "last-modified")
HTTP_ATOM(Link,                      "link")
HTTP_ATOM(Location,                  "location")
HTTP_ATOM(Max_Forwards,              "max-forwards")
HTTP_ATOM(Message_Id,                "message-id")
HTTP_ATOM(Mime,                      "mime")
HTTP_ATOM(Pragma,                    "pragma")
HTTP_ATOM(Range,                     "range")
HTTP_ATOM(Referer,                   "referer")
HTTP_ATOM(Retry_After,               "retry-after")
HTTP_ATOM(Server,                    "server")
HTTP_ATOM(Title,                     "title")
HTTP_ATOM(Trailer,                   "trailer")
HTTP_ATOM(Transfer_Encoding,         "transfer-encoding")
HTTP_ATOM(URI,                       "uri")
HTTP_ATOM(User_Agent,                "user-agent")
HTTP_ATOM(Version,                   "version")
HTTP_ATOM(Warning,                   "warning")
HTTP_ATOM(WWW_Authenticate,          "www-authenticate")
HTTP_ATOM(Set_Cookie,                "set-cookie")
