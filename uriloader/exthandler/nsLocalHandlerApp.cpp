/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:cin:
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is 
 * the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Mosedale <dmose@mozilla.org>
 *   Myk Melez <myk@mozilla.org>
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

#include "nsLocalHandlerApp.h"

// XXX why does nsMIMEInfoImpl have a threadsafe nsISupports?  do we need one 
// here too?
NS_IMPL_ISUPPORTS2(nsLocalHandlerApp, nsILocalHandlerApp, nsIHandlerApp)

////////////////////////////////////////////////////////////////////////////////
//// nsIHandlerApp

NS_IMETHODIMP nsLocalHandlerApp::GetName(nsAString& aName)
{
  if (mName.IsEmpty() && mExecutable) {
    // Don't want to cache this, just in case someone resets the app
    // without changing the description....
    mExecutable->GetLeafName(aName);
  } else {
    aName.Assign(mName);
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsLocalHandlerApp::SetName(const nsAString & aName)
{
  mName.Assign(aName);

  return NS_OK;
}

NS_IMETHODIMP
nsLocalHandlerApp::Equals(nsIHandlerApp *aHandlerApp, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(aHandlerApp);

  // If the handler app isn't a local handler app, then it's not the same app.
  nsCOMPtr <nsILocalHandlerApp> localHandlerApp = do_QueryInterface(aHandlerApp);
  if (!localHandlerApp) {
    *_retval = PR_FALSE;
    return NS_OK;
  }

  // If either handler app doesn't have an executable, then they aren't
  // the same app.
  nsCOMPtr<nsIFile> executable;
  nsresult rv = localHandlerApp->GetExecutable(getter_AddRefs(executable));
  if (NS_FAILED(rv) || !executable || !mExecutable) {
    *_retval = PR_FALSE;
    return NS_OK;
  }

  return executable->Equals(mExecutable, _retval);
}


////////////////////////////////////////////////////////////////////////////////
//// nsILocalHandlerApp

NS_IMETHODIMP nsLocalHandlerApp::GetExecutable(nsIFile **aExecutable)
{
  NS_IF_ADDREF(*aExecutable = mExecutable);
  return NS_OK;
}

NS_IMETHODIMP nsLocalHandlerApp::SetExecutable(nsIFile *aExecutable)
{
  mExecutable = aExecutable;
  return NS_OK;
}

