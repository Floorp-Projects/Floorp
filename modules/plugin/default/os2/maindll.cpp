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

HMODULE hInst; // global

// Compiler run-time functions.
extern "C" {
int  _CRT_init( void );
void _CRT_term( void );
void __ctordtorInit( void );
void __ctordtorTerm( void );
}

unsigned long _System _DLL_InitTerm( unsigned long hModule,
                                      unsigned long ulFlag ) {
  APIRET rc;
 
  switch (ulFlag) {
  case 0 :
    // Init: Prime compiler run-time and construct static C++ objects.
    if ( _CRT_init() == -1 ) {
      return 0UL;
    } else {
      __ctordtorInit();
      hInst = hModule;
    }
    break;
    
  case 1 :
    __ctordtorTerm();
    _CRT_term();
    hInst = NULLHANDLE;
    break;
    
  default  :
    return 0UL;
  }

  return 1;
}
