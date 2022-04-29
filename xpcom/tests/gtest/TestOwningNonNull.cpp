/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/OwningNonNull.h"
#include "mozilla/RefCounted.h"
#include "mozilla/RefPtr.h"
#include "gtest/gtest.h"

using namespace mozilla;

struct OwnedRefCounted : public RefCounted<OwnedRefCounted> {
  MOZ_DECLARE_REFCOUNTED_TYPENAME(OwnedRefCounted)

  OwnedRefCounted() = default;
};

TEST(OwningNonNull, Move)
{
  auto refptr = MakeRefPtr<OwnedRefCounted>();
  OwningNonNull<OwnedRefCounted> owning(std::move(refptr));
  EXPECT_FALSE(!!refptr);
}
