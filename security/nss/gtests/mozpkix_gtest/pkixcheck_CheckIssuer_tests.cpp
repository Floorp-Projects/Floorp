/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2016 Mozilla Contributors
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

#include "mozpkix/pkixcheck.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

class pkixcheck_CheckIssuer : public ::testing::Test { };

static const uint8_t EMPTY_NAME_DATA[] = {
  0x30, 0x00 /* tag, length */
};
static const Input EMPTY_NAME(EMPTY_NAME_DATA);

static const uint8_t VALID_NAME_DATA[] = {
  /* From https://www.example.com/: C=US, O=DigiCert Inc, OU=www.digicert.com,
   * CN=DigiCert SHA2 High Assurance Server CA */
  0x30, 0x70, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
  0x02, 0x55, 0x53, 0x31, 0x15, 0x30, 0x13, 0x06, 0x03, 0x55, 0x04, 0x0A,
  0x13, 0x0C, 0x44, 0x69, 0x67, 0x69, 0x43, 0x65, 0x72, 0x74, 0x20, 0x49,
  0x6E, 0x63, 0x31, 0x19, 0x30, 0x17, 0x06, 0x03, 0x55, 0x04, 0x0B, 0x13,
  0x10, 0x77, 0x77, 0x77, 0x2E, 0x64, 0x69, 0x67, 0x69, 0x63, 0x65, 0x72,
  0x74, 0x2E, 0x63, 0x6F, 0x6D, 0x31, 0x2F, 0x30, 0x2D, 0x06, 0x03, 0x55,
  0x04, 0x03, 0x13, 0x26, 0x44, 0x69, 0x67, 0x69, 0x43, 0x65, 0x72, 0x74,
  0x20, 0x53, 0x48, 0x41, 0x32, 0x20, 0x48, 0x69, 0x67, 0x68, 0x20, 0x41,
  0x73, 0x73, 0x75, 0x72, 0x61, 0x6E, 0x63, 0x65, 0x20, 0x53, 0x65, 0x72,
  0x76, 0x65, 0x72, 0x20, 0x43, 0x41
};
static const Input VALID_NAME(VALID_NAME_DATA);

TEST_F(pkixcheck_CheckIssuer, ValidIssuer)
{
  ASSERT_EQ(Success, CheckIssuer(VALID_NAME));
}

TEST_F(pkixcheck_CheckIssuer, EmptyIssuer)
{
  ASSERT_EQ(Result::ERROR_EMPTY_ISSUER_NAME, CheckIssuer(EMPTY_NAME));
}

TEST_F(pkixcheck_CheckIssuer, TrailingData)
{
  static const uint8_t validNameData[] = {
    0x30/*SEQUENCE*/, 0x02/*LENGTH=2*/,
      0x31, 0x00 // the contents of the sequence aren't validated
  };
  static const Input validName(validNameData);
  ASSERT_EQ(Success, CheckIssuer(validName));

  static const uint8_t trailingDataData[] = {
    0x30/*SEQUENCE*/, 0x02/*LENGTH=2*/,
      0x31, 0x00, // the contents of the sequence aren't validated
    0x77 // trailing data is invalid
  };
  static const Input trailingData(trailingDataData);
  ASSERT_EQ(Result::ERROR_BAD_DER, CheckIssuer(trailingData));
}

TEST_F(pkixcheck_CheckIssuer, InvalidContents)
{
  static const uint8_t invalidContentsData[] = {
    0x31/*SET (should be SEQUENCE)*/, 0x02/*LENGTH=2*/,
      0x31, 0x00
  };
  static const Input invalidContents(invalidContentsData);
  ASSERT_EQ(Result::ERROR_BAD_DER, CheckIssuer(invalidContents));
}
