/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsWinReg_h__
#define nsWinReg_h__

#include "prtypes.h"
#include "nsSoftwareUpdate.h" 
#include "nsSoftUpdateEnums.h" 
#include "nsWinRegValue.h"

#include "nsPrincipal.h"
#include "nsPrivilegeManager.h"
#include "nsTarget.h"
#include "nsUserTarget.h"


PR_BEGIN_EXTERN_C

struct nsWinReg {

public:

  /* Public Fields */

  /* Public Methods */
  nsWinReg(nsSoftwareUpdate* suObj);
  void setRootKey(PRInt32 key);
  PRInt32 createKey(char* subkey, char* classname);
  PRInt32 deleteKey(char* subkey);
  PRInt32 deleteValue(char* subkey, char* valname);
  PRInt32 setValueString(char* subkey, char* valname, char* value);
  char* getValueString(char* subkey, char* valname);
  PRInt32 setValue(char* subkey, char* valname, nsWinRegValue* value);
  nsWinRegValue* getValue(char* subkey, char* valname);
  nsSoftwareUpdate* softUpdate();

  PRInt32 finalCreateKey(PRInt32 root, char* subkey, char* classname);

  PRInt32 finalDeleteKey(PRInt32 root, char* subkey);

  PRInt32 finalDeleteValue(PRInt32 root, char* subkey, char* valname);
  PRInt32 finalSetValueString(PRInt32 root, char* subkey, char* valname, char* value);
  PRInt32 finalSetValue(PRInt32 root, char* subkey, char* valname, nsWinRegValue* value);

  
private:
  
  /* Private Fields */
  PRInt32 rootkey;
  nsPrincipal* principal;
  nsPrivilegeManager* privMgr;
  nsTarget* impersonation;
  nsSoftwareUpdate* su;

  nsUserTarget* target;
  
  /* Private Methods */
  PRInt32 nativeCreateKey(char* subkey, char* classname);
  PRInt32 nativeDeleteKey(char* subkey);
  PRInt32 nativeDeleteValue(char* subkey, char* valname);

  PRInt32 nativeSetValueString(char* subkey, char* valname, char* value);
  char* nativeGetValueString(char* subkey, char* valname);

  PRInt32 nativeSetValue(char* subkey, char* valname, nsWinRegValue* value);
  nsWinRegValue* nativeGetValue(char* subkey, char* valname);
  PRBool resolvePrivileges();
  
};

PR_END_EXTERN_C

#endif /* nsWinReg_h__ */
