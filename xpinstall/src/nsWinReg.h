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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef __NS_WINREG_H__
#define __NS_WINREG_H__

#include "nsWinRegEnums.h" 
#include "nsWinRegValue.h"

#include "nscore.h"
#include "nsISupports.h"

#include "jsapi.h"

#include "plevent.h"

#include "nsString.h"
#include "nsHashtable.h"

#include "nsSoftwareUpdate.h"

#include "nsInstallObject.h"
#include "nsInstallVersion.h"
#include "nsInstall.h"

#define _MAXKEYVALUE_ 8196

#ifndef MAX_BUF
#define MAX_BUF 4096
#endif

class nsWinReg
{
  public:

    enum
    {
      NS_HKEY_CLASSES_ROOT          = 0x80000000,
      NS_HKEY_CURRENT_USER          = 0x80000001,
      NS_HKEY_LOCAL_MACHINE         = 0x80000002,
      NS_HKEY_USERS                 = 0x80000003
    };

    /* Public Fields */

    /* Public Methods */

                      nsWinReg(nsInstall* suObj);
                      ~nsWinReg();

    PRInt32           SetRootKey(PRInt32 key);
    PRInt32           CreateKey(const nsAString& subkey, const nsAString& classname, PRInt32* aReturn);
    PRInt32           DeleteKey(const nsAString& subkey, PRInt32* aReturn);
    PRInt32           DeleteValue(const nsAString& subkey, const nsAString& valname, PRInt32* aReturn);
    PRInt32           SetValueString(const nsAString& subkey, const nsAString& valname, const nsAString& value, PRInt32* aReturn);
    PRInt32           GetValueString(const nsString& subkey, const nsString& valname, nsString* aReturn);
    PRInt32           SetValueNumber(const nsString& subkey, const nsString& valname, PRInt32 value, PRInt32* aReturn);
    PRInt32           GetValueNumber(const nsString& subkey, const nsString& valname, PRInt32* aReturn);
    PRInt32           SetValue(const nsString& subkey, const nsString& valname, nsWinRegValue* value, PRInt32* aReturn);
    PRInt32           GetValue(const nsString& subkey, const nsString& valname, nsWinRegValue** aReturn);
    PRInt32           EnumValueNames(const nsString& keyname, PRInt32 index, nsString &aReturn);
    PRInt32           EnumKeys(const nsString& keyname, PRInt32 index, nsString &aReturn);

    nsInstall*        InstallObject(void);

    PRInt32           KeyExists(const nsString& subkey, PRInt32* aReturn);
    PRInt32           ValueExists(const nsString& subkey, const nsString& valname, PRInt32* aReturn);
    PRInt32           IsKeyWritable(const nsString& subkey, PRInt32* aReturn);
    PRInt32           PrepareCreateKey(PRInt32 root, const nsString& subkey, PRInt32* aReturn);
    PRInt32           PrepareDeleteKey(PRInt32 root, const nsString& subkey, PRInt32* aReturn);
    PRInt32           PrepareDeleteValue(PRInt32 root, const nsString& subkey, const nsString& valname, PRInt32* aReturn);
    PRInt32           PrepareSetValueString(PRInt32 root, const nsString& subkey, PRInt32* aReturn);
    PRInt32           PrepareSetValueNumber(PRInt32 root, const nsString& subkey, PRInt32* aReturn);
    PRInt32           PrepareSetValue(PRInt32 root, const nsString& subkey, PRInt32* aReturn);

    PRInt32           FinalCreateKey(PRInt32 root, const nsString& subkey, const nsString& classname, PRInt32* aReturn);
    PRInt32           FinalDeleteKey(PRInt32 root, const nsString& subkey, PRInt32* aReturn);
    PRInt32           FinalDeleteValue(PRInt32 root, const nsString& subkey, const nsString& valname, PRInt32* aReturn);
    PRInt32           FinalSetValueString(PRInt32 root, const nsString& subkey, const nsString& valname, const nsString& value, PRInt32* aReturn);
    PRInt32           FinalSetValueNumber(PRInt32 root, const nsString& subkey, const nsString& valname, PRInt32 value, PRInt32* aReturn);
    PRInt32           FinalSetValue(PRInt32 root, const nsString& subkey, const nsString& valname, nsWinRegValue* value, PRInt32* aReturn);

    
  private:
    
    /* Private Fields */
    PRInt32    mRootKey;
    nsInstall* mInstallObject;

    /* Private Methods */
    PRBool            NativeKeyExists(const nsString& subkey);
    PRBool            NativeValueExists(const nsString& subkey, const nsString& valname);
    PRBool            NativeIsKeyWritable(const nsString& subkey);
    PRInt32           NativeCreateKey(const nsString& subkey, const nsString& classname);
    PRInt32           NativeDeleteKey(const nsString& subkey);
    PRInt32           NativeDeleteValue(const nsString& subkey, const nsString& valname);

    PRInt32           NativeSetValueString(const nsString& subkey, const nsString& valname, const nsString& value);
    PRInt32           NativeGetValueString(const nsString& subkey, const nsString& valname, nsString* aReturn);
    PRInt32           NativeSetValueNumber(const nsString& subkey, const nsString& valname, PRInt32 value);
    PRInt32           NativeGetValueNumber(const nsString& subkey, const nsString& valname, PRInt32* aReturn);

    PRInt32           NativeSetValue(const nsString& subkey, const nsString& valname, nsWinRegValue* value);
    nsWinRegValue*    NativeGetValue(const nsString& subkey, const nsString& valname);
};

#endif /* __NS_WINREG_H__ */

