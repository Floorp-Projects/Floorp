/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 */

#include "nsPrefService.h"
#include "nsPrefBranch.h"


nsPrefService::nsPrefService()
{
  nsPrefBranch *rootBranch = new nsPrefBranch("");
  
  mRootBranch = (nsIPrefBranch*)rootBranch;
}

nsPrefService::~nsPrefService()
{


}

NS_IMPL_ISUPPORTS2(nsPrefService, nsIPrefService, nsIPrefBranch);

NS_IMETHODIMP
nsPrefService::GetBranch(const char *root, nsIPrefBranch ** aResult)
{
  nsresult rv;

  // XXX todo - cache this stuff and allow consumers to share
  // branches (hold weak references I think)
  nsPrefBranch* prefBranch = new nsPrefBranch(root);
  
  rv = prefBranch->QueryInterface(NS_GET_IID(nsIPrefBranch),
                                  (void **)aResult);

  return rv;
}

NS_IMETHODIMP
nsPrefService::StartUp(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPrefService::ReadUserPrefs(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPrefService::ReadUserPrefsFrom(nsIFileSpec *)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPrefService::ShutDown(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPrefService::ReadUserJSFile(nsIFileSpec *)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
