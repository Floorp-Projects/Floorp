/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/******
  This file contains the list of all HTTP atoms
  See nsHttp.h for access to the atoms.

  It is designed to be used as inline input to nsHttp.cpp *only*
  through the magic of C preprocessing.

  All entries must be enclosed in the macro HTTP_ATOM which will have cruel
  and unusual things done to it.

  The first argument to HTTP_ATOM is the C++ name of the atom.
  The second argument to HTTP_ATOM is the string value of the atom.
 ******/

HTTP_ATOM(Accept,                    "Accept")
HTTP_ATOM(Accept_Encoding,           "Accept-Encoding")
HTTP_ATOM(Accept_Language,           "Accept-Language")
HTTP_ATOM(Accept_Ranges,             "Accept-Ranges")
HTTP_ATOM(Age,                       "Age")
HTTP_ATOM(Allow,                     "Allow")
HTTP_ATOM(Alternate_Service,         "Alt-Svc")
HTTP_ATOM(Alternate_Service_Used,    "Alt-Svc-Used")
HTTP_ATOM(Assoc_Req,                 "Assoc-Req")
HTTP_ATOM(Authentication,            "Authentication")
HTTP_ATOM(Authorization,             "Authorization")
HTTP_ATOM(Cache_Control,             "Cache-Control")
HTTP_ATOM(Connection,                "Connection")
HTTP_ATOM(Content_Disposition,       "Content-Disposition")
HTTP_ATOM(Content_Encoding,          "Content-Encoding")
HTTP_ATOM(Content_Language,          "Content-Language")
HTTP_ATOM(Content_Length,            "Content-Length")
HTTP_ATOM(Content_Location,          "Content-Location")
HTTP_ATOM(Content_MD5,               "Content-MD5")
HTTP_ATOM(Content_Range,             "Content-Range")
HTTP_ATOM(Content_Type,              "Content-Type")
HTTP_ATOM(Cookie,                    "Cookie")
HTTP_ATOM(Date,                      "Date")
HTTP_ATOM(DAV,                       "DAV")
HTTP_ATOM(Depth,                     "Depth")
HTTP_ATOM(Destination,               "Destination")
HTTP_ATOM(DoNotTrack,                "DNT")
HTTP_ATOM(ETag,                      "Etag")
HTTP_ATOM(Expect,                    "Expect")
HTTP_ATOM(Expires,                   "Expires")
HTTP_ATOM(From,                      "From")
HTTP_ATOM(Host,                      "Host")
HTTP_ATOM(If,                        "If")
HTTP_ATOM(If_Match,                  "If-Match")
HTTP_ATOM(If_Modified_Since,         "If-Modified-Since")
HTTP_ATOM(If_None_Match,             "If-None-Match")
HTTP_ATOM(If_None_Match_Any,         "If-None-Match-Any")
HTTP_ATOM(If_Range,                  "If-Range")
HTTP_ATOM(If_Unmodified_Since,       "If-Unmodified-Since")
HTTP_ATOM(Keep_Alive,                "Keep-Alive")
HTTP_ATOM(Last_Modified,             "Last-Modified")
HTTP_ATOM(Lock_Token,                "Lock-Token")
HTTP_ATOM(Link,                      "Link")
HTTP_ATOM(Location,                  "Location")
HTTP_ATOM(Max_Forwards,              "Max-Forwards")
HTTP_ATOM(Overwrite,                 "Overwrite")
HTTP_ATOM(Pragma,                    "Pragma")
HTTP_ATOM(Prefer,                    "Prefer")
HTTP_ATOM(Proxy_Authenticate,        "Proxy-Authenticate")
HTTP_ATOM(Proxy_Authorization,       "Proxy-Authorization")
HTTP_ATOM(Proxy_Connection,          "Proxy-Connection")
HTTP_ATOM(Range,                     "Range")
HTTP_ATOM(Referer,                   "Referer")
HTTP_ATOM(Retry_After,               "Retry-After")
HTTP_ATOM(Server,                    "Server")
HTTP_ATOM(Set_Cookie,                "Set-Cookie")
HTTP_ATOM(Set_Cookie2,               "Set-Cookie2")
HTTP_ATOM(Status_URI,                "Status-URI")
HTTP_ATOM(TE,                        "TE")
HTTP_ATOM(Title,                     "Title")
HTTP_ATOM(Timeout,                   "Timeout")
HTTP_ATOM(Trailer,                   "Trailer")
HTTP_ATOM(Transfer_Encoding,         "Transfer-Encoding")
HTTP_ATOM(URI,                       "URI")
HTTP_ATOM(Upgrade,                   "Upgrade")
HTTP_ATOM(User_Agent,                "User-Agent")
HTTP_ATOM(Vary,                      "Vary")
HTTP_ATOM(Version,                   "Version")
HTTP_ATOM(WWW_Authenticate,          "WWW-Authenticate")
HTTP_ATOM(Warning,                   "Warning")
HTTP_ATOM(X_Firefox_Spdy,            "X-Firefox-Spdy")
HTTP_ATOM(X_Firefox_Spdy_Proxy,      "X-Firefox-Spdy-Proxy")

// methods are case sensitive and do not use atom table
