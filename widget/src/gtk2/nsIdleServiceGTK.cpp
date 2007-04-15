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
 *  Gijs Kruitbosch <gijskruitbosch@gmail.com> 
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

#include "nsIdleServiceGTK.h"
#include "nsIServiceManager.h"
#include "nsDebug.h"
#include "prlink.h"
#include "prlog.h"


#ifdef PR_LOGGING
static PRLogModuleInfo* sIdleLog = nsnull;
#endif

typedef PRBool (*_XScreenSaverQueryExtension_fn)(Display* dpy, int* event_base,
                                                 int* error_base);

typedef XScreenSaverInfo* (*_XScreenSaverAllocInfo_fn)(void);

typedef void (*_XScreenSaverQueryInfo_fn)(Display* dpy, Drawable drw,
                                          XScreenSaverInfo *info);

static PRLibrary* xsslib = nsnull;
static _XScreenSaverQueryExtension_fn _XSSQueryExtension = nsnull;
static _XScreenSaverAllocInfo_fn _XSSAllocInfo = nsnull;
static _XScreenSaverQueryInfo_fn _XSSQueryInfo = nsnull;


NS_IMPL_ISUPPORTS1(nsIdleServiceGTK, nsIIdleService)

nsIdleServiceGTK::nsIdleServiceGTK()
    : mXssInfo(nsnull)
{
    NS_ASSERTION(!xsslib, "created two instances of the idle service");
#ifdef PR_LOGGING
    if (!sIdleLog)
        sIdleLog = PR_NewLogModule("nsIIdleService");
#endif

    xsslib = PR_LoadLibrary("libXss.so.1");
    if (!xsslib) // ouch.
    {
#ifdef PR_LOGGING
        PR_LOG(sIdleLog, PR_LOG_WARNING, ("Failed to find libXss.so!\n"));
#endif
        return;
    }

    _XSSQueryExtension = (_XScreenSaverQueryExtension_fn)
        PR_FindFunctionSymbol(xsslib, "XScreenSaverQueryExtension");
    _XSSAllocInfo = (_XScreenSaverAllocInfo_fn)
        PR_FindFunctionSymbol(xsslib, "XScreenSaverAllocInfo");
    _XSSQueryInfo = (_XScreenSaverQueryInfo_fn)
        PR_FindFunctionSymbol(xsslib, "XScreenSaverQueryInfo");
#ifdef PR_LOGGING
    if (!_XSSQueryExtension)
        PR_LOG(sIdleLog, PR_LOG_WARNING, ("Failed to get XSSQueryExtension!\n"));
    if (!_XSSAllocInfo)
        PR_LOG(sIdleLog, PR_LOG_WARNING, ("Failed to get XSSAllocInfo!\n"));
    if (!_XSSQueryInfo)
        PR_LOG(sIdleLog, PR_LOG_WARNING, ("Failed to get XSSQueryInfo!\n"));
#endif
 
}

nsIdleServiceGTK::~nsIdleServiceGTK()
{
    if (mXssInfo)
        XFree(mXssInfo);
    if (xsslib) {
        PR_UnloadLibrary(xsslib);
        xsslib = nsnull;
    }
}

NS_IMETHODIMP
nsIdleServiceGTK::GetIdleTime(PRUint32 *aTimeDiff)
{
    // Ask xscreensaver about idle time:
    int event_base, error_base;
    *aTimeDiff = 0;

    // We might not have a display (cf. in xpcshell)
    Display *dplay = GDK_DISPLAY();
    if (!dplay || !_XSSQueryExtension || !_XSSAllocInfo || !_XSSQueryInfo)
    {
#ifdef PR_LOGGING
        if (!dplay)
            PR_LOG(sIdleLog, PR_LOG_WARNING, ("No display found!\n"));
        else
            PR_LOG(sIdleLog, PR_LOG_WARNING, ("One of the Xss functions is missing!\n"));
#endif
        return NS_ERROR_FAILURE;
    }

    if (_XSSQueryExtension(dplay, &event_base, &error_base))
    {
        if (!mXssInfo)
            mXssInfo = _XSSAllocInfo();
        if (!mXssInfo)
            return NS_ERROR_OUT_OF_MEMORY;
        _XSSQueryInfo(dplay, GDK_ROOT_WINDOW(), mXssInfo);
        *aTimeDiff = mXssInfo->idle;
        return NS_OK;
    }
    // If we get here, we couldn't get to XScreenSaver:
#ifdef PR_LOGGING
    PR_LOG(sIdleLog, PR_LOG_WARNING, ("XSSQueryExtension returned false!\n"));
#endif
    return NS_ERROR_FAILURE;
}

