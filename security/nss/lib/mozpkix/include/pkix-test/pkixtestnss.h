/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2018 Mozilla Contributors
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

// This file provides some implementation-specific test utilities. This is only
// necessary because some PSM xpcshell test utilities overlap in functionality
// with these test utilities, so the underlying implementation is shared.

#ifndef mozilla_pkix_test_pkixtestnss_h
#define mozilla_pkix_test_pkixtestnss_h

#include <keyhi.h>
#include <keythi.h>
#include "mozpkix/nss_scoped_ptrs.h"
#include "mozpkix/test/pkixtestutil.h"

namespace mozilla {
namespace pkix {
namespace test {

TestKeyPair* CreateTestKeyPair(const TestPublicKeyAlgorithm publicKeyAlg,
                               const ScopedSECKEYPublicKey& publicKey,
                               const ScopedSECKEYPrivateKey& privateKey);
}
}
}  // namespace mozilla::pkix::test

#endif  // mozilla_pkix_test_pkixtestnss_h
