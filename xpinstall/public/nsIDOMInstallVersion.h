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

#ifndef nsIDOMInstallVersion_h__
#define nsIDOMInstallVersion_h__

#include "nsISupports.h"
#include "nsString.h"
#include "nsIScriptContext.h"

class nsIDOMInstallVersion;

#define NS_IDOMINSTALLVERSION_IID \
 { 0x18c2f986, 0xb09f, 0x11d2, \
  {0xbc, 0xde, 0x00, 0x80, 0x5f, 0x0e, 0x13, 0x53}} 

class nsIDOMInstallVersion : public nsISupports {
public:
  static const nsIID& IID() { static nsIID iid = NS_IDOMINSTALLVERSION_IID; return iid; }
  enum {
    SU_EQUAL = 0,
    SU_BLD_DIFF = 1,
    SU_BLD_DIFF_MINUS = -1,
    SU_REL_DIFF = 2,
    SU_REL_DIFF_MINUS = -2,
    SU_MINOR_DIFF = 3,
    SU_MINOR_DIFF_MINUS = -3,
    SU_MAJOR_DIFF = 4,
    SU_MAJOR_DIFF_MINUS = -4
  };

  NS_IMETHOD    GetMajor(PRInt32* aMajor)=0;
  NS_IMETHOD    SetMajor(PRInt32 aMajor)=0;

  NS_IMETHOD    GetMinor(PRInt32* aMinor)=0;
  NS_IMETHOD    SetMinor(PRInt32 aMinor)=0;

  NS_IMETHOD    GetRelease(PRInt32* aRelease)=0;
  NS_IMETHOD    SetRelease(PRInt32 aRelease)=0;

  NS_IMETHOD    GetBuild(PRInt32* aBuild)=0;
  NS_IMETHOD    SetBuild(PRInt32 aBuild)=0;

  NS_IMETHOD    Init(const nsString& aVersionString)=0;

  NS_IMETHOD    ToString(nsString& aReturn)=0;

  NS_IMETHOD    CompareTo(nsIDOMInstallVersion* aVersionObject, PRInt32* aReturn)=0;
};


#define NS_DECL_IDOMINSTALLVERSION   \
  NS_IMETHOD    GetMajor(PRInt32* aMajor);  \
  NS_IMETHOD    SetMajor(PRInt32 aMajor);  \
  NS_IMETHOD    GetMinor(PRInt32* aMinor);  \
  NS_IMETHOD    SetMinor(PRInt32 aMinor);  \
  NS_IMETHOD    GetRelease(PRInt32* aRelease);  \
  NS_IMETHOD    SetRelease(PRInt32 aRelease);  \
  NS_IMETHOD    GetBuild(PRInt32* aBuild);  \
  NS_IMETHOD    SetBuild(PRInt32 aBuild);  \
  NS_IMETHOD    Init(const nsString& aVersionString);  \
  NS_IMETHOD    ToString(nsString& aReturn);  \
  NS_IMETHOD    CompareTo(nsIDOMInstallVersion* aVersionObject, PRInt32* aReturn);  \



#define NS_FORWARD_IDOMINSTALLVERSION(_to)  \
  NS_IMETHOD    GetMajor(PRInt32* aMajor) { return _to##GetMajor(aMajor); } \
  NS_IMETHOD    SetMajor(PRInt32 aMajor) { return _to##SetMajor(aMajor); } \
  NS_IMETHOD    GetMinor(PRInt32* aMinor) { return _to##GetMinor(aMinor); } \
  NS_IMETHOD    SetMinor(PRInt32 aMinor) { return _to##SetMinor(aMinor); } \
  NS_IMETHOD    GetRelease(PRInt32* aRelease) { return _to##GetRelease(aRelease); } \
  NS_IMETHOD    SetRelease(PRInt32 aRelease) { return _to##SetRelease(aRelease); } \
  NS_IMETHOD    GetBuild(PRInt32* aBuild) { return _to##GetBuild(aBuild); } \
  NS_IMETHOD    SetBuild(PRInt32 aBuild) { return _to##SetBuild(aBuild); } \
  NS_IMETHOD    Init(const nsString& aVersionString) { return _to##Init(aVersionString); }  \
  NS_IMETHOD    ToString(nsString& aReturn) { return _to##ToString(aReturn); }  \
  NS_IMETHOD    CompareTo(nsIDOMInstallVersion* aVersionObject, PRInt32* aReturn) { return _to##CompareTo(aVersionObject, aReturn); }  \


extern nsresult NS_InitInstallVersionClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptInstallVersion(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMInstallVersion_h__
