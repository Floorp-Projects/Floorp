/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gagan Saksena <gagan@netscape.com> (original author)
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
HTTP_ATOM(Alternate_Protocol,        "Alternate-Protocol")
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

// methods are atoms too.
//
// Note: winnt.h defines DELETE macro, so we'll just keep the methods mixedcase
// even though they're normally written all uppercase. -- darin

HTTP_ATOM(Connect,                   "CONNECT")
HTTP_ATOM(Copy,                      "COPY")
HTTP_ATOM(Delete,                    "DELETE")
HTTP_ATOM(Get,                       "GET")
HTTP_ATOM(Head,                      "HEAD")
HTTP_ATOM(Index,                     "INDEX")
HTTP_ATOM(Lock,                      "LOCK")
HTTP_ATOM(M_Post,                    "M-POST")
HTTP_ATOM(Mkcol,                     "MKCOL")
HTTP_ATOM(Move,                      "MOVE")
HTTP_ATOM(Options,                   "OPTIONS")
HTTP_ATOM(Post,                      "POST")
HTTP_ATOM(Propfind,                  "PROPFIND")
HTTP_ATOM(Proppatch,                 "PROPPATCH")
HTTP_ATOM(Put,                       "PUT")
HTTP_ATOM(Report,                    "REPORT")
HTTP_ATOM(Search,                    "SEARCH")
HTTP_ATOM(Trace,                     "TRACE")
HTTP_ATOM(Unlock,                    "UNLOCK")
