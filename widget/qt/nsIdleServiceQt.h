/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIdleServiceQt_h__
#define nsIdleServiceQt_h__

#include "nsIdleService.h"

#if !defined(MOZ_PLATFORM_MAEMO) && defined(MOZ_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct {
    Window window;              // Screen saver window
    int state;                  // ScreenSaver(Off,On,Disabled)
    int kind;                   // ScreenSaver(Blanked,Internal,External)
    unsigned long til_or_since; // milliseconds since/til screensaver kicks in
    unsigned long idle;         // milliseconds idle
    unsigned long event_mask;   // event stuff
} XScreenSaverInfo;
#endif

class nsIdleServiceQt : public nsIdleService
{
public:
    NS_DECL_ISUPPORTS
    nsIdleServiceQt();

    bool PollIdleTime(PRUint32* aIdleTime);

private:
    ~nsIdleServiceQt();
#if !defined(MOZ_PLATFORM_MAEMO) && defined(MOZ_X11)
    XScreenSaverInfo* mXssInfo;
#endif

protected:
    bool UsePollMode();
};

#endif // nsIdleServiceQt_h__
