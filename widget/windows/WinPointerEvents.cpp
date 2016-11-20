/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * WinPointerEvents - Helper functions to retrieve PointerEvent's attributes
 */

#include "nscore.h"
#include "WinPointerEvents.h"
#include "mozilla/MouseEvents.h"

using namespace mozilla;
using namespace mozilla::widget;

const wchar_t WinPointerEvents::kPointerLibraryName[] =  L"user32.dll";
HMODULE WinPointerEvents::sLibraryHandle = nullptr;
WinPointerEvents::GetPointerTypePtr WinPointerEvents::getPointerType = nullptr;
WinPointerEvents::GetPointerInfoPtr WinPointerEvents::getPointerInfo = nullptr;
WinPointerEvents::GetPointerPenInfoPtr WinPointerEvents::getPointerPenInfo = nullptr;
bool WinPointerEvents::sPointerEventEnabled = true;

WinPointerEvents::WinPointerEvents()
{
  InitLibrary();
  static bool addedPointerEventEnabled = false;
  if (!addedPointerEventEnabled) {
    Preferences::AddBoolVarCache(&sPointerEventEnabled,
                                 "dom.w3c_pointer_events.enabled", true);
    addedPointerEventEnabled = true;
  }
}

/* Load and shutdown */
void
WinPointerEvents::InitLibrary()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!IsWin8OrLater()) {
    // Only Win8 or later supports WM_POINTER*
    return;
  }
  if (getPointerType) {
    // Return if we already initialized the PointerEvent related interfaces
    return;
  }
  sLibraryHandle = ::LoadLibraryW(kPointerLibraryName);
  MOZ_ASSERT(sLibraryHandle, "cannot load pointer library");
  if (sLibraryHandle) {
    getPointerType =
      (GetPointerTypePtr)GetProcAddress(sLibraryHandle, "GetPointerType");
    getPointerInfo =
      (GetPointerInfoPtr)GetProcAddress(sLibraryHandle, "GetPointerInfo");
    getPointerPenInfo =
      (GetPointerPenInfoPtr)GetProcAddress(sLibraryHandle, "GetPointerPenInfo");
  }

  if (!getPointerType || !getPointerInfo || !getPointerPenInfo) {
    MOZ_ASSERT(false, "get PointerEvent interfaces failed");
    getPointerType = nullptr;
    getPointerInfo = nullptr;
    getPointerPenInfo = nullptr;
    return;
  }
}

bool
WinPointerEvents::ShouldFireCompatibilityMouseEventsForPen(WPARAM aWParam)
{
  if (!sLibraryHandle || !sPointerEventEnabled) {
    // Firing mouse events by handling Windows WM_POINTER* when preference is on
    // and the Windows platform supports PointerEvent related interfaces.
    return false;
  }

  uint32_t pointerId = GetPointerId(aWParam);
  POINTER_INPUT_TYPE pointerType = PT_POINTER;
  if (!GetPointerType(pointerId, &pointerType)) {
    MOZ_ASSERT(false, "cannot find PointerType");
    return false;
  }
  return (pointerType == PT_PEN);
}

bool
WinPointerEvents::GetPointerType(uint32_t aPointerId,
                                 POINTER_INPUT_TYPE *aPointerType)
{
  if (!getPointerType) {
    return false;
  }
  return getPointerType(aPointerId, aPointerType);
}

bool
WinPointerEvents::GetPointerInfo(uint32_t aPointerId,
                                 POINTER_INFO *aPointerInfo)
{
  if (!getPointerInfo) {
    return false;
  }
  return getPointerInfo(aPointerId, aPointerInfo);
}

bool
WinPointerEvents::GetPointerPenInfo(uint32_t aPointerId,
                                    POINTER_PEN_INFO *aPenInfo)
{
  if (!getPointerPenInfo) {
    return false;
  }
  return getPointerPenInfo(aPointerId, aPenInfo);
}
