/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
/* AUTO-GENERATED. DO NOT EDIT!!! */

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


extern nsresult NS_InitInstallTriggerGlobalClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptInstallTriggerGlobal(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMInstallTriggerGlobal_h__
