/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This attempts to test all the possible variations of |operator==|
 * used with |nsCOMPtr|s.
 */

#include "nsCOMPtr.h"

#define NS_ICOMPTREQTESTFOO_IID \
{0x8eb5bbef, 0xd1a3, 0x4659, \
  {0x9c, 0xf6, 0xfd, 0xf3, 0xe4, 0xd2, 0x00, 0x0e}}

class nsICOMPtrEqTestFoo : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICOMPTREQTESTFOO_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICOMPtrEqTestFoo, NS_ICOMPTREQTESTFOO_IID)

int
main()
{
  nsCOMPtr<nsICOMPtrEqTestFoo> s;
  nsICOMPtrEqTestFoo* r = 0;
  const nsCOMPtr<nsICOMPtrEqTestFoo> sc;
  const nsICOMPtrEqTestFoo* rc = 0;
  nsICOMPtrEqTestFoo* const rk = 0;
  const nsICOMPtrEqTestFoo* const rkc = 0;
  nsICOMPtrEqTestFoo* d = s;

  return (!(true &&
        (s == s) &&
        (s == r) &&
        (s == sc) &&
        (s == rc) &&
        (s == rk) &&
        (s == rkc) &&
        (s == d) &&
        (r == s) &&
        (r == sc) &&
        (r == rc) &&
        (r == rk) &&
        (r == rkc) &&
        (r == d) &&
        (sc == s) &&
        (sc == r) &&
        (sc == sc) &&
        (sc == rc) &&
        (sc == rk) &&
        (sc == rkc) &&
        (sc == d) &&
        (rc == s) &&
        (rc == r) &&
        (rc == sc) &&
        (rc == rk) &&
        (rc == rkc) &&
        (rc == d) &&
        (rk == s) &&
        (rk == r) &&
        (rk == sc) &&
        (rk == rc) &&
        (rk == rkc) &&
        (rk == d) &&
        (rkc == s) &&
        (rkc == r) &&
        (rkc == sc) &&
        (rkc == rc) &&
        (rkc == rk) &&
        (rkc == d) &&
        (d == s) &&
        (d == r) &&
        (d == sc) &&
        (d == rc) &&
        (d == rk) &&
        (d == rkc) &&
        true));
}
