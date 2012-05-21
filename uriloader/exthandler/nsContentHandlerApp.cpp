/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:cin:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentHandlerApp.h"
#include "nsIURI.h"
#include "nsIClassInfoImpl.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#define NS_CONTENTHANDLER_CID \
{ 0x43ec2c82, 0xb9db, 0x4835, {0x80, 0x3f, 0x64, 0xc9, 0x72, 0x5a, 0x70, 0x28 } }

NS_IMPL_CLASSINFO(nsContentHandlerApp, NULL, 0, NS_CONTENTHANDLER_CID)
NS_IMPL_ISUPPORTS1_CI(nsContentHandlerApp, nsIHandlerApp)

nsContentHandlerApp::nsContentHandlerApp(nsString aName, nsCString aType,
                                         ContentAction::Action& aAction) :
  mName(aName),
  mType(aType),
  mAction(aAction)
{
}

////////////////////////////////////////////////////////////////////////////////
//// nsIHandlerInfo

NS_IMETHODIMP nsContentHandlerApp::GetName(nsAString& aName)
{
  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP nsContentHandlerApp::SetName(const nsAString& aName)
{
  mName.Assign(aName);
  return NS_OK;
}

NS_IMETHODIMP nsContentHandlerApp::Equals(nsIHandlerApp *aHandlerApp, bool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsContentHandlerApp::GetDetailedDescription(nsAString& aDetailedDescription)
{
  aDetailedDescription.Assign(mDetailedDescription);
  return NS_OK;
}

NS_IMETHODIMP nsContentHandlerApp::SetDetailedDescription(const nsAString& aDetailedDescription)
{
  mDetailedDescription.Assign(aDetailedDescription);
  return NS_OK;
}

NS_IMETHODIMP
nsContentHandlerApp::LaunchWithURI(nsIURI *aURI,
                                   nsIInterfaceRequestor *aWindowContext)
{
  nsCAutoString spec;
  nsresult rv = aURI->GetAsciiSpec(spec);
  NS_ENSURE_SUCCESS(rv,rv);
  const char* url = spec.get();

  QList<ContentAction::Action> actions = 
    ContentAction::Action::actionsForFile(QUrl(url), QString(mType.get()));
  for (int i = 0; i < actions.size(); ++i) {
    if (actions[i].name() == QString((QChar*)mName.get(), mName.Length())) {
      actions[i].trigger();
      break;
    }
  }

  return NS_OK;
}

