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
#include "nsIURI.h"
#include "nsIProcess.h"

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
nsLocalHandlerApp::SetDetailedDescription(const nsAString & aDescription)
{
  mDetailedDescription.Assign(aDescription);

  return NS_OK;
}

NS_IMETHODIMP
nsLocalHandlerApp::GetDetailedDescription(nsAString& aDescription)
{
  aDescription.Assign(mDetailedDescription);
  
  return NS_OK;
}

NS_IMETHODIMP
nsLocalHandlerApp::Equals(nsIHandlerApp *aHandlerApp, bool *_retval)
{
  NS_ENSURE_ARG_POINTER(aHandlerApp);

  *_retval = PR_FALSE;

  // If the handler app isn't a local handler app, then it's not the same app.
  nsCOMPtr <nsILocalHandlerApp> localHandlerApp = do_QueryInterface(aHandlerApp);
  if (!localHandlerApp)
    return NS_OK;

  // If either handler app doesn't have an executable, then they aren't
  // the same app.
  nsCOMPtr<nsIFile> executable;
  nsresult rv = localHandlerApp->GetExecutable(getter_AddRefs(executable));
  if (NS_FAILED(rv))
    return rv;

  // Equality for two empty nsIHandlerApp
  if (!executable && !mExecutable) {
    *_retval = PR_TRUE;
    return NS_OK;
  }

  // At least one is set so they are not equal
  if (!mExecutable || !executable)
    return NS_OK;

  // Check the command line parameter list lengths
  PRUint32 len;
  localHandlerApp->GetParameterCount(&len);
  if (mParameters.Length() != len)
    return NS_OK;

  // Check the command line params lists
  for (PRUint32 idx = 0; idx < mParameters.Length(); idx++) {
    nsAutoString param;
    if (NS_FAILED(localHandlerApp->GetParameter(idx, param)) ||
        !param.Equals(mParameters[idx]))
      return NS_OK;
  }

  return executable->Equals(mExecutable, _retval);
}

NS_IMETHODIMP
nsLocalHandlerApp::LaunchWithURI(nsIURI *aURI,
                                 nsIInterfaceRequestor *aWindowContext)
{
  // pass the entire URI to the handler.
  nsCAutoString spec;
  aURI->GetAsciiSpec(spec);
  return LaunchWithIProcess(spec);
}

nsresult
nsLocalHandlerApp::LaunchWithIProcess(const nsCString& aArg)
{
  nsresult rv;
  nsCOMPtr<nsIProcess> process = do_CreateInstance(NS_PROCESS_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  if (NS_FAILED(rv = process->Init(mExecutable)))
    return rv;

  const char *string = aArg.get();

  return process->Run(PR_FALSE, &string, 1);
}

////////////////////////////////////////////////////////////////////////////////
//// nsILocalHandlerApp

/* attribute nsIFile executable; */
NS_IMETHODIMP
nsLocalHandlerApp::GetExecutable(nsIFile **aExecutable)
{
  NS_IF_ADDREF(*aExecutable = mExecutable);
  return NS_OK;
}

NS_IMETHODIMP
nsLocalHandlerApp::SetExecutable(nsIFile *aExecutable)
{
  mExecutable = aExecutable;
  return NS_OK;
}

/* readonly attribute unsigned long parameterCount; */
NS_IMETHODIMP
nsLocalHandlerApp::GetParameterCount(PRUint32 *aParameterCount)
{
  *aParameterCount = mParameters.Length();
  return NS_OK;
}

/* void clearParameters (); */
NS_IMETHODIMP
nsLocalHandlerApp::ClearParameters()
{
  mParameters.Clear();
  return NS_OK;
}

/* void appendParameter (in AString param); */
NS_IMETHODIMP
nsLocalHandlerApp::AppendParameter(const nsAString & aParam)
{
  mParameters.AppendElement(aParam);
  return NS_OK;
}

/* AString getParameter (in unsigned long parameterIndex); */
NS_IMETHODIMP
nsLocalHandlerApp::GetParameter(PRUint32 parameterIndex, nsAString & _retval)
{
  if (mParameters.Length() <= parameterIndex)
    return NS_ERROR_INVALID_ARG;

  _retval.Assign(mParameters[parameterIndex]);
  return NS_OK;
}

/* boolean parameterExists (in AString param); */
NS_IMETHODIMP
nsLocalHandlerApp::ParameterExists(const nsAString & aParam, bool *_retval)
{
  *_retval = mParameters.Contains(aParam);
  return NS_OK;
}
