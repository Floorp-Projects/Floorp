/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef EnterpriseRoots_h
#define EnterpriseRoots_h

#include "mozilla/Vector.h"
#include "mozpkix/Input.h"
#include "mozpkix/Result.h"
#include "nsTArray.h"

class EnterpriseCert {
 public:
  EnterpriseCert() : mIsRoot(false) {}

  nsresult Init(const uint8_t* data, size_t len, bool isRoot);
  // Like a copy constructor but able to return a result.
  nsresult Init(const EnterpriseCert& orig);

  nsresult CopyBytes(nsTArray<uint8_t>& dest) const;
  mozilla::pkix::Result GetInput(mozilla::pkix::Input& input) const;
  bool GetIsRoot() const;

 private:
  mozilla::Vector<uint8_t> mDER;
  bool mIsRoot;
};

// This may block and must not be called from the main thread.
nsresult GatherEnterpriseCerts(mozilla::Vector<EnterpriseCert>& certs);

#endif  // EnterpriseRoots_h
