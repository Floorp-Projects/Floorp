/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ex: set tabstop=8 softtabstop=4 shiftwidth=4 expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDebug.h"
#include "nsString.h"
#include "nsCUPSShim.h"
#include "mozilla/ArrayUtils.h"
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
static const int gSymNameCt = mozilla::ArrayLength(gSymName);


bool
nsCUPSShim::Init()
{
    mCupsLib = PR_LoadLibrary("libcups.so.2");
    if (!mCupsLib)
        return false;

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
            nsAutoCString msg(gSymName[i]);
            msg.AppendLiteral(" not found in CUPS library");
            NS_WARNING(msg.get());
#endif
            PR_UnloadLibrary(mCupsLib);
            mCupsLib = nullptr;
            return false;
        }
    }
    return true;
}
