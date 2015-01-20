/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeConnectionHelper.h"
#include "nsAutodialWin.h"
#include "nsIOService.h"

//-----------------------------------------------------------------------------
// API typically invoked on the socket transport thread
//-----------------------------------------------------------------------------

bool
nsNativeConnectionHelper::OnConnectionFailed(const char16_t* hostName)
{
    if (gIOService->IsLinkUp())
        return false;

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
