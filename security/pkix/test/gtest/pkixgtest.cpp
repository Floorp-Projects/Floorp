/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2013 Mozilla Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pkixgtest.h"

#include <ctime>

#include "pkix/Time.h"

namespace mozilla { namespace pkix { namespace test {

const std::time_t ONE_DAY_IN_SECONDS_AS_TIME_T =
  static_cast<std::time_t>(Time::ONE_DAY_IN_SECONDS);

// This assumes that time/time_t are POSIX-compliant in that time() returns
// the number of seconds since the Unix epoch.
const std::time_t now(time(nullptr));
const std::time_t oneDayBeforeNow(time(nullptr) -
                                  ONE_DAY_IN_SECONDS_AS_TIME_T);
const std::time_t oneDayAfterNow(time(nullptr) +
                                 ONE_DAY_IN_SECONDS_AS_TIME_T);

} } } // namespace mozilla::pkix::test
