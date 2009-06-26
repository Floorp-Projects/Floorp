/* -*- Mode: C++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla XPCOM.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sergei Dolgov <sergei_d@fi.tartu.ee>
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

#include "nsGlueLinking.h"
#include "nsXPCOMGlue.h"

#include <kernel/image.h>
#include <stdlib.h>
#include <stdio.h>
#include <Entry.h>
#include <Path.h>

#define LEADING_UNDERSCORE


struct DependentLib
{
    image_id         libHandle;
    DependentLib *next;
};

static DependentLib *sTop;
static image_id sXULLibHandle;

static void
AppendDependentLib(image_id libHandle)
{
    DependentLib *d = new DependentLib;
    if (!d)
        return;

    d->next = sTop;
    d->libHandle = libHandle;

    sTop = d;
}

static void
ReadDependentCB(const char *aDependentLib)
{
    image_id libHandle = load_add_on(aDependentLib);
    if (!libHandle)
        return;

    AppendDependentLib(libHandle);
}

nsresult
XPCOMGlueLoad(const char *xpcomFile, GetFrozenFunctionsFunc *func)
{
    char xpcomDir[MAXPATHLEN];
	BPath libpath;
    if (libpath.SetTo(xpcomFile, 0, true) == B_OK) {
        sprintf(xpcomDir, libpath.Path());
        char *lastSlash = strrchr(xpcomDir, '/');
        if (lastSlash) {
            *lastSlash = '\0';

            XPCOMGlueLoadDependentLibs(xpcomDir, ReadDependentCB);

            snprintf(lastSlash, MAXPATHLEN - strlen(xpcomDir), "/" XUL_DLL);

            sXULLibHandle = load_add_on(xpcomDir);
        }
    }


    image_id libHandle = nsnull;

    if (xpcomFile[0] != '.' || xpcomFile[1] != '\0') {
        libHandle = load_add_on(xpcomFile);
        if (libHandle) {
            AppendDependentLib(libHandle);
        }
    }

    GetFrozenFunctionsFunc sym = 0;
    status_t result;
    
    result = get_image_symbol(libHandle,
                              LEADING_UNDERSCORE "NS_GetFrozenFunctions", B_SYMBOL_TYPE_TEXT, (void **)sym);

    if (!sym || B_OK != result) {
        XPCOMGlueUnload();
        return NS_ERROR_NOT_AVAILABLE;
    }

    *func = sym;

    return OK;
}

void
XPCOMGlueUnload()
{
    while (sTop) {
        unload_add_on(sTop->libHandle);

        DependentLib *temp = sTop;
        sTop = sTop->next;

        delete temp;
    }

    if (sXULLibHandle) {
        unload_add_on(sXULLibHandle);
        sXULLibHandle = nsnull;
    }
}

nsresult
XPCOMGlueLoadXULFunctions(const nsDynamicFunctionLoad *symbols)
{

    nsresult rv = NS_OK;
    
    while (symbols->functionName) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer),
                 LEADING_UNDERSCORE "%s", symbols->functionName);

        *symbols->function = 0;
        status_t result;
        result = get_image_symbol(sXULLibHandle, buffer, B_SYMBOL_TYPE_TEXT, (void **)*symbols->function );
        if (!*symbols->function || B_OK != result)
            rv = NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;

        ++symbols;
    }
    return rv;
}
