/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MINIDUMP_ANALYZER_H__
#define MINIDUMP_ANALYZER_H__

#include <string>

namespace CrashReporter {

bool GenerateStacks(const std::string& aDumpPath, const bool aFullStacks);

}

#endif  // MINIDUMP_ANALYZER_H__
