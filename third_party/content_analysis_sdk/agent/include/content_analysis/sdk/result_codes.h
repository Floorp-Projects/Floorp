// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_AGENT_INCLUDE_CONTENT_ANALYSIS_SDK_RESULT_CODES_H_
#define CONTENT_ANALYSIS_AGENT_INCLUDE_CONTENT_ANALYSIS_SDK_RESULT_CODES_H_

#include <stdint.h>

namespace content_analysis {
namespace sdk {

// Result codes of methods and functions.

#define RC_RECOVERABLE(RC, MSG) RC,
#define RC_UNRECOVERABLE(RC, MSG) RC,
enum class ResultCode {
#include "content_analysis/sdk/result_codes.inc"
};
#undef RC_RECOVERABLE
#undef RC_UNRECOVERABLE

// Returns true if the error is recoverable.  A recoverable errors means the
// agent may still receive new requests from Google Chrome.  An unrecoverable
// error means the agent is unlikely to get more request from Google Chrome.
inline bool IsRecoverableError(ResultCode rc) {
  return rc < ResultCode::ERR_FIRST_UNRECOVERABLE_ERROR;
}

// Returns a human readable error for the given result code.
const char* ResultCodeToString(ResultCode rc);

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_AGENT_INCLUDE_CONTENT_ANALYSIS_SDK_RESULT_CODES_H_
