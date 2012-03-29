/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Gijs Kruitbosch <gijskruitbosch@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef MOZ_X11
#include "mozilla/X11Util.h"
#endif
#include "nsIdleServiceQt.h"
#include "nsIServiceManager.h"
#include "nsDebug.h"
#include "prlink.h"

#if !defined(MOZ_PLATFORM_MAEMO) && defined(MOZ_X11)
typedef bool (*_XScreenSaverQueryExtension_fn)(Display* dpy, int* event_base,
                                                 int* error_base);

typedef XScreenSaverInfo* (*_XScreenSaverAllocInfo_fn)(void);

typedef void (*_XScreenSaverQueryInfo_fn)(Display* dpy, Drawable drw,
                                          XScreenSaverInfo *info);

static _XScreenSaverQueryExtension_fn _XSSQueryExtension = nsnull;
static _XScreenSaverAllocInfo_fn _XSSAllocInfo = nsnull;
static _XScreenSaverQueryInfo_fn _XSSQueryInfo = nsnull;
#endif

static bool sInitialized = false;

NS_IMPL_ISUPPORTS2(nsIdleServiceQt, nsIIdleService, nsIdleService)

nsIdleServiceQt::nsIdleServiceQt()
#if !defined(MOZ_PLATFORM_MAEMO) && defined(MOZ_X11)
    : mXssInfo(nsnull)
#endif
{
}

static void Initialize()
{
    sInitialized = true;

#if !defined(MOZ_PLATFORM_MAEMO) && defined(MOZ_X11)
    // This will leak - See comments in ~nsIdleServiceQt().
    PRLibrary* xsslib = PR_LoadLibrary("libXss.so.1");
    if (!xsslib) {
        return;
    }

    _XSSQueryExtension = (_XScreenSaverQueryExtension_fn)
        PR_FindFunctionSymbol(xsslib, "XScreenSaverQueryExtension");
    _XSSAllocInfo = (_XScreenSaverAllocInfo_fn)
        PR_FindFunctionSymbol(xsslib, "XScreenSaverAllocInfo");
    _XSSQueryInfo = (_XScreenSaverQueryInfo_fn)
        PR_FindFunctionSymbol(xsslib, "XScreenSaverQueryInfo");
#endif
}

nsIdleServiceQt::~nsIdleServiceQt()
{
#if !defined(MOZ_PLATFORM_MAEMO) && defined(MOZ_X11)
    if (mXssInfo)
        XFree(mXssInfo);

// It is not safe to unload libXScrnSaver until each display is closed because
// the library registers callbacks through XESetCloseDisplay (Bug 397607).
// (Also the library and its functions are scoped for the file not the object.)
#if 0
    if (xsslib) {
        PR_UnloadLibrary(xsslib);
        xsslib = nsnull;
    }
#endif
#endif
}

bool
nsIdleServiceQt::PollIdleTime(PRUint32 *aIdleTime)
{
#if !defined(MOZ_PLATFORM_MAEMO) && defined(MOZ_X11)
    // Ask xscreensaver about idle time:
    *aIdleTime = 0;

    // We might not have a display (cf. in xpcshell)
    Display *dplay = mozilla::DefaultXDisplay();
    if (!dplay) {
        return false;
    }

    if (!sInitialized) {
        Initialize();
    }
    if (!_XSSQueryExtension || !_XSSAllocInfo || !_XSSQueryInfo) {
        return false;
    }

    int event_base, error_base;
    if (_XSSQueryExtension(dplay, &event_base, &error_base)) {
        if (!mXssInfo)
            mXssInfo = _XSSAllocInfo();
        if (!mXssInfo)
            return false;

        _XSSQueryInfo(dplay, RootWindowOfScreen(DefaultScreenOfDisplay(mozilla::DefaultXDisplay())), mXssInfo);
        *aIdleTime = mXssInfo->idle;
        return true;
    }
#endif

    return false;
}

bool
nsIdleServiceQt::UsePollMode()
{
#if !defined(MOZ_PLATFORM_MAEMO) && defined(MOZ_X11)
    return false;
#endif
    return true;
}

