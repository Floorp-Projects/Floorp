/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ex: set tabstop=8 softtabstop=2 shiftwidth=2 expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCUPSShim_h___
#define nsCUPSShim_h___

#include <cups/cups.h>
#include "mozilla/Atomics.h"
#include "mozilla/Mutex.h"

// TODO: This should be a configure option, ideally.
#ifndef XP_MACOSX
#  define CUPS_SHIM_RUNTIME_LINK
#endif

struct PRLibrary;

class nsCUPSShim {
 public:
#ifdef CUPS_SHIM_RUNTIME_LINK
  nsCUPSShim();
  bool InitOkay() { return mInitOkay; }
#else
  nsCUPSShim() = default;
  bool InitOkay() { return true; }
#endif

  // We allow some functions to be missing, to degrade as gracefully as possible
  // for older versions of CUPS.
  //
  // The current target is CUPS 1.6 (bug 1701019).
  enum class Optional : bool { No, Yes };

  /**
   * Function pointers for supported functions. These are only
   * valid after successful initialization.
   */
#define CUPS_SHIM_ALL_FUNCS(X)              \
  X(Optional::No, cupsAddOption)            \
  X(Optional::No, cupsCheckDestSupported)   \
  X(Optional::No, cupsConnectDest)          \
  X(Optional::No, cupsCopyDest)             \
  X(Optional::No, cupsCopyDestInfo)         \
  X(Optional::No, cupsDoRequest)            \
  X(Optional::No, cupsEnumDests)            \
  X(Optional::No, cupsFreeDestInfo)         \
  X(Optional::No, cupsFreeDests)            \
  X(Optional::No, cupsGetDestMediaByName)   \
  X(Optional::Yes, cupsFindDestDefault)     \
  X(Optional::Yes, cupsGetDestMediaDefault) \
  X(Optional::Yes, cupsGetDestMediaCount)   \
  X(Optional::Yes, cupsGetDestMediaByIndex) \
  X(Optional::Yes, cupsLocalizeDestMedia)   \
  X(Optional::No, cupsGetDest)              \
  X(Optional::No, cupsGetDests)             \
  X(Optional::No, cupsGetDests2)            \
  X(Optional::No, cupsGetNamedDest)         \
  X(Optional::No, cupsGetOption)            \
  X(Optional::No, cupsServer)               \
  X(Optional::Yes, httpAddrPort)            \
  X(Optional::No, httpClose)                \
  X(Optional::No, httpGetHostname)          \
  X(Optional::Yes, httpGetAddress)          \
  X(Optional::No, ippAddString)             \
  X(Optional::No, ippAddStrings)            \
  X(Optional::No, ippDelete)                \
  X(Optional::No, ippFindAttribute)         \
  X(Optional::No, ippGetCount)              \
  X(Optional::No, ippGetString)             \
  X(Optional::No, ippNewRequest)            \
  X(Optional::No, ippPort)

#ifdef CUPS_SHIM_RUNTIME_LINK
  // Define a single field which holds a function pointer.
#  define CUPS_SHIM_FUNC_DECL(opt_, fn_) decltype(::fn_)* fn_ = nullptr;
#else
  // Define a static constexpr function pointer. GCC can sometimes optimize
  // away the pointer fetch for this.
#  define CUPS_SHIM_FUNC_DECL(opt_, fn_) \
    static constexpr decltype(::fn_)* const fn_ = ::fn_;
#endif

  CUPS_SHIM_ALL_FUNCS(CUPS_SHIM_FUNC_DECL)
#undef CUPS_SHIM_FUNC_DECL

#ifdef CUPS_SHIM_RUNTIME_LINK
 private:
  bool mInitOkay = false;
  PRLibrary* mCupsLib = nullptr;
#endif
};

#endif /* nsCUPSShim_h___ */
