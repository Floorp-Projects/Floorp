/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBRTCLOG_H_
#define WEBRTCLOG_H_

#include "webrtc/common_types.h"

void StartWebRtcLog(uint32_t log_level = webrtc::kTraceDefault);
void EnableWebRtcLog();
void StopWebRtcLog();

#endif
