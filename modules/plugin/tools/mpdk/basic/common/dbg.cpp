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
 *
 */

/**********************************************************************
*
* dbg.cpp
*
* This is a helper utility which allows diagnostic info in the output
* window in debug project environment in more convenient way than
* printf or OutputDebugString
*
***********************************************************************/

#include "xplat.h"

extern char szAppName[];

#ifdef _DEBUG
void __cdecl dbgOut(LPSTR format, ...) 
{ 
#ifdef XP_PC
  static char buf[1024];
  strcpy(buf, szAppName);
  strcat(buf, ": ");
  va_list  va;
  va_start(va, format);
  wvsprintf(&buf[lstrlen(buf)], format, va);
  va_end(va);
  strcat(buf, "\n");
  OutputDebugString(buf); 
#endif
}

#endif

