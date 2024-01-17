/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EnterpriseRoots_h
#define EnterpriseRoots_h

#include "ScopedNSSTypes.h"
#include "mozpkix/Input.h"
#include "mozpkix/Result.h"
#include "nsTArray.h"

class EnterpriseCert {
 public:
  EnterpriseCert(const uint8_t* data, size_t len, bool isRoot)
      : mDER(data, len), mIsRoot(isRoot) {}
  EnterpriseCert(const EnterpriseCert& other)
      : mDER(other.mDER.Clone()), mIsRoot(other.mIsRoot) {}
  EnterpriseCert(EnterpriseCert&& other)
      : mDER(std::move(other.mDER)), mIsRoot(other.mIsRoot) {}

  void CopyBytes(nsTArray<uint8_t>& dest) const;
  mozilla::pkix::Result GetInput(mozilla::pkix::Input& input) const;
  bool GetIsRoot() const;
  // Is this certificate a known, built-in root?
  bool IsKnownRoot(mozilla::UniqueSECMODModule& rootsModule);

 private:
  nsTArray<uint8_t> mDER;
  bool mIsRoot;
};

// This may block and must not be called from the main thread.
nsresult GatherEnterpriseCerts(nsTArray<EnterpriseCert>& certs);

#endif  // EnterpriseRoots_h
