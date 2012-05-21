/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeConnectionHelper.h"

#if defined(MOZ_PLATFORM_MAEMO)
#include "nsAutodialMaemo.h"
#else
#include "nsAutodialWin.h"
#endif

#include "nsIOService.h"

//-----------------------------------------------------------------------------
// API typically invoked on the socket transport thread
//-----------------------------------------------------------------------------

bool
nsNativeConnectionHelper::OnConnectionFailed(const PRUnichar* hostName)
{
  // On mobile platforms, instead of relying on the link service, we
  // should ask the dialer directly.  This allows the dialer to update
  // link status more forcefully rather than passively watching link
  // status changes.
#if !defined(MOZ_PLATFORM_MAEMO)
    if (gIOService->IsLinkUp())
        return false;
#endif

    nsAutodial autodial;
    if (autodial.ShouldDialOnNetworkError())
        return NS_SUCCEEDED(autodial.DialDefault(hostName));

    return false;
}

bool
nsNativeConnectionHelper::IsAutodialEnabled()
{
    nsAutodial autodial;
    return autodial.Init() == NS_OK && autodial.ShouldDialOnNetworkError();
}
