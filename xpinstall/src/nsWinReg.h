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

#include "nsWinRegEnums.h" 
#include "nsWinRegValue.h"

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

#define _MAXKEYVALUE_ 8196
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
    PRInt32           createKey(const nsString& subkey, const nsString& classname, PRInt32* aReturn);
    PRInt32           deleteKey(const nsString& subkey, PRInt32* aReturn);
    PRInt32           deleteValue(const nsString& subkey, const nsString& valname, PRInt32* aReturn);
    PRInt32           setValueString(const nsString& subkey, const nsString& valname, const nsString& value, PRInt32* aReturn);
    PRInt32           getValueString(const nsString& subkey, const nsString& valname, nsString* aReturn);
    PRInt32           setValueNumber(const nsString& subkey, const nsString& valname, PRInt32 value, PRInt32* aReturn);
    PRInt32           getValueNumber(const nsString& subkey, const nsString& valname, PRInt32* aReturn);
    PRInt32           setValue(const nsString& subkey, const nsString& valname, nsWinRegValue* value, PRInt32* aReturn);
    PRInt32           getValue(const nsString& subkey, const nsString& valname, nsWinRegValue** aReturn);

    nsInstall*        installObject(void);

    PRInt32           finalCreateKey(PRInt32 root, const nsString& subkey, const nsString& classname, PRInt32* aReturn);
    PRInt32           finalDeleteKey(PRInt32 root, const nsString& subkey, PRInt32* aReturn);
    PRInt32           finalDeleteValue(PRInt32 root, const nsString& subkey, const nsString& valname, PRInt32* aReturn);
    PRInt32           finalSetValueString(PRInt32 root, const nsString& subkey, const nsString& valname, const nsString& value, PRInt32* aReturn);
    PRInt32           finalSetValueNumber(PRInt32 root, const nsString& subkey, const nsString& valname, PRInt32 value, PRInt32* aReturn);
    PRInt32           finalSetValue(PRInt32 root, const nsString& subkey, const nsString& valname, nsWinRegValue* value, PRInt32* aReturn);

    
  private:
    
    /* Private Fields */
    PRInt32    rootkey;
    nsInstall* su;

    /* Private Methods */
    PRInt32           nativeCreateKey(const nsString& subkey, const nsString& classname);
    PRInt32           nativeDeleteKey(const nsString& subkey);
    PRInt32           nativeDeleteValue(const nsString& subkey, const nsString& valname);

    PRInt32           nativeSetValueString(const nsString& subkey, const nsString& valname, const nsString& value);
    void              nativeGetValueString(const nsString& subkey, const nsString& valname, nsString* aReturn);
    PRInt32           nativeSetValueNumber(const nsString& subkey, const nsString& valname, PRInt32 value);
    void              nativeGetValueNumber(const nsString& subkey, const nsString& valname, PRInt32* aReturn);

    PRInt32           nativeSetValue(const nsString& subkey, const nsString& valname, nsWinRegValue* value);
    nsWinRegValue*    nativeGetValue(const nsString& subkey, const nsString& valname);
};

#endif /* __NS_WINREG_H__ */

