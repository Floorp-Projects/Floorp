/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com
#include "logging.h"
#include "transportflow.h"
#include "transportlayer.h"
#include "nsThreadUtils.h"

// Logging context
namespace mozilla {

MOZ_MTLOG_MODULE("mtransport")

nsresult TransportLayer::Init() {
  if (state_ != TS_NONE)
    return state_ == TS_ERROR ? NS_ERROR_FAILURE : NS_OK;

  nsresult rv = InitInternal();

  if (!NS_SUCCEEDED(rv)) {
    state_ = TS_ERROR;
    return rv;
  }
  state_ = TS_INIT;

  return NS_OK;
}

void TransportLayer::Inserted(TransportFlow *flow, TransportLayer *downward) {
  downward_ = downward;
  flow_id_ = flow->id();
  MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Inserted: downward='" <<
    (downward ? downward->id(): "none") << "'");

  WasInserted();
}

void TransportLayer::SetState(State state, const char *file, unsigned line) {
  if (state != state_) {
    MOZ_MTLOG(state == TS_ERROR ? ML_ERROR : ML_DEBUG,
              file << ":" << line << ": " <<
              LAYER_INFO << "state " << state_ << "->" << state);
    state_ = state;
    SignalStateChange(this, state);
  }
}

nsresult TransportLayer::RunOnThread(nsIRunnable *event) {
  if (target_) {
    nsIThread *thr;

    DebugOnly<nsresult> rv = NS_GetCurrentThread(&thr);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    if (target_ != thr) {
      return target_->Dispatch(event, NS_DISPATCH_SYNC);
    }
  }

  return event->Run();
}

}  // close namespace
