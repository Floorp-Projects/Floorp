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

#ifndef INSTALLCLEANUPDEFINES_H
#define INSTALLCLEANUPDEFINES_H

#define CLEANUP_MESSAGE_FILENAME  "cmessage.txt"

#ifndef XP_MAC
#define CLEANUP_REGISTRY "xpicleanup.dat"
#endif

#if defined (XP_MAC)
#define CLEANUP_REGISTRY  "XPICleanup Data"
#define ESSENTIAL_FILES  "Essential Files"
#ifdef DEBUG
#define CLEANUP_UTIL "XPICleanupDebug"
#else
#define CLEANUP_UTIL "XPICleanup"
#endif //DEBUG

#elif defined (XP_WIN)
#define CLEANUP_UTIL "xpicleanup.exe"

#elif defined (XP_OS2)
#define CLEANUP_UTIL "xpicleanup.exe"

#else
#define CLEANUP_UTIL "xpicleanup"

#endif


#endif
