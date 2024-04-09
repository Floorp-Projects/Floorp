/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Perfetto.h"
#include <stdlib.h>

PERFETTO_TRACK_EVENT_STATIC_STORAGE();

void InitPerfetto() {
  if (!getenv("MOZ_DISABLE_PERFETTO")) {
    perfetto::TracingInitArgs args;
    args.backends |= perfetto::kSystemBackend;
    perfetto::Tracing::Initialize(args);
    perfetto::TrackEvent::Register();
  }
}
