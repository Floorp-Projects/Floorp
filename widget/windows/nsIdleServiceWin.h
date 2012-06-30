/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIdleServiceWin_h__
#define nsIdleServiceWin_h__

#include "nsIdleService.h"


/* NOTE: Compare of GetTickCount() could overflow.  This corrects for
* overflow situations.
***/
#ifndef SAFE_COMPARE_EVEN_WITH_WRAPPING
#define SAFE_COMPARE_EVEN_WITH_WRAPPING(A, B) (((int)((long)A - (long)B) & 0xFFFFFFFF))
#endif


class nsIdleServiceWin : public nsIdleService
{
public:
    NS_DECL_ISUPPORTS_INHERITED

    bool PollIdleTime(PRUint32* aIdleTime);

    static already_AddRefed<nsIdleServiceWin> GetInstance()
    {
        nsIdleServiceWin* idleService =
            static_cast<nsIdleServiceWin*>(nsIdleService::GetInstance().get());
        if (!idleService) {
            idleService = new nsIdleServiceWin();
            NS_ADDREF(idleService);
        }
        
        return idleService;
    }

protected:
    nsIdleServiceWin() { }
    virtual ~nsIdleServiceWin() { }
    bool UsePollMode();
};

#endif // nsIdleServiceWin_h__
