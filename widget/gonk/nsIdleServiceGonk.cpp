/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIdleServiceGonk.h"
#include "nsIServiceManager.h"

NS_IMPL_ISUPPORTS2(nsIdleServiceGonk, nsIIdleService, nsIdleService)

bool
nsIdleServiceGonk::PollIdleTime(PRUint32 *aIdleTime)
{
    return false;
}

bool
nsIdleServiceGonk::UsePollMode()
{
    return false;
}
