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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsAppShellCIDs_h__
#define nsAppShellCIDs_h__
#include "nsIFactory.h"

// 43147b80-8a39-11d2-9938-0080c7cb1080
#define NS_APPSHELL_SERVICE_CID \
{ 0x43147b80, 0x8a39, 0x11d2, \
  {0x99, 0x38, 0x00, 0x80, 0xc7, 0xcb, 0x10, 0x80} }

// XXX:  This is temporary...
// 106b6f40-d79e-11d2-99db-0080c7cb1080
#define NS_PROTOCOL_HELPER_CID \
{ 0x106b6f40, 0xd79e, 0x11d2, \
  {0x99, 0xdb, 0x00, 0x80, 0xc7, 0xcb, 0x10, 0x80} }

#define NS_SESSIONHISTORY_CID \
{ 0x68e73d52, 0x12eb, 0x11d3, { 0xbd, 0xc0, 0x00, 0x50, 0x04, 0x0a, 0x9b, 0x44 } }

extern nsresult NS_NewSessionHistoryFactory(nsIFactory** aResult);

#endif /* nsAppShellCIDs_h__ */

