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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Alec Flett <alecf@netscape.com>
 * Brian Nesse <bnesse@netscape.com>
 */

#include "nsCOMPtr.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsIPrefLocalizedString.h"
#include "nsISecurityPref.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsWeakReference.h"

class nsPrefBranch : public nsIPrefBranch,
                     public nsIPrefBranchInternal,
                     public nsISecurityPref,
                     public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPREFBRANCH
  NS_DECL_NSIPREFBRANCHINTERNAL
  NS_DECL_NSISECURITYPREF

  nsPrefBranch(const char *aPrefRoot, PRBool aDefaultBranch);
  virtual ~nsPrefBranch();

protected:
  nsPrefBranch()	/* disallow use of this constructer */
    { };

  nsresult   GetDefaultFromPropertiesFile(const char *aPrefName, PRUnichar **return_buf);
  const char *getPrefName(const char *aPrefName);
  nsresult   getValidatedPrefName(const char *aPrefName, const char **_retval);

private:
  PRInt32         mPrefRootLength;
  nsAutoVoidArray *mObservers;
  nsCString       mPrefRoot;
  nsCStringArray  mObserverDomains;
  PRBool          mIsDefault;

};


class nsPrefLocalizedString : public nsIPrefLocalizedString,
                              public nsISupportsWString
{
public:
  nsPrefLocalizedString();
  virtual ~nsPrefLocalizedString();

  NS_DECL_ISUPPORTS
  NS_FORWARD_NSISUPPORTSWSTRING(mUnicodeString->)
  NS_FORWARD_NSISUPPORTSPRIMITIVE(mUnicodeString->)

  nsresult Init();

private:
  nsCOMPtr<nsISupportsWString> mUnicodeString;
};

