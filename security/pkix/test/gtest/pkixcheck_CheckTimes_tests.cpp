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

#include "pkixgtest.h"
#include "pkixtestutil.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

namespace mozilla { namespace pkix {

extern Result CheckTimes(const CERTValidity& validity, PRTime time);

} } // namespace mozilla::pkix

static const SECItem empty_null = { siBuffer, nullptr, 0 };

static const PRTime PAST_TIME(YMDHMS(1998, 12, 31, 12, 23, 56));

static const uint8_t OLDER_GENERALIZEDTIME_DATA[] = {
  '1', '9', '9', '9', '0', '1', '0', '1', // 1999-01-01
  '0', '0', '0', '0', '0', '0', 'Z'       // 00:00:00Z
};
static const SECItem OLDER_GENERALIZEDTIME = {
  siGeneralizedTime,
  const_cast<uint8_t*>(OLDER_GENERALIZEDTIME_DATA),
  sizeof(OLDER_GENERALIZEDTIME_DATA)
};

static const uint8_t OLDER_UTCTIME_DATA[] = {
  '9', '9', '0', '1', '0', '1',           // (19)99-01-01
  '0', '0', '0', '0', '0', '0', 'Z'       // 00:00:00Z
};
static const SECItem OLDER_UTCTIME = {
  siUTCTime,
  const_cast<uint8_t*>(OLDER_UTCTIME_DATA),
  sizeof(OLDER_UTCTIME_DATA)
};

static const PRTime NOW(YMDHMS(2016, 12, 31, 12, 23, 56));

static const uint8_t NEWER_GENERALIZEDTIME_DATA[] = {
  '2', '0', '2', '1', '0', '1', '0', '1', // 2021-01-01
  '0', '0', '0', '0', '0', '0', 'Z'       // 00:00:00Z
};
static const SECItem NEWER_GENERALIZEDTIME = {
  siGeneralizedTime,
  const_cast<uint8_t*>(NEWER_GENERALIZEDTIME_DATA),
  sizeof(NEWER_GENERALIZEDTIME_DATA)
};

static const uint8_t NEWER_UTCTIME_DATA[] = {
  '2', '1', '0', '1', '0', '1',           // 2021-01-01
  '0', '0', '0', '0', '0', '0', 'Z'       // 00:00:00Z
};
static const SECItem NEWER_UTCTIME = {
  siUTCTime,
  const_cast<uint8_t*>(NEWER_UTCTIME_DATA),
  sizeof(NEWER_UTCTIME_DATA)
};

static const PRTime FUTURE_TIME(YMDHMS(2025, 12, 31, 12, 23, 56));



class pkixcheck_CheckTimes : public ::testing::Test
{
public:
  virtual void SetUp()
  {
    PR_SetError(0, 0);
  }
};

TEST_F(pkixcheck_CheckTimes, BothEmptyNull)
{
  static const CERTValidity validity = { nullptr, empty_null, empty_null };
  ASSERT_RecoverableError(SEC_ERROR_EXPIRED_CERTIFICATE,
                          CheckTimes(validity, NOW));
}

TEST_F(pkixcheck_CheckTimes, NotBeforeEmptyNull)
{
  static const CERTValidity validity = { nullptr, empty_null, NEWER_UTCTIME };
  ASSERT_RecoverableError(SEC_ERROR_EXPIRED_CERTIFICATE,
                          CheckTimes(validity, NOW));
}

TEST_F(pkixcheck_CheckTimes, NotAfterEmptyNull)
{
  static const CERTValidity validity = { nullptr, OLDER_UTCTIME, empty_null };
  ASSERT_RecoverableError(SEC_ERROR_EXPIRED_CERTIFICATE,
                          CheckTimes(validity, NOW));
}

TEST_F(pkixcheck_CheckTimes, Valid_UTCTIME_UTCTIME)
{
  static const CERTValidity validity = {
    nullptr, OLDER_UTCTIME, NEWER_UTCTIME
  };
  ASSERT_Success(CheckTimes(validity, NOW));
}

TEST_F(pkixcheck_CheckTimes, Valid_GENERALIZEDTIME_GENERALIZEDTIME)
{
  static const CERTValidity validity = {
    nullptr, OLDER_GENERALIZEDTIME, NEWER_GENERALIZEDTIME
  };
  ASSERT_Success(CheckTimes(validity, NOW));
}

TEST_F(pkixcheck_CheckTimes, Valid_GENERALIZEDTIME_UTCTIME)
{
  static const CERTValidity validity = {
    nullptr, OLDER_GENERALIZEDTIME, NEWER_UTCTIME
  };
  ASSERT_Success(CheckTimes(validity, NOW));
}

TEST_F(pkixcheck_CheckTimes, Valid_UTCTIME_GENERALIZEDTIME)
{
  static const CERTValidity validity = {
    nullptr, OLDER_UTCTIME, NEWER_GENERALIZEDTIME
  };
  ASSERT_Success(CheckTimes(validity, NOW));
}

TEST_F(pkixcheck_CheckTimes, InvalidBeforeNotBefore)
{
  static const CERTValidity validity = {
    nullptr, OLDER_UTCTIME, NEWER_UTCTIME
  };
  ASSERT_RecoverableError(SEC_ERROR_EXPIRED_CERTIFICATE,
                          CheckTimes(validity, PAST_TIME));
}

TEST_F(pkixcheck_CheckTimes, InvalidAfterNotAfter)
{
  static const CERTValidity validity = {
    nullptr, OLDER_UTCTIME, NEWER_UTCTIME
  };
  ASSERT_RecoverableError(SEC_ERROR_EXPIRED_CERTIFICATE,
                          CheckTimes(validity, FUTURE_TIME));
}

TEST_F(pkixcheck_CheckTimes, InvalidNotAfterBeforeNotBefore)
{
  static const CERTValidity validity = {
    nullptr, NEWER_UTCTIME, OLDER_UTCTIME
  };
  ASSERT_RecoverableError(SEC_ERROR_EXPIRED_CERTIFICATE,
                          CheckTimes(validity, NOW));
}
