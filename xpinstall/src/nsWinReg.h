/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef __NS_WINREG_H__
#define __NS_WINREG_H__

//#include "prtypes.h"
#include "nsSoftUpdateEnums.h" 
#include "nsWinRegValue.h"

//#include "nsPrincipal.h"
//#include "nsPrivilegeManager.h"
//#include "nsTarget.h"
//#include "nsUserTarget.h"

#include "nscore.h"
#include "nsISupports.h"

#include "jsapi.h"

#include "plevent.h"

#include "nsString.h"
#include "nsFileSpec.h"
#include "nsVector.h"
#include "nsHashtable.h"

#include "nsSoftwareUpdate.h"

#include "nsInstallObject.h"
#include "nsInstallVersion.h"
#include "nsInstall.h"

class nsWinReg
{
  public:

    enum
    {
      HKEY_CLASSES_ROOT             = 0x80000000,
      HKEY_CURRENT_USER             = 0x80000001,
      HKEY_LOCAL_MACHINE            = 0x80000002,
      HKEY_USERS                    = 0x80000003
    };

    /* Public Fields */

    /* Public Methods */

                      nsWinReg(nsInstall* suObj);

    PRInt32           setRootKey(PRInt32 key);
    PRInt32           createKey(nsString subkey, nsString classname, PRInt32* aReturn);
    PRInt32           deleteKey(nsString subkey, PRInt32* aReturn);
    PRInt32           deleteValue(nsString subkey, nsString valname, PRInt32* aReturn);
    PRInt32           setValueString(nsString subkey, nsString valname, nsString value, PRInt32* aReturn);
    PRInt32           getValueString(nsString subkey, nsString valname, nsString** aReturn);
    PRInt32           setValue(nsString subkey, nsString valname, nsWinRegValue* value, PRInt32* aReturn);
    PRInt32           getValue(nsString subkey, nsString valname, nsWinRegValue** aReturn);

    nsInstall*        installObject(void);

    PRInt32           finalCreateKey(PRInt32 root, nsString subkey, nsString classname, PRInt32* aReturn);
    PRInt32           finalDeleteKey(PRInt32 root, nsString subkey, PRInt32* aReturn);
    PRInt32           finalDeleteValue(PRInt32 root, nsString subkey, nsString valname, PRInt32* aReturn);
    PRInt32           finalSetValueString(PRInt32 root, nsString subkey, nsString valname, nsString value, PRInt32* aReturn);
    PRInt32           finalSetValue(PRInt32 root, nsString subkey, nsString valname, nsWinRegValue* value, PRInt32* aReturn);

    
  private:
    
    /* Private Fields */
    PRInt32 rootkey;
//    nsPrincipal* principal;
//    nsPrivilegeManager* privMgr;
//    nsTarget* impersonation;
    nsInstall* su;

//    nsUserTarget* target;
    
    /* Private Methods */
    PRInt32 nativeCreateKey(nsString subkey, nsString classname);
    PRInt32 nativeDeleteKey(nsString subkey);
    PRInt32 nativeDeleteValue(nsString subkey, nsString valname);

    PRInt32 nativeSetValueString(nsString subkey, nsString valname, nsString value);
    nsString* nativeGetValueString(nsString subkey, nsString valname);

    PRInt32 nativeSetValue(nsString subkey, nsString valname, nsWinRegValue* value);
    nsWinRegValue* nativeGetValue(nsString subkey, nsString valname);
//    PRBool resolvePrivileges();
};

#endif /* __NS_WINREG_H__ */
