/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2002 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Bolian Yin (bolian.yin@sun.com)
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>
#include <glib.h>

#include "prenv.h"
#include "nsCommonWidget.h"

#include "nsGConfInterface.h"
#include "nsAccessibilityInterface.h"


static const char sMaiLibName [] ="libmai.so";
static const char sMaiInitFunc [] = "mai_init";

static const char sGconfAccKey [] = "/desktop/gnome/interface/accessibility";
static const char sAccEnv [] = "GNOME_ACCESSIBILITY";

GnomeAccessibilityModule nsAccessibilityInterface::mAtkBridge = {
    "libatk-bridge.so", NULL,
    "gnome_accessibility_module_init", NULL,
    "gnome_accessibility_module_shutdown", NULL,
};

MaiHook *nsAccessibilityInterface::mMaiHook = nsnull;
PRLibrary *nsAccessibilityInterface::mMaiLib = nsnull;
PRBool nsAccessibilityInterface::mInitialized = PR_FALSE;

PRBool
nsAccessibilityInterface::Init()
{
    if (mInitialized)
        return PR_TRUE;

    //load accessibility when
    // 1. GNOME_ACCESSIBILITY set to non-zero, or
    // 2. accessibility gconf key set to True.

    PRFuncPtr maiInit = NULL;
    PRBool enableAcc = FALSE;

    //check if accessibility enabled/disabled by environment variable
    const char *envValue = PR_GetEnv(sAccEnv);

    if (envValue) {
        enableAcc = atoi(envValue);
        LOG(("Accessibility Env %s=%s\n", sAccEnv, envValue));
    }
    //check gconf-2 setting
    else {
        enableAcc = nsGConfInterface::GetBool(sGconfAccKey);
        LOG(("Accessibility Gconf %s Enabled\n", enableAcc ? "" : "NOT"));
    }

    if (!enableAcc) {
        LOG(("Accessibility NOT Enabled\n"));
        goto cleanup;
    }

    //load and initialize mai library
    mMaiLib = PR_LoadLibrary(sMaiLibName);
    if (!mMaiLib) {
        LOG(("Accessibility Fail to load Mai library %s\n", sMaiLibName));
        goto cleanup;
    }
    maiInit = PR_FindFunctionSymbol(mMaiLib, sMaiInitFunc);
    if (!maiInit || !(*MaiInit(maiInit))(&mMaiHook) || !mMaiHook) {
        LOG(("Accessibility Fail to initialize mai library\n"));
        goto cleanup;
    }

    //load and initialize atk-bridge library
    if (!LoadGtkModule(mAtkBridge)) {
        LOG(("Fail to load lib: %s\n", mAtkBridge.libName));
        goto cleanup;
    }
    //init atk-bridge
    (*mAtkBridge.init)();

    //at last
    mInitialized = PR_TRUE;
    return PR_TRUE;

 cleanup:
    if (mMaiLib) {
        mMaiHook = nsnull;
        PR_UnloadLibrary(mMaiLib);
        mMaiLib = nsnull;
        mMaiHook = nsnull;
    }
    return PR_FALSE;
}

PRBool
nsAccessibilityInterface::ShutDown()
{
    if (!mInitialized)
        return PR_TRUE;

    mInitialized = PR_FALSE;
    if (mMaiHook) {
        mMaiHook->MaiShutdown();
        mMaiHook = NULL;
    }
    if (mAtkBridge.lib) {
        if (mAtkBridge.shutdown)
            (*mAtkBridge.shutdown)();
        //Not unload atk-bridge library
        //an exit function registered will take care of it
        mAtkBridge.lib = NULL;
        mAtkBridge.init = NULL;
        mAtkBridge.shutdown = NULL;
    }
    if (mMaiLib) {
        PR_UnloadLibrary(mMaiLib);
        mMaiLib = nsnull;
    }
    return PR_TRUE;
}

PRBool
nsAccessibilityInterface::AddTopLevel(nsIAccessible *toplevel)
{
    if (mMaiHook && mMaiHook->AddTopLevelAccessible)
        return (*mMaiHook->AddTopLevelAccessible)(toplevel);
    return PR_FALSE;
}

PRBool
nsAccessibilityInterface::RemoveTopLevel(nsIAccessible *toplevel)
{
    if (mMaiHook && mMaiHook->RemoveTopLevelAccessible)
        return (*mMaiHook->RemoveTopLevelAccessible)(toplevel);
    return PR_FALSE;
}

PRBool
nsAccessibilityInterface::LoadGtkModule(GnomeAccessibilityModule& aModule)
{
    if (!aModule.libName)
        return PR_FALSE;
    if (!(aModule.lib = PR_LoadLibrary(aModule.libName))) {

        LOG(("Fail to load lib: %s in default path\n", aModule.libName));

        //try to load the module with "gtk-2.0/modules" appended
        char *curLibPath = PR_GetLibraryPath();
        nsCAutoString libPath(curLibPath);
        LOG(("Current Lib path=%s\n", curLibPath));
        PR_FreeLibraryName(curLibPath);

        PRInt16 loc1 = 0, loc2 = 0;
        PRInt16 subLen = 0;
        while (loc2 >= 0) {
            loc2 = libPath.FindChar(':', loc1);
            if (loc2 < 0)
                subLen = libPath.Length() - loc1;
            else
                subLen = loc2 - loc1;
            nsCAutoString sub(Substring(libPath, loc1, subLen));
            sub.Append("/gtk-2.0/modules/");
            sub.Append(aModule.libName);
            aModule.lib = PR_LoadLibrary(sub.get());
            if (aModule.lib) {
                LOG(("Ok, load %s from %s\n", aModule.libName, sub.get()));
                break;
            }
            loc1 = loc2+1;
        }
        if (!aModule.lib) {
            LOG(("Fail to load %s\n", aModule.libName));
            return PR_FALSE;
        }
    }

    //we have loaded the library, try to get the function ptrs
    if (!(aModule.init = PR_FindFunctionSymbol(aModule.lib,
                                               aModule.initName)) ||
        !(aModule.shutdown = PR_FindFunctionSymbol(aModule.lib,
                                                   aModule.shutdownName))) {

        //fail, :(
        LOG(("Fail to find symbol %s in %s", aModule.init ?
             aModule.shutdownName : aModule.initName, aModule.libName));
        PR_UnloadLibrary(aModule.lib);
        aModule.lib = NULL;
        return PR_FALSE;
    }

    return PR_TRUE;
}
