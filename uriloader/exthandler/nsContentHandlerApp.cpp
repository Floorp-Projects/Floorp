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
 * The Original Code is libcontentaction integration.
 *
 * The Initial Developer of the Original Code is
 * the Nokia Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Egor Starkov <starkov.egor@gmail.com>
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

