/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FOGIPC_h__
#define FOGIPC_h__

#include <functional>

#include "mozilla/Maybe.h"
#include "nsTArrayForwardDeclare.h"

namespace mozilla {
namespace ipc {
class ByteBuf;
}  // namespace ipc
}  // namespace mozilla

// This module provides the interface for FOG to communicate between processes.

namespace mozilla {
namespace glean {

/**
 * The parent process is asking you to flush your data ASAP.
 *
 * @param aResolver - The function you need to call with the bincoded,
 *                    serialized payload that the Rust impl hands you.
 */
void FlushFOGData(std::function<void(ipc::ByteBuf&&)>&& aResolver);

/**
 * Called by FOG on the parent process when it wants to flush all its
 * children's data.
 * @param aResolver - The function that'll be called with the results.
 */
void FlushAllChildData(
    std::function<void(const nsTArray<ipc::ByteBuf>&&)>&& aResolver);

/**
 * A child process has sent you this buf as a treat.
 * @param buf - a bincoded serialized payload that the Rust impl understands.
 */
void FOGData(ipc::ByteBuf&& buf);

/**
 * Called by FOG on a child process when it wants to send a buf to the parent.
 * @param buf - a bincoded serialized payload that the Rust impl understands.
 */
void SendFOGData(ipc::ByteBuf&& buf);

}  // namespace glean
}  // namespace mozilla

#endif  // FOGIPC_h__
