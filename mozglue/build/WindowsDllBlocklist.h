/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_windowsdllblocklist_h
#define mozilla_windowsdllblocklist_h

#if (defined(_MSC_VER) || defined(__MINGW32__))  && (defined(_M_IX86) || defined(_M_X64))

#include <windows.h>
#include "mozilla/Attributes.h"
#include "mozilla/Types.h"

#define HAS_DLL_BLOCKLIST

enum DllBlocklistInitFlags
{
  eDllBlocklistInitFlagDefault = 0,
  eDllBlocklistInitFlagIsChildProcess = 1
};

MFBT_API void DllBlocklist_Initialize(uint32_t aInitFlags = eDllBlocklistInitFlagDefault);
MFBT_API void DllBlocklist_WriteNotes(HANDLE file);
MFBT_API bool DllBlocklist_CheckStatus();

// Forward declaration
namespace mozilla {
namespace detail {
class DllServicesBase;
} // namespace detail
} // namespace mozilla

MFBT_API void DllBlocklist_SetDllServices(mozilla::detail::DllServicesBase* aSvc);

#endif // defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#endif // mozilla_windowsdllblocklist_h
