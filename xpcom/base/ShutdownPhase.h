/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ShutdownPhase_h
#define ShutdownPhase_h

namespace mozilla {

// Must be contiguous starting at 0
enum class ShutdownPhase {
  NotInShutdown = 0,
  AppShutdownConfirmed,
  AppShutdownNetTeardown,
  AppShutdownTeardown,
  AppShutdown,
  AppShutdownQM,
  AppShutdownTelemetry,
  XPCOMWillShutdown,
  XPCOMShutdown,
  XPCOMShutdownThreads,
  XPCOMShutdownFinal,
  CCPostLastCycleCollection,
  ShutdownPhase_Length,         // never pass this value
  First = AppShutdownConfirmed  // for iteration
};

}  // namespace mozilla

#endif  // ShutdownPhase_h
