/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ex: set tabstop=8 softtabstop=2 shiftwidth=2 expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDebug.h"
#include "nsString.h"
#include "nsCUPSShim.h"
#include "mozilla/ArrayUtils.h"
#include "prlink.h"

#ifdef CUPS_SHIM_RUNTIME_LINK

// TODO: This is currently pointless as we always use the compile-time linked
// version of CUPS, but in the future this may become a configure option.
// We also cannot use NSPR's library suffix support, since that cannot handle
// version number suffixes.
#  ifdef XP_MACOSX
static const char gCUPSLibraryName[] = "libcups.2.dylib";
#  else
static const char gCUPSLibraryName[] = "libcups.so.2";
#  endif

template <typename FuncT>
static bool LoadCupsFunc(PRLibrary*& lib, FuncT*& dest,
                         const char* const name) {
  dest = (FuncT*)PR_FindSymbol(lib, name);
  if (MOZ_UNLIKELY(!dest)) {
#  ifdef DEBUG
    nsAutoCString msg(name);
    msg.AppendLiteral(" not found in CUPS library");
    NS_WARNING(msg.get());
#  endif
#  ifndef MOZ_TSAN
    // With TSan, we cannot unload libcups once we have loaded it because
    // TSan does not support unloading libraries that are matched from its
    // suppression list. Hence we just keep the library loaded in TSan builds.
    PR_UnloadLibrary(lib);
#  endif
    lib = nullptr;
    return false;
  }
  return true;
}

nsCUPSShim::nsCUPSShim() {
  mCupsLib = PR_LoadLibrary(gCUPSLibraryName);
  if (!mCupsLib) {
    return;
  }

  // This is a macro so that it could also load from libcups if we are
  // configured to use it as a compile-time dependency.
#  define CUPS_SHIM_LOAD(NAME) \
    if (!LoadCupsFunc(mCupsLib, NAME, #NAME)) return;
  CUPS_SHIM_ALL_FUNCS(CUPS_SHIM_LOAD)
#  undef CUPS_SHIM_LOAD

  // Set mInitOkay only if all cups functions are loaded successfully.
  mInitOkay = true;
}

#endif
