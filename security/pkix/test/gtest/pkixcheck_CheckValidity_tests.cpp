/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2014 Mozilla Contributors
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

#include "pkixcheck.h"
#include "pkixgtest.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

static const Time PAST_TIME(YMDHMS(1998, 12, 31, 12, 23, 56));

#define OLDER_GENERALIZEDTIME \
  0x18, 15,                               /* tag, length */ \
  '1', '9', '9', '9', '0', '1', '0', '1', /* 1999-01-01 */ \
  '0', '0', '0', '0', '0', '0', 'Z'       /* 00:00:00Z */

#define OLDER_UTCTIME \
  0x17, 13,                               /* tag, length */ \
  '9', '9', '0', '1', '0', '1',           /* (19)99-01-01 */ \
  '0', '0', '0', '0', '0', '0', 'Z'       /* 00:00:00Z */

static const Time NOW(YMDHMS(2016, 12, 31, 12, 23, 56));

#define NEWER_GENERALIZEDTIME \
  0x18, 15,                               /* tag, length */ \
  '2', '0', '2', '1', '0', '1', '0', '1', /* 2021-01-01 */ \
  '0', '0', '0', '0', '0', '0', 'Z'       /* 00:00:00Z */

#define NEWER_UTCTIME \
  0x17, 13,                               /* tag, length */ \
  '2', '1', '0', '1', '0', '1',           /* 2021-01-01 */ \
  '0', '0', '0', '0', '0', '0', 'Z'       /* 00:00:00Z */

static const Time FUTURE_TIME(YMDHMS(2025, 12, 31, 12, 23, 56));

class pkixcheck_CheckValidity : public ::testing::Test { };

static const uint8_t OLDER_UTCTIME_NEWER_UTCTIME_DATA[] = {
  OLDER_UTCTIME,
  NEWER_UTCTIME,
};
static const Input
OLDER_UTCTIME_NEWER_UTCTIME(OLDER_UTCTIME_NEWER_UTCTIME_DATA);

TEST_F(pkixcheck_CheckValidity, Valid_UTCTIME_UTCTIME)
{
  static Time notBefore(Time::uninitialized);
  static Time notAfter(Time::uninitialized);
  ASSERT_EQ(Success, ParseValidity(OLDER_UTCTIME_NEWER_UTCTIME, &notBefore, &notAfter));
  ASSERT_EQ(Success, CheckValidity(NOW, notBefore, notAfter));
}

TEST_F(pkixcheck_CheckValidity, Valid_GENERALIZEDTIME_GENERALIZEDTIME)
{
  static const uint8_t DER[] = {
    OLDER_GENERALIZEDTIME,
    NEWER_GENERALIZEDTIME,
  };
  static const Input validity(DER);
  static Time notBefore(Time::uninitialized);
  static Time notAfter(Time::uninitialized);
  ASSERT_EQ(Success, ParseValidity(validity, &notBefore, &notAfter));
  ASSERT_EQ(Success, CheckValidity(NOW, notBefore, notAfter));
}

TEST_F(pkixcheck_CheckValidity, Valid_GENERALIZEDTIME_UTCTIME)
{
  static const uint8_t DER[] = {
    OLDER_GENERALIZEDTIME,
    NEWER_UTCTIME,
  };
  static const Input validity(DER);
  static Time notBefore(Time::uninitialized);
  static Time notAfter(Time::uninitialized);
  ASSERT_EQ(Success, ParseValidity(validity, &notBefore, &notAfter));
  ASSERT_EQ(Success, CheckValidity(NOW, notBefore, notAfter));
}

TEST_F(pkixcheck_CheckValidity, Valid_UTCTIME_GENERALIZEDTIME)
{
  static const uint8_t DER[] = {
    OLDER_UTCTIME,
    NEWER_GENERALIZEDTIME,
  };
  static const Input validity(DER);
  static Time notBefore(Time::uninitialized);
  static Time notAfter(Time::uninitialized);
  ASSERT_EQ(Success, ParseValidity(validity, &notBefore, &notAfter));
  ASSERT_EQ(Success, CheckValidity(NOW, notBefore, notAfter));
}

TEST_F(pkixcheck_CheckValidity, InvalidBeforeNotBefore)
{
  static Time notBefore(Time::uninitialized);
  static Time notAfter(Time::uninitialized);
  ASSERT_EQ(Success, ParseValidity(OLDER_UTCTIME_NEWER_UTCTIME, &notBefore, &notAfter));
  ASSERT_EQ(Result::ERROR_NOT_YET_VALID_CERTIFICATE, CheckValidity(PAST_TIME, notBefore, notAfter));
}

TEST_F(pkixcheck_CheckValidity, InvalidAfterNotAfter)
{
  static Time notBefore(Time::uninitialized);
  static Time notAfter(Time::uninitialized);
  ASSERT_EQ(Success, ParseValidity(OLDER_UTCTIME_NEWER_UTCTIME, &notBefore, &notAfter));
  ASSERT_EQ(Result::ERROR_EXPIRED_CERTIFICATE, CheckValidity(FUTURE_TIME, notBefore, notAfter));
}
