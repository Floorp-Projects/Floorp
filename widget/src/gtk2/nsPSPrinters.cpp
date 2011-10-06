/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ex: set tabstop=8 softtabstop=4 shiftwidth=4 expandtab: */
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
 * The Original Code is the Mozilla PostScript driver printer list component.
 *
 * The Initial Developer of the Original Code is
 * Kenneth Herron <kherron+mozilla@fmailbox.com>
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#include "nscore.h"
#include "nsCUPSShim.h"
#include "nsIServiceManager.h"
#include "nsPSPrinters.h"
#include "nsReadableUtils.h"        // StringBeginsWith()
#include "nsCUPSShim.h"
#include "mozilla/Preferences.h"

#include "prlink.h"
#include "prenv.h"
#include "plstr.h"

using namespace mozilla;

#define NS_CUPS_PRINTER "CUPS/"
#define NS_CUPS_PRINTER_LEN (sizeof(NS_CUPS_PRINTER) - 1)

/* dummy printer name for the gfx/src/ps driver */
#define NS_POSTSCRIPT_DRIVER_NAME "PostScript/"

nsCUPSShim gCupsShim;

nsPSPrinterList::nsPSPrinterList()
{
    // Should we try cups?
    if (Preferences::GetBool("print.postscript.cups.enabled", PR_TRUE) &&
        !gCupsShim.IsInitialized()) {
        gCupsShim.Init();
    }
}


/* Check whether the PostScript module has been disabled at runtime */
PRBool
nsPSPrinterList::Enabled()
{
    const char *val = PR_GetEnv("MOZILLA_POSTSCRIPT_ENABLED");
    if (val && (val[0] == '0' || !PL_strcasecmp(val, "false")))
        return PR_FALSE;

    // is the PS module enabled?
    return Preferences::GetBool("print.postscript.enabled", PR_TRUE);
}


/* Fetch a list of printers handled by the PostsScript module */
void
nsPSPrinterList::GetPrinterList(nsTArray<nsCString>& aList)
{
    aList.Clear();

    // Query CUPS for a printer list. The default printer goes to the
    // head of the output list; others are appended.
    if (gCupsShim.IsInitialized()) {
        cups_dest_t *dests;

        int num_dests = (gCupsShim.mCupsGetDests)(&dests);
        if (num_dests) {
            for (int i = 0; i < num_dests; i++) {
                nsCAutoString fullName(NS_CUPS_PRINTER);
                fullName.Append(dests[i].name);
                if (dests[i].instance != NULL) {
                    fullName.Append("/");
                    fullName.Append(dests[i].instance);
                }
                if (dests[i].is_default)
                    aList.InsertElementAt(0, fullName);
                else
                    aList.AppendElement(fullName);
            }
        }
        (gCupsShim.mCupsFreeDests)(num_dests, dests);
    }

    // Build the "classic" list of printers -- those accessed by running
    // an opaque command. This list always contains a printer named "default".
    // In addition, we look for either an environment variable
    // MOZILLA_POSTSCRIPT_PRINTER_LIST or a preference setting
    // print.printer_list, which contains a space-separated list of printer
    // names.
    aList.AppendElement(
            NS_LITERAL_CSTRING(NS_POSTSCRIPT_DRIVER_NAME "default"));

    nsCAutoString list(PR_GetEnv("MOZILLA_POSTSCRIPT_PRINTER_LIST"));
    if (list.IsEmpty()) {
        list = Preferences::GetCString("print.printer_list");
    }
    if (!list.IsEmpty()) {
        // For each printer (except "default" which was already added),
        // construct a string "PostScript/<name>" and append it to the list.
        char *state;

        for (char *name = PL_strtok_r(list.BeginWriting(), " ", &state);
                nsnull != name;
                name = PL_strtok_r(nsnull, " ", &state)
        ) {
            if (0 != strcmp(name, "default")) {
                nsCAutoString fullName(NS_POSTSCRIPT_DRIVER_NAME);
                fullName.Append(name);
                aList.AppendElement(fullName);
            }
        }
    }
}


/* Identify the printer type */
nsPSPrinterList::PrinterType
nsPSPrinterList::GetPrinterType(const nsACString& aName)
{
    if (StringBeginsWith(aName, NS_LITERAL_CSTRING(NS_POSTSCRIPT_DRIVER_NAME)))
        return kTypePS;
    else if (StringBeginsWith(aName, NS_LITERAL_CSTRING(NS_CUPS_PRINTER)))
        return kTypeCUPS;
    else
        return kTypeUnknown;
}
