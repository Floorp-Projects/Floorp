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

#ifndef nsIDOMInstallTriggerGlobal_h__
#define nsIDOMInstallTriggerGlobal_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"
#include "nsXPITriggerInfo.h"


#define NS_IDOMINSTALLTRIGGERGLOBAL_IID \
 { 0x18c2f987, 0xb09f, 0x11d2, \
  {0xbc, 0xde, 0x00, 0x80, 0x5f, 0x0e, 0x13, 0x53}} 

class nsIDOMInstallTriggerGlobal : public nsISupports {
public:
  static const nsIID& IID() { static nsIID iid = NS_IDOMINSTALLTRIGGERGLOBAL_IID; return iid; }
  enum {
    MAJOR_DIFF = 4,
    MINOR_DIFF = 3,
    REL_DIFF = 2,
    BLD_DIFF = 1,
    EQUAL = 0
  };

  NS_IMETHOD    UpdateEnabled(PRBool* aReturn)=0;

  NS_IMETHOD    Install(nsXPITriggerInfo* aInfo, PRBool* aReturn)=0;

  NS_IMETHOD    InstallChrome(PRUint32 aType, nsXPITriggerItem* aItem, PRBool* aReturn)=0;

  NS_IMETHOD    StartSoftwareUpdate(const nsString& aURL, PRInt32 aFlags, PRBool* aReturn)=0;

  NS_IMETHOD    ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, PRInt32 aDiffLevel, const nsString& aVersion, PRInt32 aMode, PRInt32* aReturn)=0;
  NS_IMETHOD    ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, PRInt32 aDiffLevel, nsIDOMInstallVersion* aVersion, PRInt32 aMode, PRInt32* aReturn)=0;
  NS_IMETHOD    ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, nsIDOMInstallVersion* aVersion, PRInt32 aMode, PRInt32* aReturn)=0;
  NS_IMETHOD    ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, const nsString& aVersion, PRInt32 aMode, PRInt32* aReturn)=0;
  NS_IMETHOD    ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, const nsString& aVersion, PRInt32* aReturn)=0;
  NS_IMETHOD    ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, nsIDOMInstallVersion* aVersion, PRInt32* aReturn)=0;

  NS_IMETHOD    CompareVersion(const nsString& aRegName, PRInt32 aMajor, PRInt32 aMinor, PRInt32 aRelease, PRInt32 aBuild, PRInt32* aReturn)=0;
  NS_IMETHOD    CompareVersion(const nsString& aRegName, const nsString& aVersion, PRInt32* aReturn)=0;
  NS_IMETHOD    CompareVersion(const nsString& aRegName, nsIDOMInstallVersion* aVersion, PRInt32* aReturn)=0;

  NS_IMETHOD    GetVersion(const nsString& component, nsString& version)=0;

};


#define NS_DECL_IDOMINSTALLTRIGGERGLOBAL   \
  NS_IMETHOD    UpdateEnabled(PRBool* aReturn);  \
  NS_IMETHOD    Install(nsXPITriggerInfo* aInfo, PRBool* aReturn); \
  NS_IMETHOD    StartSoftwareUpdate(const nsString& aURL, PRInt32 aFlags, PRInt32* aReturn);  \
  NS_IMETHOD    StartSoftwareUpdate(const nsString& aURL, PRInt32* aReturn);  \
  NS_IMETHOD    ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, PRInt32 aDiffLevel, const nsString& aVersion, PRInt32 aMode, PRInt32* aReturn);  \
  NS_IMETHOD    ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, PRInt32 aDiffLevel, nsIDOMInstallVersion* aVersion, PRInt32 aMode, PRInt32* aReturn);  \
  NS_IMETHOD    ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, nsIDOMInstallVersion* aVersion, PRInt32 aMode, PRInt32* aReturn);  \
  NS_IMETHOD    ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, const nsString& aVersion, PRInt32 aMode, PRInt32* aReturn);  \
  NS_IMETHOD    ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, const nsString& aVersion, PRInt32* aReturn);  \
  NS_IMETHOD    ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, nsIDOMInstallVersion* aVersion, PRInt32* aReturn);  \
  NS_IMETHOD    CompareVersion(const nsString& aRegName, PRInt32 aMajor, PRInt32 aMinor, PRInt32 aRelease, PRInt32 aBuild, PRInt32* aReturn);  \
  NS_IMETHOD    CompareVersion(const nsString& aRegName, const nsString& aVersion, PRInt32* aReturn);  \
  NS_IMETHOD    CompareVersion(const nsString& aRegName, nsIDOMInstallVersion* aVersion, PRInt32* aReturn);  \
  NS_IMETHOD    GetVersion(const nsString& component, nsString& version); \



#define NS_FORWARD_IDOMINSTALLTRIGGERGLOBAL(_to)  \
  NS_IMETHOD    UpdateEnabled(PRBool* aReturn) { return _to##UpdateEnabled(aReturn); }  \
  NS_IMETHOD    Install(nsXPITriggerInfo* aInfo, PRBool* aReturn) { return _to##Install(aInfo,aReturn); } \
  NS_IMETHOD    StartSoftwareUpdate(const nsString& aURL, PRInt32 aFlags, PRInt32* aReturn) { return _to##StartSoftwareUpdate(aURL, aFlags, aReturn); }  \
  NS_IMETHOD    StartSoftwareUpdate(const nsString& aURL, PRInt32* aReturn) { return _to##StartSoftwareUpdate(aURL, aReturn); }  \
  NS_IMETHOD    ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, PRInt32 aDiffLevel, const nsString& aVersion, PRInt32 aMode, PRInt32* aReturn) { return _to##ConditionalSoftwareUpdate(aURL, aRegName, aDiffLevel, aVersion, aMode, aReturn); }  \
  NS_IMETHOD    ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, PRInt32 aDiffLevel, nsIDOMInstallVersion* aVersion, PRInt32 aMode, PRInt32* aReturn) { return _to##ConditionalSoftwareUpdate(aURL, aRegName, aDiffLevel, aVersion, aMode, aReturn); }  \
  NS_IMETHOD    ConditionalSoftwareUpdate(const nsString& aURL, nsIDOMInstallVersion* aRegName, const nsString& aVersion, PRInt32 aMode, PRInt32* aReturn) { return _to##ConditionalSoftwareUpdate(aURL, aDiffLevel, aVersion, aMode, aReturn); }  \
  NS_IMETHOD    ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, const nsString& aVersion, PRInt32 aMode, PRInt32* aReturn) { return _to##ConditionalSoftwareUpdate(aURL, aDiffLevel, aVersion, aMode, aReturn); }  \
  NS_IMETHOD    ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, const nsString& aVersion, PRInt32* aReturn) { return _to##ConditionalSoftwareUpdate(aURL, aDiffLevel, aVersion, aReturn); }  \
  NS_IMETHOD    ConditionalSoftwareUpdate(const nsString& aURL, const nsString& aRegName, nsIDOMInstallVersion* aVersion, PRInt32* aReturn) { return _to##ConditionalSoftwareUpdate(aURL, aDiffLevel, aVersion, aReturn); }  \
  NS_IMETHOD    CompareVersion(const nsString& aRegName, PRInt32 aMajor, PRInt32 aMinor, PRInt32 aRelease, PRInt32 aBuild, PRInt32* aReturn) { return _to##CompareVersion(aRegName, aMajor, aMinor, aRelease, aBuild, aReturn); }  \
  NS_IMETHOD    CompareVersion(const nsString& aRegName, const nsString& aVersion, PRInt32* aReturn) { return _to##CompareVersion(aRegName, aVersion, aReturn); }  \
  NS_IMETHOD    CompareVersion(const nsString& aRegName, nsIDOMInstallVersion* aVersion, PRInt32* aReturn) { return _to##CompareVersion(aRegName, aVersion, aReturn); }  \
  NS_IMETHOD    GetVersion(const nsString& component, nsString& version); { return _to##GetVersion(component, version); }  \


extern nsresult NS_InitInstallTriggerGlobalClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptInstallTriggerGlobal(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMInstallTriggerGlobal_h__
