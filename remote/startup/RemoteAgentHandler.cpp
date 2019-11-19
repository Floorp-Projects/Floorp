/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "nsCOMPtr.h"
#include "nsICommandLineHandler.h"
#include "nsISupportsUtils.h"

#include "RemoteAgentHandler.h"

// anonymous namespace prevents outside C++ code
// from improperly accessing these implementation details
namespace {
extern "C" {
// implemented in Rust, see handler.rs
void new_remote_agent_handler(nsICommandLineHandler** result);
}

static mozilla::StaticRefPtr<nsICommandLineHandler> sHandler;
}  // namespace

already_AddRefed<nsICommandLineHandler> GetRemoteAgentHandler() {
  nsCOMPtr<nsICommandLineHandler> handler;

  if (sHandler) {
    handler = sHandler;
  } else {
    new_remote_agent_handler(getter_AddRefs(handler));
    sHandler = handler;
    mozilla::ClearOnShutdown(&sHandler);
  }

  return handler.forget();
}
