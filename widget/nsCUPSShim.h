/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ex: set tabstop=8 softtabstop=4 shiftwidth=4 expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCUPSShim_h___
#define nsCUPSShim_h___

#include <cups/cups.h>

struct PRLibrary;

/* Note: this class relies on static initialization. */
class nsCUPSShim {
 public:
  /**
   * Initialize this object. Attempt to load the CUPS shared
   * library and find function pointers for the supported
   * functions (see below).
   * @return false if the shared library could not be loaded, or if
   *                  any of the functions could not be found.
   *         true  for successful initialization.
   */
  bool Init();

  /**
   * @return true  if the object was initialized successfully.
   *         false otherwise.
   */
  bool IsInitialized() const { return mInited; }

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
  X(cupsFreeDestInfo)          \
  X(cupsFreeDests)             \
  X(cupsGetDestMediaCount)     \
  X(cupsGetDestMediaByIndex)   \
  X(cupsGetDest)               \
  X(cupsGetDests)              \
  X(cupsGetNamedDest)          \
  X(cupsLocalizeDestMedia)     \
  X(cupsPrintFile)             \
  X(cupsTempFd)                \
  X(httpClose)

#define CUPS_SHIM_FUNC_DECL(X) decltype(::X)* X;
  CUPS_SHIM_ALL_FUNCS(CUPS_SHIM_FUNC_DECL)
#undef CUPS_SHIM_FUNC_DECL

 private:
  bool mInited = false;
  PRLibrary* mCupsLib;
};

#endif /* nsCUPSShim_h___ */
