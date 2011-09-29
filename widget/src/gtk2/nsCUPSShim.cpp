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

#include "nsDebug.h"
#include "nsString.h"
#include "nsCUPSShim.h"
#include "prlink.h"


// List of symbols to find in libcups. Must match symAddr[] defined in Init().
// Making this an array of arrays instead of pointers allows storing the
// whole thing in read-only memory.
static const char gSymName[][sizeof("cupsPrintFile")] = {
    { "cupsAddOption" },
    { "cupsFreeDests" },
    { "cupsGetDest" },
    { "cupsGetDests" },
    { "cupsPrintFile" },
    { "cupsTempFd" },
};
static const int gSymNameCt = NS_ARRAY_LENGTH(gSymName);


bool
nsCUPSShim::Init()
{
    mCupsLib = PR_LoadLibrary("libcups.so.2");
    if (!mCupsLib)
        return PR_FALSE;

    // List of symbol pointers. Must match gSymName[] defined above.
    void **symAddr[] = {
        (void **)&mCupsAddOption,
        (void **)&mCupsFreeDests,
        (void **)&mCupsGetDest,
        (void **)&mCupsGetDests,
        (void **)&mCupsPrintFile,
        (void **)&mCupsTempFd,
    };

    for (int i = gSymNameCt; i--; ) {
        *(symAddr[i]) = PR_FindSymbol(mCupsLib, gSymName[i]);
        if (! *(symAddr[i])) {
#ifdef DEBUG
            nsCAutoString msg(gSymName[i]);
            msg.Append(" not found in CUPS library");
            NS_WARNING(msg.get());
#endif
            PR_UnloadLibrary(mCupsLib);
            mCupsLib = nsnull;
            return PR_FALSE;
        }
    }
    return PR_TRUE;
}
