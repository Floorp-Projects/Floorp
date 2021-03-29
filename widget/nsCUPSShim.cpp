/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ex: set tabstop=8 softtabstop=2 shiftwidth=2 expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDebug.h"
#include "nsString.h"
#include "nsCUPSShim.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Logging.h"
#include "prlink.h"

#ifdef CUPS_SHIM_RUNTIME_LINK

mozilla::LazyLogModule gCupsLinkLog("CupsLink");

#  define DEBUG_LOG(...) \
    MOZ_LOG(gCupsLinkLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

// TODO: This is currently pointless as we always use the compile-time linked
// version of CUPS, but in the future this may become a configure option.
// We also cannot use NSPR's library suffix support, since that cannot handle
// version number suffixes.
#  ifdef XP_MACOSX
static const char gCUPSLibraryName[] = "libcups.2.dylib";
#  else
static const char gCUPSLibraryName[] = "libcups.so.2";
#  endif

static bool LoadCupsFunc(PRLibrary* aLib, void** aDest, const char* const aName,
                         nsCUPSShim::Optional aOptional) {
  *aDest = PR_FindSymbol(aLib, aName);
  if (!*aDest) {
    DEBUG_LOG("%s not found in CUPS library", aName);
    return bool(aOptional);
  }
  return true;
}

nsCUPSShim::nsCUPSShim() {
  mCupsLib = PR_LoadLibrary(gCUPSLibraryName);
  if (!mCupsLib) {
    DEBUG_LOG("CUPS library not found");
    return;
  }

  bool success = true;

  // This is a macro so that it could also load from libcups if we are
  // configured to use it as a compile-time dependency.
  //
  // We try to load all functions even if some fail so that we get the debug log
  // unconditionally.
#  define CUPS_SHIM_LOAD(opt_, fn_) \
    success |=                      \
        LoadCupsFunc(mCupsLib, reinterpret_cast<void**>(&fn_), #fn_, opt_);
  CUPS_SHIM_ALL_FUNCS(CUPS_SHIM_LOAD)
#  undef CUPS_SHIM_LOAD

  if (!success) {
#  ifndef MOZ_TSAN
    // With TSan, we cannot unload libcups once we have loaded it because
    // TSan does not support unloading libraries that are matched from its
    // suppression list. Hence we just keep the library loaded in TSan builds.
    PR_UnloadLibrary(mCupsLib);
#  endif
    mCupsLib = nullptr;
    return;
  }

  // Set mInitOkay only if all cups functions are loaded successfully.
  mInitOkay = true;
}

#endif
