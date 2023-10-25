// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "content_analysis/sdk/result_codes.h"

namespace content_analysis {
namespace sdk {

#define ERR_TO_RC(ERR, RC) case ERR: return ResultCode::RC;

ResultCode ErrorToResultCode(DWORD err) {
  switch (err) {
  ERR_TO_RC(ERROR_SUCCESS, OK);
  ERR_TO_RC(ERROR_ACCESS_DENIED, ERR_AGENT_ALREADY_EXISTS);
  ERR_TO_RC(ERROR_BROKEN_PIPE, ERR_BROKEN_PIPE);
  ERR_TO_RC(ERROR_INVALID_NAME, ERR_INVALID_CHANNEL_NAME);
  ERR_TO_RC(ERROR_MORE_DATA, ERR_MORE_DATA);
  ERR_TO_RC(ERROR_IO_PENDING, ERR_IO_PENDING);
  default:
    return ResultCode::ERR_UNEXPECTED;
  }
}

}  // namespace sdk
}  // namespace content_analysis
