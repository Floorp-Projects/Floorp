/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsQtNetworkManager.h"
#include "nsAutodialQt.h"
#include "nsNetCID.h"
#include "nsCOMPtr.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIServiceManager.h"


nsAutodial::nsAutodial()
{
}

nsAutodial::~nsAutodial()
{
}

nsresult
nsAutodial::Init()
{
  return NS_OK;
}

nsresult
nsAutodial::DialDefault(const char16_t* hostName)
{
  if (nsQtNetworkManager::get()->openConnection(QString::fromUtf16((const ushort*)hostName))) {
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

bool
nsAutodial::ShouldDialOnNetworkError()
{
  if (nsQtNetworkManager::get()->isOnline()) {
    return false;
  }

  return true;
}
