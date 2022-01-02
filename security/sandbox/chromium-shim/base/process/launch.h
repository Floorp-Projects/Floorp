/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a reduced version of Chromium's //base/process/launch.h
// to satisfy compiler.

#ifndef BASE_PROCESS_LAUNCH_H_
#define BASE_PROCESS_LAUNCH_H_

#include <vector>

#include "base/environment.h"

namespace base {

#if defined(OS_WIN)
typedef std::vector<HANDLE> HandlesToInheritVector;
#endif

}  // namespace base

#endif  // BASE_PROCESS_LAUNCH_H_
