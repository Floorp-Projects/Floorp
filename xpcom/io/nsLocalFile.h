/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Doug Turner <dougt@netscape.com>
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 build.
 */

#ifndef _NS_LOCAL_FILE_H_
#define _NS_LOCAL_FILE_H_

#define NS_LOCAL_FILE_CID {0x2e23e220, 0x60be, 0x11d3, {0x8c, 0x4a, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74}}

// nsXPComInit needs to know about how we are implemented,
// so here we will export it.  Other users should not depend
// on this.

#ifdef XP_WIN
#include "nsLocalFileWin.h"
#elif defined(XP_MAC) && !defined(XP_MACOSX)
#include "nsLocalFileMac.h"
#elif defined(XP_UNIX) || defined(XP_BEOS) || defined(XP_MACOSX)
#include "nsLocalFileUnix.h"
#elif defined(XP_OS2)
#include "nsLocalFileOS2.h"
#else
#error NOT_IMPLEMENTED
#endif

void NS_StartupLocalFile();
void NS_ShutdownLocalFile();
#endif
