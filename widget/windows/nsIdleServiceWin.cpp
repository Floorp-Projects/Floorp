/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIdleServiceWin.h"
#include <windows.h>

NS_IMPL_ISUPPORTS_INHERITED0(nsIdleServiceWin, nsIdleService)

bool
nsIdleServiceWin::PollIdleTime(uint32_t *aIdleTime)
{
    LASTINPUTINFO inputInfo;
    inputInfo.cbSize = sizeof(inputInfo);
    if (!::GetLastInputInfo(&inputInfo))
        return false;

    *aIdleTime = SAFE_COMPARE_EVEN_WITH_WRAPPING(GetTickCount(), inputInfo.dwTime);

    return true;
}

bool
nsIdleServiceWin::UsePollMode()
{
    return true;
}
