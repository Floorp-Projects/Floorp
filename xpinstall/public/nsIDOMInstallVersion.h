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
  static const nsIID& GetIID() { static nsIID iid = NS_IDOMINSTALLVERSION_IID; return iid; }
  enum {
    EQUAL = 0,
    BLD_DIFF = 1,
    BLD_DIFF_MINUS = -1,
    REL_DIFF = 2,
    REL_DIFF_MINUS = -2,
    MINOR_DIFF = 3,
    MINOR_DIFF_MINUS = -3,
    MAJOR_DIFF = 4,
    MAJOR_DIFF_MINUS = -4
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
  NS_IMETHOD    CompareTo(const nsString& aString, PRInt32* aReturn)=0;
  NS_IMETHOD    CompareTo(PRInt32 aMajor, PRInt32 aMinor, PRInt32 aRelease, PRInt32 aBuild, PRInt32* aReturn)=0;
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
  NS_IMETHOD    CompareTo(const nsString& aString, PRInt32* aReturn);  \
  NS_IMETHOD    CompareTo(PRInt32 aMajor, PRInt32 aMinor, PRInt32 aRelease, PRInt32 aBuild, PRInt32* aReturn);  \



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
  NS_IMETHOD    CompareTo(const nsString& aString, PRInt32* aReturn) { return _to##CompareTo(aString, aReturn); }  \
  NS_IMETHOD    CompareTo(PRInt32 aMajor, PRInt32 aMinor, PRInt32 aRelease, PRInt32 aBuild, PRInt32* aReturn) { return _to##CompareTo(aMajor, aMinor, aRelease, aBuild, aReturn); }  \


extern nsresult NS_InitInstallVersionClass(nsIScriptContext *aContext, void **aPrototype);

extern "C" NS_DOM nsresult NS_NewScriptInstallVersion(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn);

#endif // nsIDOMInstallVersion_h__
