/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Frank Yung-Fong Tang <ftang@netscape.com>
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
 
#include <Gestalt.h>
#include "nsTSMStrategy.h"

#ifdef DEBUG

//#define FORCE_USE_UNICODE_API
//#define FORCE_NOT_USE_UNICODE_API

#endif 


PRBool nsTSMStrategy::gUseUnicodeForInputMethod = PR_FALSE;
PRBool nsTSMStrategy::gUseUnicodeForKeyboard = PR_FALSE;
PRBool nsTSMStrategy::gInit = PR_FALSE;

void nsTSMStrategy::Init()
{
  if ( !gInit)
  {
    gInit = PR_TRUE;
    OSErr  err;
    long version;
    err = Gestalt(gestaltTSMgrVersion, &version);
    if ((err == noErr) && (version >=  gestaltTSMgr15))
    {
      gUseUnicodeForInputMethod = PR_TRUE;
      // only enable if OS 9.0 or greater; there is a bug
      // (in at least OS 8.6) that causes double input (Bug #106022)
      err = Gestalt(gestaltSystemVersion, &version);
      gUseUnicodeForKeyboard = (err == noErr) && (version >= 0x900);
    } 
#ifdef FORCE_USE_UNICODE_API
    gUseUnicodeForInputMethod = PR_TRUE;
#elif defined( FORCE_NOT_USE_UNICODE_API )
    gUseUnicodeForInputMethod = PR_FALSE;
#endif
    // there are no way we can use unicode for keyboard, but not using 
    // Unicode for IME
    if ( !gUseUnicodeForInputMethod)
      gUseUnicodeForKeyboard = PR_FALSE;
  }
    
}

PRBool nsTSMStrategy::UseUnicodeForInputMethod()
{
  Init();
  return gUseUnicodeForInputMethod;  
}

PRBool nsTSMStrategy::UseUnicodeForKeyboard()
{
  Init();
  return gUseUnicodeForKeyboard;  
}