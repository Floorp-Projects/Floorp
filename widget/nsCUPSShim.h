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

  /**
   * Function pointers for supported functions. These are only
   * valid after successful initialization.
   */
#define CUPS_SHIM_ALL_FUNCS(X) \
  X(cupsAddOption)             \
  X(cupsCheckDestSupported)    \
  X(cupsConnectDest)           \
  X(cupsCopyDest)              \
  X(cupsCopyDestInfo)          \
  X(cupsDoRequest)             \
  X(cupsEnumDests)             \
  X(cupsFindDestDefault)       \
  X(cupsFreeDestInfo)          \
  X(cupsFreeDests)             \
  X(cupsGetDestMediaDefault)   \
  X(cupsGetDestMediaCount)     \
  X(cupsGetDestMediaByIndex)   \
  X(cupsGetDestMediaByName)    \
  X(cupsGetDest)               \
  X(cupsGetDests)              \
  X(cupsGetNamedDest)          \
  X(cupsGetOption)             \
  X(cupsLocalizeDestMedia)     \
  X(cupsPrintFile)             \
  X(cupsTempFd)                \
  X(httpClose)                 \
  X(ippAddString)              \
  X(ippAddStrings)             \
  X(ippDelete)                 \
  X(ippFindAttribute)          \
  X(ippGetCount)               \
  X(ippGetString)              \
  X(ippNewRequest)

#ifdef CUPS_SHIM_RUNTIME_LINK
  // Define a single field which holds a function pointer.
#  define CUPS_SHIM_FUNC_DECL(X) decltype(::X)* X = nullptr;
#else
  // Define a static constexpr function pointer. GCC can sometimes optimize
  // away the pointer fetch for this.
#  define CUPS_SHIM_FUNC_DECL(X) static constexpr decltype(::X)* const X = ::X;
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
