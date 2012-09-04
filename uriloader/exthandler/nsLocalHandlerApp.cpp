/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:cin:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

  *_retval = false;

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
    *_retval = true;
    return NS_OK;
  }

  // At least one is set so they are not equal
  if (!mExecutable || !executable)
    return NS_OK;

  // Check the command line parameter list lengths
  uint32_t len;
  localHandlerApp->GetParameterCount(&len);
  if (mParameters.Length() != len)
    return NS_OK;

  // Check the command line params lists
  for (uint32_t idx = 0; idx < mParameters.Length(); idx++) {
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
  nsAutoCString spec;
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

  return process->Run(false, &string, 1);
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
nsLocalHandlerApp::GetParameterCount(uint32_t *aParameterCount)
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
nsLocalHandlerApp::GetParameter(uint32_t parameterIndex, nsAString & _retval)
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
