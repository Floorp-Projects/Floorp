/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*

  XPCOM Class ID for the HTTP Handler that can be created with the
  factory.

  -Gagan Saksena 03/24/99

*/

#ifndef nsHTTPCID_h__
#define nsHTTPCID_h__

// {1C49C1D0-E254-11d2-B016-006097BFC036}
#define NS_HTTP_HANDLER_FACTORY_CID \
    { 0x1c49c1d0, 0xe254, 0x11d2, { 0xb0, 0x16, 0x0, 0x60, 0x97, 0xbf, 0xc0, 0x36 } }

#endif // nsHTTPCID_h__
