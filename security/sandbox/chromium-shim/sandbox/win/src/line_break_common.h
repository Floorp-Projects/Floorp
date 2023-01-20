/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SANDBOX_SRC_LINE_BREAK_COMMON_H_
#define SANDBOX_SRC_LINE_BREAK_COMMON_H_

#include "sandbox/win/src/crosscall_params.h"

namespace sandbox {

#if defined(MOZ_DEBUG)
// Set a low max brokered length for testing to exercise the chunking code.
static const std::ptrdiff_t kMaxBrokeredLen = 50;

#else
// Parameters are stored aligned to sizeof(int64_t).
// So to calculate the maximum length we can use when brokering to the parent,
// we take the max params buffer size, take off 8 for the aligned length and 6
// and 7 to allow for the maximum padding that can be added to the text and
// break before buffers. We then divide by three, because the text characters
// are wchar_t and the break before elements are uint8_t.
static const std::ptrdiff_t kMaxBrokeredLen =
    (ActualCallParams<3, kIPCChannelSize>::MaxParamsSize() - 8 - 6 - 7) / 3;
#endif

}  // namespace sandbox

#endif  // SANDBOX_SRC_LINE_BREAK_COMMON_H_
