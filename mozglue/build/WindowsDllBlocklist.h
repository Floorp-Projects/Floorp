/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_windowsdllblocklist_h
#define mozilla_windowsdllblocklist_h

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))

#include <windows.h>
#include "mozilla/GuardObjects.h"
#include "mozilla/Attributes.h"

#define HAS_DLL_BLOCKLIST

MFBT_API void DllBlocklist_Initialize();
MFBT_API void DllBlocklist_SetInXPCOMLoadOnMainThread(bool inXPCOMLoadOnMainThread);
MFBT_API void DllBlocklist_WriteNotes(HANDLE file);
MFBT_API bool DllBlocklist_CheckStatus();

class MOZ_RAII AutoSetXPCOMLoadOnMainThread
{
  public:
    AutoSetXPCOMLoadOnMainThread(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM) {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
      DllBlocklist_SetInXPCOMLoadOnMainThread(true);
    }

    ~AutoSetXPCOMLoadOnMainThread() {
      DllBlocklist_SetInXPCOMLoadOnMainThread(false);
    }

  private:
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

#endif // defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#endif // mozilla_windowsdllblocklist_h
