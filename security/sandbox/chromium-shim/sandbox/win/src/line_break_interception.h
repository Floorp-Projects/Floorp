/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SANDBOX_SRC_LINE_BREAK_INTERCEPTION_H_
#define SANDBOX_SRC_LINE_BREAK_INTERCEPTION_H_

#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

ResultCode GetComplexLineBreaksProxy(const wchar_t* text, uint32_t length,
                                     uint8_t* break_before);

}  // namespace sandbox

#endif  // SANDBOX_SRC_LINE_BREAK_INTERCEPTION_H_
