/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mike Pinkerton (pinkerton@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "DefProcFakery.h"


#pragma options align=mac68k
typedef struct {
  short jmpInstr;
  Ptr jmpAddr;
  Handle mSystemDefProc;
} JmpRecord, *JmpPtr, **JmpHandle;
#pragma options align=reset


Boolean 
DefProcFakery :: CreateDefProc( RoutineDescriptor* inRoutineAddr, Handle inSystemDefProc, Handle* outDefProcHandle )
{
  *outDefProcHandle = ::NewHandle(sizeof(JmpRecord));
  if ( !*outDefProcHandle )
    return false;

  JmpHandle jh = (JmpHandle) *outDefProcHandle;
  (**jh).jmpInstr = 0x4EF9;                 // jump instruction
  (**jh).jmpAddr = (Ptr) inRoutineAddr;     // where to jump to
  (**jh).mSystemDefProc = inSystemDefProc;  // the system defproc, so we can get it later

  ::HLockHi((Handle)jh);

  return true;
}


//
// GetSystemDefProc
//
// Returns the system defProc stashed in the fake defproc from when it was created
//
Handle
DefProcFakery :: GetSystemDefProc ( Handle inFakedDefProc )
{
  Handle sysDefProc = NULL;
  
  JmpHandle jH = (JmpHandle) inFakedDefProc;
  if ( jH )
    sysDefProc = (**jH).mSystemDefProc;
    
  return sysDefProc;

} // GetSystemDefProc


//
// DestroyDefProc
//
// Delete the def proc and the routine descriptor associated with it
//
void
DefProcFakery :: DestroyDefProc ( Handle inFakedDefProc ) 
{
  JmpHandle jh = (JmpHandle) inFakedDefProc;
  
  ::DisposeRoutineDescriptor ( (RoutineDescriptor*)((**jh).jmpAddr) );
  ::DisposeHandle ( inFakedDefProc );

} // DestroyDefProc