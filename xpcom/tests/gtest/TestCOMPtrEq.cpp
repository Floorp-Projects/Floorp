/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This attempts to test all the possible variations of |operator==|
 * used with |nsCOMPtr|s.
 */

#include "nsCOMPtr.h"
#include "gtest/gtest.h"

#define NS_ICOMPTREQTESTFOO_IID                      \
  {                                                  \
    0x8eb5bbef, 0xd1a3, 0x4659, {                    \
      0x9c, 0xf6, 0xfd, 0xf3, 0xe4, 0xd2, 0x00, 0x0e \
    }                                                \
  }

class nsICOMPtrEqTestFoo : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICOMPTREQTESTFOO_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICOMPtrEqTestFoo, NS_ICOMPTREQTESTFOO_IID)

TEST(COMPtrEq, NullEquality)
{
  nsCOMPtr<nsICOMPtrEqTestFoo> s;
  nsICOMPtrEqTestFoo* r = nullptr;
  const nsCOMPtr<nsICOMPtrEqTestFoo> sc;
  const nsICOMPtrEqTestFoo* rc = nullptr;
  nsICOMPtrEqTestFoo* const rk = nullptr;
  const nsICOMPtrEqTestFoo* const rkc = nullptr;
  nsICOMPtrEqTestFoo* d = s;

  ASSERT_EQ(s, s);
  ASSERT_EQ(s, r);
  ASSERT_EQ(s, sc);
  ASSERT_EQ(s, rc);
  ASSERT_EQ(s, rk);
  ASSERT_EQ(s, rkc);
  ASSERT_EQ(s, d);
  ASSERT_EQ(r, s);
  ASSERT_EQ(r, sc);
  ASSERT_EQ(r, rc);
  ASSERT_EQ(r, rk);
  ASSERT_EQ(r, rkc);
  ASSERT_EQ(r, d);
  ASSERT_EQ(sc, s);
  ASSERT_EQ(sc, r);
  ASSERT_EQ(sc, sc);
  ASSERT_EQ(sc, rc);
  ASSERT_EQ(sc, rk);
  ASSERT_EQ(sc, rkc);
  ASSERT_EQ(sc, d);
  ASSERT_EQ(rc, s);
  ASSERT_EQ(rc, r);
  ASSERT_EQ(rc, sc);
  ASSERT_EQ(rc, rk);
  ASSERT_EQ(rc, rkc);
  ASSERT_EQ(rc, d);
  ASSERT_EQ(rk, s);
  ASSERT_EQ(rk, r);
  ASSERT_EQ(rk, sc);
  ASSERT_EQ(rk, rc);
  ASSERT_EQ(rk, rkc);
  ASSERT_EQ(rk, d);
  ASSERT_EQ(rkc, s);
  ASSERT_EQ(rkc, r);
  ASSERT_EQ(rkc, sc);
  ASSERT_EQ(rkc, rc);
  ASSERT_EQ(rkc, rk);
  ASSERT_EQ(rkc, d);
  ASSERT_EQ(d, s);
  ASSERT_EQ(d, r);
  ASSERT_EQ(d, sc);
  ASSERT_EQ(d, rc);
  ASSERT_EQ(d, rk);
  ASSERT_EQ(d, rkc);
}
