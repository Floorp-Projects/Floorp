/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is for support functions for the Rust IPC module.
// Some information just isn't available to Rust and must be made available over
// FFI.
#include "FOGIPC.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsThreadUtils.h"

using mozilla::RunOnShutdown;
using mozilla::ShutdownPhase;
using mozilla::glean::FlushFOGData;
using mozilla::glean::SendFOGData;
using mozilla::ipc::ByteBuf;

extern "C" {
void FOG_RegisterContentChildShutdown() {
  RunOnShutdown(
      [] {
        FlushFOGData([](ByteBuf&& aBuf) { SendFOGData(std::move(aBuf)); });
      },
      ShutdownPhase::AppShutdownConfirmed);
}

int FOG_GetProcessType() { return XRE_GetProcessType(); }

/**
 * Called from FOG IPC in Rust when the IPC Payload might be getting full.
 * We should probably flush before we reach the max IPC message size.
 */
void FOG_IPCPayloadFull() {
  // FOG IPC must happen on the main thread until bug 1641989.
  NS_DispatchToMainThread(
      NS_NewRunnableFunction("FOG IPC Payload getting full", [] {
        FlushFOGData([](ByteBuf&& aBuf) { SendFOGData(std::move(aBuf)); });
      }));
}
}
