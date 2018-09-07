/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DocumentAnalyticsTrackerFastBlocked_h__
#define DocumentAnalyticsTrackerFastBlocked_h__

#include "ipc/IPCMessageUtils.h"
#include "mozilla/TelemetryHistogramEnums.h"

namespace IPC {

  template <>
  struct ParamTraits<mozilla::Telemetry::LABELS_DOCUMENT_ANALYTICS_TRACKER_FASTBLOCKED> :
    public ContiguousEnumSerializer<mozilla::Telemetry::LABELS_DOCUMENT_ANALYTICS_TRACKER_FASTBLOCKED,
                                    mozilla::Telemetry::LABELS_DOCUMENT_ANALYTICS_TRACKER_FASTBLOCKED::other,
                                    (mozilla::Telemetry::LABELS_DOCUMENT_ANALYTICS_TRACKER_FASTBLOCKED)
                                      (uint32_t(mozilla::Telemetry::LABELS_DOCUMENT_ANALYTICS_TRACKER_FASTBLOCKED::all) + 1)>
  {};

}

#endif
