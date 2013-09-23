/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIdleServiceGTK_h__
#define nsIdleServiceGTK_h__

#include "nsIdleService.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <gdk/gdkx.h>

typedef struct {
    Window window;              // Screen saver window
    int state;                  // ScreenSaver(Off,On,Disabled)
    int kind;                   // ScreenSaver(Blanked,Internal,External)
    unsigned long til_or_since; // milliseconds since/til screensaver kicks in
    unsigned long idle;         // milliseconds idle
    unsigned long event_mask;   // event stuff
} XScreenSaverInfo;

class nsIdleServiceGTK : public nsIdleService
{
public:
    NS_DECL_ISUPPORTS_INHERITED

    bool PollIdleTime(uint32_t* aIdleTime);

    static already_AddRefed<nsIdleServiceGTK> GetInstance()
    {
        nsRefPtr<nsIdleServiceGTK> idleService =
            nsIdleService::GetInstance().downcast<nsIdleServiceGTK>();
        if (!idleService) {
            idleService = new nsIdleServiceGTK();
        }

        return idleService.forget();
    }

private:
    ~nsIdleServiceGTK();
    XScreenSaverInfo* mXssInfo;

protected:
    nsIdleServiceGTK();
    bool UsePollMode();
};

#endif // nsIdleServiceGTK_h__
