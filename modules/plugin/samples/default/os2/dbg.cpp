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
 *   IBM Corp.
 */

#include <os2.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

extern char szAppName[];

#ifdef _DEBUG

void __cdecl dbgOut(LPSTR format, ...) { 
  static char buf[1024];
  lstrcpy(buf, szAppName);
  lstrcat(buf, ": ");
  va_list  va;
  va_start(va, format);
  wvsprintf(&buf[lstrlen(buf)], format, va);
  va_end(va);
  lstrcat(buf, "\n");
  OutputDebugString(buf); 
}

#endif
