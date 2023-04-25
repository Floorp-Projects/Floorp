/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is for support functions for the Rust IPC module.
// Some information just isn't available to Rust and must be made available over
// FFI.
#include "FOGIPC.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Unused.h"
#include "nsThreadUtils.h"

using mozilla::AppShutdown;
using mozilla::RunOnShutdown;
using mozilla::ShutdownPhase;
using mozilla::Unused;
using mozilla::glean::FlushFOGData;
using mozilla::glean::SendFOGData;
using mozilla::ipc::ByteBuf;

extern "C" {
void FOG_RegisterContentChildShutdown() {
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed)) {
    return;
  }

  // If there is no main thread (too early in startup or too late in shutdown),
  // there's nothing we can do but log.
  bool failed =
      NS_FAILED(NS_DispatchToMainThread(NS_NewRunnableFunction(__func__, [] {
        // By the time the main thread dispatched this, it may already be too
        // late.
        if (AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed)) {
          return;
        }
        RunOnShutdown(
            [] {
              FlushFOGData(
                  [](ByteBuf&& aBuf) { SendFOGData(std::move(aBuf)); });
            },
            ShutdownPhase::AppShutdownConfirmed);
      })));
  if (failed) {
    NS_WARNING(
        "Failed to register FOG content child shutdown flush. "
        "Will lose shutdown data and leak a runnable.");
    mozilla::glean::fog_ipc::shutdown_registration_failures.Add(1);
  }
}

int FOG_GetProcessType() { return XRE_GetProcessType(); }

/**
 * Called from FOG IPC in Rust when the IPC Payload might be getting full.
 * We should probably flush before we reach the max IPC message size.
 */
void FOG_IPCPayloadFull() {
  // NS_DispatchToMainThread can leak the runnable (bug 1787589), so let's be
  // sure not to create it too late in shutdown.
  // We choose XPCOMShutdown to match gFOG->Shutdown().
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMShutdown)) {
    return;
  }
  // FOG IPC must happen on the main thread until bug 1641989.
  // If there is no main thread (too early in startup or too late in shutdown),
  // there's nothing we can do but log.
  Unused << NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(
      NS_NewRunnableFunction("FOG IPC Payload getting full", [] {
        FlushFOGData([](ByteBuf&& aBuf) { SendFOGData(std::move(aBuf)); });
      }))));
}
}
