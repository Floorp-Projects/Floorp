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
 * Brian Nesse <bnesse@netscape.com>
 */

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefBranchInternal.h"
#include "nsIObserver.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"

class nsIFile;

class nsPrefService : public nsIPrefService,
                      public nsIObserver,
                      public nsIPrefBranch,
                      public nsIPrefBranchInternal,
                      public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPREFSERVICE
  NS_FORWARD_NSIPREFBRANCH(mRootBranch->)
  NS_DECL_NSIPREFBRANCHINTERNAL
  NS_DECL_NSIOBSERVER

  nsPrefService();
  virtual ~nsPrefService();

  nsresult Init();
                           
protected:
  nsresult useDefaultPrefFile();
  nsresult useUserPrefFile();
  nsresult readConfigFile();

private:
  nsCOMPtr<nsIPrefBranch> mRootBranch;
  nsIFile*                mCurrentFile;
    
};

