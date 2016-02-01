/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <gtk/gtk.h>

#include "nsIdleServiceGTK.h"
#include "nsIServiceManager.h"
#include "nsDebug.h"
#include "prlink.h"
#include "mozilla/Logging.h"

using mozilla::LogLevel;

static PRLogModuleInfo* sIdleLog = nullptr;

typedef bool (*_XScreenSaverQueryExtension_fn)(Display* dpy, int* event_base,
                                                 int* error_base);

typedef XScreenSaverInfo* (*_XScreenSaverAllocInfo_fn)(void);

typedef void (*_XScreenSaverQueryInfo_fn)(Display* dpy, Drawable drw,
                                          XScreenSaverInfo *info);

static bool sInitialized = false;
static _XScreenSaverQueryExtension_fn _XSSQueryExtension = nullptr;
static _XScreenSaverAllocInfo_fn _XSSAllocInfo = nullptr;
static _XScreenSaverQueryInfo_fn _XSSQueryInfo = nullptr;

NS_IMPL_ISUPPORTS_INHERITED0(nsIdleServiceGTK, nsIdleService)

static void Initialize()
{
    if (!GDK_IS_X11_DISPLAY(gdk_display_get_default()))
        return;

    // This will leak - See comments in ~nsIdleServiceGTK().
    PRLibrary* xsslib = PR_LoadLibrary("libXss.so.1");
    if (!xsslib) // ouch.
    {
        MOZ_LOG(sIdleLog, LogLevel::Warning, ("Failed to find libXss.so!\n"));
        return;
    }

    _XSSQueryExtension = (_XScreenSaverQueryExtension_fn)
        PR_FindFunctionSymbol(xsslib, "XScreenSaverQueryExtension");
    _XSSAllocInfo = (_XScreenSaverAllocInfo_fn)
        PR_FindFunctionSymbol(xsslib, "XScreenSaverAllocInfo");
    _XSSQueryInfo = (_XScreenSaverQueryInfo_fn)
        PR_FindFunctionSymbol(xsslib, "XScreenSaverQueryInfo");

    if (!_XSSQueryExtension)
        MOZ_LOG(sIdleLog, LogLevel::Warning, ("Failed to get XSSQueryExtension!\n"));
    if (!_XSSAllocInfo)
        MOZ_LOG(sIdleLog, LogLevel::Warning, ("Failed to get XSSAllocInfo!\n"));
    if (!_XSSQueryInfo)
        MOZ_LOG(sIdleLog, LogLevel::Warning, ("Failed to get XSSQueryInfo!\n"));

    sInitialized = true;
}

nsIdleServiceGTK::nsIdleServiceGTK()
    : mXssInfo(nullptr)
{
    if (!sIdleLog)
        sIdleLog = PR_NewLogModule("nsIIdleService");

    Initialize();
}

nsIdleServiceGTK::~nsIdleServiceGTK()
{
    if (mXssInfo)
        XFree(mXssInfo);

// It is not safe to unload libXScrnSaver until each display is closed because
// the library registers callbacks through XESetCloseDisplay (Bug 397607).
// (Also the library and its functions are scoped for the file not the object.)
#if 0
    if (xsslib) {
        PR_UnloadLibrary(xsslib);
        xsslib = nullptr;
    }
#endif
}

bool
nsIdleServiceGTK::PollIdleTime(uint32_t *aIdleTime)
{
    if (!sInitialized) {
        // For some reason, we could not find xscreensaver.
        return false;
    }

    // Ask xscreensaver about idle time:
    *aIdleTime = 0;

    // We might not have a display (cf. in xpcshell)
    Display *dplay = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    if (!dplay) {
        MOZ_LOG(sIdleLog, LogLevel::Warning, ("No display found!\n"));
        return false;
    }

    if (!_XSSQueryExtension || !_XSSAllocInfo || !_XSSQueryInfo) {
        return false;
    }

    int event_base, error_base;
    if (_XSSQueryExtension(dplay, &event_base, &error_base))
    {
        if (!mXssInfo)
            mXssInfo = _XSSAllocInfo();
        if (!mXssInfo)
            return false;
        _XSSQueryInfo(dplay, GDK_ROOT_WINDOW(), mXssInfo);
        *aIdleTime = mXssInfo->idle;
        return true;
    }
    // If we get here, we couldn't get to XScreenSaver:
    MOZ_LOG(sIdleLog, LogLevel::Warning, ("XSSQueryExtension returned false!\n"));
    return false;
}

bool
nsIdleServiceGTK::UsePollMode()
{
    return sInitialized;
}
