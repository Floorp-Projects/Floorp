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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef NSDEFS_H
#define NSDEFS_H

#include <os2.h>
#ifdef XP_OS2_VACPP
#include <builtin.h>
#endif

#ifdef _DEBUG
  #define BREAK_TO_DEBUGGER           _interrupt(3)
#else   
  #define BREAK_TO_DEBUGGER
#endif  

#ifdef _DEBUG
  #define VERIFY(exp)                 ((exp) ? 0: (WinGetLastError((HAB)0), BREAK_TO_DEBUGGER))
#else   // !_DEBUG
  #define VERIFY(exp)                 (exp)
#endif  // !_DEBUG

#define WC_SCROLLBAR_STRING    "#8"  //string equivalent to WC_SCROLLBAR

#endif  // NSDEFS_H


