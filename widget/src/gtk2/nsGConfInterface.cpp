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
#include "nsCOMPtr.h"
#include "nsIPref.h"
#include "nsIServiceManagerUtils.h"

#include "nsCommonWidget.h"
#include "nsGConfInterface.h"

//gconf function type
G_BEGIN_DECLS
typedef PRBool (*GConfClientGetBool) (void *client, const gchar *key,
                                      GError **err);
typedef void * (*GConfClientGetDefault) (void);
G_END_DECLS

typedef struct {
    const char *FuncName;
    PRFuncPtr  FuncPtr;
}GConfFuncListType;

static GConfFuncListType sGConfFuncList[] = {
    {"gconf_client_get_default", nsnull},
    {"gconf_client_get_bool", nsnull},
    {nsnull, nsnull},
};

static const char sPrefGConfKey[] = "accessibility.unix.gconf2.shared-library";
static const char sDefaultLibName1[] = "libgconf-2.so.4";
static const char sDefaultLibName2[] = "libgconf-2.so";

void *nsGConfInterface::mGConfClient = nsnull;
PRLibrary *nsGConfInterface::mGConfLib = nsnull;
PRBool nsGConfInterface::mInitialized = PR_FALSE;

/* public */
PRBool
nsGConfInterface::GetBool(const char *aKey)
{
    if (!mInitialized && !Init())
        return PR_FALSE;
    GConfClientGetBool getBool =
        NS_REINTERPRET_CAST(GConfClientGetBool, sGConfFuncList[1].FuncPtr);
    return getBool(mGConfClient, aKey, NULL);
}

/* private */
PRBool
nsGConfInterface::Init()
{
    if (mInitialized)
        return PR_TRUE;

    nsCOMPtr<nsIPref> pref = do_GetService(NS_PREF_CONTRACTID);
    if (!pref)
        return PR_FALSE;


    nsXPIDLCString gconfLibName;
    nsresult rv;

    //check if gconf-2 library is given in prefs
    rv = pref->GetCharPref(sPrefGConfKey, getter_Copies(gconfLibName));
    if (NS_SUCCEEDED(rv)) {
        //use the library name in the preference
        LOG(("GConf library in prefs is %s\n", gconfLibName.get()));
        mGConfLib = PR_LoadLibrary(gconfLibName.get());
    }
    else {
        LOG(("GConf library not specified in prefs, try the default: "
             "%s and %s\n", sDefaultLibName1, sDefaultLibName2));
        mGConfLib = PR_LoadLibrary(sDefaultLibName1);
        if (!mGConfLib)
            mGConfLib = PR_LoadLibrary(sDefaultLibName2);
    }

    if (!mGConfLib) {
        LOG(("Fail to load GConf library\n"));
        return PR_FALSE;
    }


    //check every func we need in the gconf library
    GConfFuncListType *funcList;
    PRFuncPtr func;
    for (funcList = sGConfFuncList; funcList->FuncName; ++funcList) {
        func = PR_FindFunctionSymbol(mGConfLib, funcList->FuncName);
        if (!func) {
            LOG(("Check GConf Function Error: %s", funcList->FuncName));
            PR_UnloadLibrary(mGConfLib);
            mGConfLib = nsnull;
            return PR_FALSE;
        }
        funcList->FuncPtr = func;
    }

    //get default gconf client
    GConfClientGetDefault getDefault =
        NS_REINTERPRET_CAST(GConfClientGetDefault, sGConfFuncList[0].FuncPtr);

    mGConfClient = (*getDefault)();
    if (!mGConfClient) {
        LOG(("Fail to Get default gconf client\n"));
        PR_UnloadLibrary(mGConfLib);
        mGConfLib = nsnull;
        return PR_FALSE;
    }
    mInitialized = PR_TRUE;
    return PR_TRUE;
}
