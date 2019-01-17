/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_glue_UntrustedDllsHandler_h
#define mozilla_glue_UntrustedDllsHandler_h

#include <windows.h>
#include <winternl.h>  // For PUNICODE_STRING

#include "mozilla/Attributes.h"
#include "mozilla/glue/WindowsDllServices.h"  // For ModuleLoadEvent
#include "mozilla/Vector.h"

namespace mozilla {
namespace glue {

// This class examines successful module loads, converts them to a
// glue::ModuleLoadEvent, and stores them for consumption by the caller.
// In the process, we capture data about the event that is later used to
// evaluate trustworthiness of the module.
class UntrustedDllsHandler {
 public:
  static void Init();

#ifdef DEBUG
  static void Shutdown();
#endif  // DEBUG

  /**
   * To prevent issues with re-entrancy, we only capture module load events at
   * the top-level call. Recursive module loads get bundled with the top-level
   * module load. In order to do that, we must track when LdrLoadDLL() enters
   * and exits, to detect recursion.
   *
   * EnterLoaderCall() must be called at the top of LdrLoadDLL(), and
   * ExitLoaderCall() must be called when LdrLoadDLL() exits.
   *
   * These methods may be called even outside of Init() / Shutdown().
   */
  static void EnterLoaderCall();
  static void ExitLoaderCall();

  /**
   * OnAfterModuleLoad should be called between calls to EnterLoaderCall() and
   * ExitLoaderCall(). This handles the successful module load and records it
   * internally if needed. To handle the resulting recorded event, call
   * TakePendingEvents().
   *
   * This method may be called even outside of Init() / Shutdown().
   */
  static void OnAfterModuleLoad(uintptr_t aBaseAddr,
                                PUNICODE_STRING aLdrModuleName,
                                double aLoadDurationMS);

  /**
   * Call TakePendingEvents to get any events that have been recorded since
   * the last call to TakePendingEvents().
   *
   * This method may be called even outside of Init() / Shutdown().
   *
   * @param  aOut [out] Receives a list of events.
   * @return true if aOut now contains elements.
   */
  static bool TakePendingEvents(
      Vector<ModuleLoadEvent, 0, InfallibleAllocPolicy>& aOut);
};

}  // namespace glue
}  // namespace mozilla

#endif  // mozilla_glue_UntrustedDllsHandler_h
