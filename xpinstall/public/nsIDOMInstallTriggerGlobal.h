/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef nsIDOMInstallTriggerGlobal_h__
#define nsIDOMInstallTriggerGlobal_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsXPITriggerInfo.h"
#include "nsIXPIInstallInfo.h"


#define NS_IDOMINSTALLTRIGGERGLOBAL_IID \
 { 0xe8c7941c, 0xaaa0, 0x4faf, \
  {0x83, 0xe8, 0x01, 0xbe, 0x8b, 0xbe, 0x8a, 0x57}}

class nsIDOMInstallTriggerGlobal : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOMINSTALLTRIGGERGLOBAL_IID)
  enum {
    NOT_FOUND = -5,
    MAJOR_DIFF = 4,
    MINOR_DIFF = 3,
    REL_DIFF = 2,
    BLD_DIFF = 1,
    EQUAL = 0
  };

  NS_IMETHOD    GetOriginatingURI(nsIScriptGlobalObject* aGlobalObject, nsIURI * *aUri)=0;

  NS_IMETHOD    UpdateEnabled(nsIScriptGlobalObject* aGlobalObject, PRBool aUseWhitelist, PRBool* aReturn)=0;

  NS_IMETHOD    UpdateEnabled(nsIURI* aURI, PRBool aUseWhitelist, PRBool* aReturn)=0;

  NS_IMETHOD    StartInstall(nsIXPIInstallInfo* aInstallInfo, PRBool* aReturn)=0;

  NS_IMETHOD    Install(nsIScriptGlobalObject* globalObject, nsXPITriggerInfo* aInfo, PRBool* aReturn)=0;

  NS_IMETHOD    InstallChrome(nsIScriptGlobalObject* globalObject, PRUint32 aType, nsXPITriggerItem* aItem, PRBool* aReturn)=0;

  NS_IMETHOD    StartSoftwareUpdate(nsIScriptGlobalObject* globalObject, const nsString& aURL, PRInt32 aFlags, PRBool* aReturn)=0;

  NS_IMETHOD    CompareVersion(const nsString& aRegName, PRInt32 aMajor, PRInt32 aMinor, PRInt32 aRelease, PRInt32 aBuild, PRInt32* aReturn)=0;
  NS_IMETHOD    CompareVersion(const nsString& aRegName, const nsString& aVersion, PRInt32* aReturn)=0;
  NS_IMETHOD    CompareVersion(const nsString& aRegName, nsIDOMInstallVersion* aVersion, PRInt32* aReturn)=0;

  NS_IMETHOD    GetVersion(const nsString& component, nsString& version)=0;

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDOMInstallTriggerGlobal,
                              NS_IDOMINSTALLTRIGGERGLOBAL_IID)

extern nsresult NS_InitInstallTriggerGlobalClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" nsresult NS_NewScriptInstallTriggerGlobal(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMInstallTriggerGlobal_h__
