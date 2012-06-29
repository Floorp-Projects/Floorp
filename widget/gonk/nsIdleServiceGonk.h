/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIdleServiceGonk_h__
#define nsIdleServiceGonk_h__

#include "nsIdleService.h"

class nsIdleServiceGonk : public nsIdleService
{
public:
    NS_DECL_ISUPPORTS_INHERITED

    bool PollIdleTime(PRUint32* aIdleTime);

    static already_AddRefed<nsIdleServiceGonk> GetInstance()
    {
        nsIdleServiceGonk* idleService =
            static_cast<nsIdleServiceGonk*>(nsIdleService::GetInstance().get());
        if (!idleService) {
            idleService = new nsIdleServiceGonk();
            NS_ADDREF(idleService);
        }

        return idleService;
    }

protected:
    nsIdleServiceGonk() { }
    virtual ~nsIdleServiceGonk() { }
    bool UsePollMode();
};

#endif // nsIdleServiceGonk_h__
