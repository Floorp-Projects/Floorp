/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ex: set tabstop=8 softtabstop=4 shiftwidth=4 expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDebug.h"
#include "nsString.h"
#include "nsCUPSShim.h"
#include "mozilla/ArrayUtils.h"
#include "prlink.h"

#ifdef XP_MACOSX
// TODO: On OS X we are guaranteed to have CUPS, so it would be nice to just
// assign the members from the header directly instead of dlopen'ing.
// Alternatively, we could just do some #define's on OS X, but we don't use
// CUPS all that much, so there really isn't too much overhead in storing this
// table of functions even on OS X.
static const char gCUPSLibraryName[] = "libcups.2.dylib";
#else
static const char gCUPSLibraryName[] = "libcups.so.2";
#endif

template <typename FuncT>
static bool LoadCupsFunc(PRLibrary* const lib, FuncT*& dest,
                         const char* const name) {
  dest = (FuncT*)PR_FindSymbol(lib, name);
  if (MOZ_UNLIKELY(!dest)) {
#ifdef DEBUG
    nsAutoCString msg(name);
    msg.AppendLiteral(" not found in CUPS library");
    NS_WARNING(msg.get());
#endif
    return false;
  }
  return true;
}

// This is a macro so that it could also load from libcups if we are configured
// to use it as a compile-time dependency.
#define CUPS_SHIM_LOAD(MEMBER, NAME) LoadCupsFunc(mCupsLib, MEMBER, #NAME)

bool nsCUPSShim::Init() {
  MOZ_ASSERT(!mInited);
  mCupsLib = PR_LoadLibrary(gCUPSLibraryName);
  if (!mCupsLib) {
    return false;
  }

  if (!(CUPS_SHIM_LOAD(mCupsAddOption, cupsAddOption) &&
        CUPS_SHIM_LOAD(mCupsFreeDests, cupsFreeDests) &&
        CUPS_SHIM_LOAD(mCupsGetDest, cupsGetDest) &&
        CUPS_SHIM_LOAD(mCupsGetDests, cupsGetDests) &&
        CUPS_SHIM_LOAD(mCupsPrintFile, cupsPrintFile) &&
        CUPS_SHIM_LOAD(mCupsTempFd, cupsTempFd))) {
#ifndef MOZ_TSAN
    // With TSan, we cannot unload libcups once we have loaded it because
    // TSan does not support unloading libraries that are matched from its
    // suppression list. Hence we just keep the library loaded in TSan builds.
    PR_UnloadLibrary(mCupsLib);
#endif
    mCupsLib = nullptr;
    return false;
  }
  mInited = true;
  return true;
}
