/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2015 Mozilla Contributors
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

#include "pkixder.h"
#include "pkixgtest.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

namespace mozilla { namespace pkix {
  extern Result TLSFeaturesSatisfiedInternal(const Input* requiredTLSFeatures,
                                             const Input* stapledOCSPResponse);
} } // namespace mozilla::pkix

struct TLSFeaturesTestParams
{
  ByteString requiredTLSFeatures;
  Result expectedResultWithResponse;
  Result expectedResultWithoutResponse;
};

::std::ostream& operator<<(::std::ostream& os, const TLSFeaturesTestParams&)
{
  return os << "TODO (bug 1318770)";
}

#define BS(s) ByteString(s, MOZILLA_PKIX_ARRAY_LENGTH(s))
static const uint8_t statusRequest[] = {
  0x30, 0x03, 0x02, 0x01, 0x05
};

static const uint8_t unknown[] = {
  0x30, 0x03, 0x02, 0x01, 0x06
};

static const uint8_t statusRequestAndUnknown[] = {
  0x30, 0x06, 0x02, 0x01, 0x05, 0x02, 0x01, 0x06
};

static const uint8_t duplicateStatusRequest[] = {
  0x30, 0x06, 0x02, 0x01, 0x05, 0x02, 0x01, 0x05
};

static const uint8_t twoByteUnknown[] = {
  0x30, 0x04, 0x02, 0x02, 0x05, 0x05
};

static const uint8_t zeroByteInteger[] = {
  0x30, 0x02, 0x02, 0x00
};

static const TLSFeaturesTestParams
  TLSFEATURESSATISFIED_TEST_PARAMS[] =
{
  // some tests with checks enforced
  { ByteString(), Result::ERROR_BAD_DER, Result::ERROR_BAD_DER },
  { BS(statusRequest), Success, Result::ERROR_REQUIRED_TLS_FEATURE_MISSING },
  { BS(unknown), Result::ERROR_REQUIRED_TLS_FEATURE_MISSING,
    Result::ERROR_REQUIRED_TLS_FEATURE_MISSING },
  { BS(statusRequestAndUnknown), Result::ERROR_REQUIRED_TLS_FEATURE_MISSING,
    Result::ERROR_REQUIRED_TLS_FEATURE_MISSING },
  { BS(duplicateStatusRequest), Success,
    Result::ERROR_REQUIRED_TLS_FEATURE_MISSING },
  { BS(twoByteUnknown), Result::ERROR_REQUIRED_TLS_FEATURE_MISSING,
    Result::ERROR_REQUIRED_TLS_FEATURE_MISSING },
  { BS(zeroByteInteger), Result::ERROR_REQUIRED_TLS_FEATURE_MISSING,
    Result::ERROR_REQUIRED_TLS_FEATURE_MISSING },
};

class pkixcheck_TLSFeaturesSatisfiedInternal
  : public ::testing::Test
  , public ::testing::WithParamInterface<TLSFeaturesTestParams>
{
};

TEST_P(pkixcheck_TLSFeaturesSatisfiedInternal, TLSFeaturesSatisfiedInternal) {
  const TLSFeaturesTestParams& params(GetParam());

  Input featuresInput;
  ASSERT_EQ(Success, featuresInput.Init(params.requiredTLSFeatures.data(),
                                        params.requiredTLSFeatures.length()));
  Input responseInput;
  // just create an input with any data in it
  ByteString stapledOCSPResponse = BS(statusRequest);
  ASSERT_EQ(Success, responseInput.Init(stapledOCSPResponse.data(),
                                        stapledOCSPResponse.length()));
  // first we omit the response
  ASSERT_EQ(params.expectedResultWithoutResponse,
            TLSFeaturesSatisfiedInternal(&featuresInput, nullptr));
  // then we try again with the response
  ASSERT_EQ(params.expectedResultWithResponse,
            TLSFeaturesSatisfiedInternal(&featuresInput, &responseInput));
}

INSTANTIATE_TEST_CASE_P(
  pkixcheck_TLSFeaturesSatisfiedInternal,
  pkixcheck_TLSFeaturesSatisfiedInternal,
  testing::ValuesIn(TLSFEATURESSATISFIED_TEST_PARAMS));
