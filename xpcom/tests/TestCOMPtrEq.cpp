/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TestCOMPtrEq.cpp.
 *
 * The Initial Developer of the Original Code is
 * L. David Baron.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

 /**
  * This attempts to test all the possible variations of |operator==|
  * used with |nsCOMPtr|s.  Currently only the tests where pointers
  * are to the same class are enabled.  It's not clear whether we
  * should be supporting other tests, and some of them won't work
  * on at least some platforms.  If we add separate comparisons
  * for nsCOMPtr<nsISupports> we'll need to add more tests for
  * those cases.
  */

#include "nsCOMPtr.h"

  // Don't test these now, since some of them won't work and it's
  // not clear whether they should (see above).
#undef NSCAP_EQTEST_TEST_ACROSS_TYPES

#define NS_ICOMPTREQTESTFOO_IID \
  {0x8eb5bbef, 0xd1a3, 0x4659, \
    {0x9c, 0xf6, 0xfd, 0xf3, 0xe4, 0xd2, 0x00, 0x0e}}

class nsICOMPtrEqTestFoo : public nsISupports {
  public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICOMPTREQTESTFOO_IID)
};

#ifdef NSCAP_EQTEST_TEST_ACROSS_TYPES

#define NS_ICOMPTREQTESTFOO2_IID \
  {0x6516387b, 0x36c5, 0x4036, \
    {0x82, 0xc9, 0xa7, 0x4d, 0xd9, 0xe5, 0x92, 0x2f}}

class nsICOMPtrEqTestFoo2 : public nsISupports {
  public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICOMPTREQTESTFOO2_IID)
};

#endif

int
main()
  {
    nsCOMPtr<nsICOMPtrEqTestFoo> s;
    nsICOMPtrEqTestFoo* r = 0;
    const nsCOMPtr<nsICOMPtrEqTestFoo> sc;
    const nsICOMPtrEqTestFoo* rc = 0;
    nsICOMPtrEqTestFoo* const rk = 0;
    const nsICOMPtrEqTestFoo* const rkc = 0;
    nsDerivedSafe<nsICOMPtrEqTestFoo>* d = s.get();
    
#ifdef NSCAP_EQTEST_TEST_ACROSS_TYPES
    nsCOMPtr<nsICOMPtrEqTestFoo2> s2;
    nsICOMPtrEqTestFoo2* r2 = 0;
    const nsCOMPtr<nsICOMPtrEqTestFoo2> sc2;
    const nsICOMPtrEqTestFoo2* rc2 = 0;
    nsICOMPtrEqTestFoo2* const rk2 = 0;
    const nsICOMPtrEqTestFoo2* const rkc2 = 0;
    nsDerivedSafe<nsICOMPtrEqTestFoo2>* d2 = s2.get();
#endif

    return (!(PR_TRUE &&
              (s == s) &&
              (s == r) &&
              (s == sc) &&
              (s == rc) &&
              (s == rk) &&
              (s == rkc) &&
              (s == d) &&
              (r == s) &&
              (r == r) &&
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
              (rc == rc) &&
              (rc == rk) &&
              (rc == rkc) &&
              (rc == d) &&
              (rk == s) &&
              (rk == r) &&
              (rk == sc) &&
              (rk == rc) &&
              (rk == rk) &&
              (rk == rkc) &&
              (rk == d) &&
              (rkc == s) &&
              (rkc == r) &&
              (rkc == sc) &&
              (rkc == rc) &&
              (rkc == rk) &&
              (rkc == rkc) &&
              (rkc == d) &&
              (d == s) &&
              (d == r) &&
              (d == sc) &&
              (d == rc) &&
              (d == rk) &&
              (d == rkc) &&
              (d == d) &&
#ifdef NSCAP_EQTEST_TEST_ACROSS_TYPES
              (s == s2) &&
              (s == r2) &&
              (s == sc2) &&
              (s == rc2) &&
              (s == rk2) &&
              (s == rkc2) &&
              (s == d2) &&
              (r == s2) &&
              (r == r2) &&
              (r == sc2) &&
              (r == rc2) &&
              (r == rk2) &&
              (r == rkc2) &&
              (r == d2) &&
              (sc == s2) &&
              (sc == r2) &&
              (sc == sc2) &&
              (sc == rc2) &&
              (sc == rk2) &&
              (sc == rkc2) &&
              (sc == d2) &&
              (rc == s2) &&
              (rc == r2) &&
              (rc == sc2) &&
              (rc == rc2) &&
              (rc == rk2) &&
              (rc == rkc2) &&
              (rc == d2) &&
              (rk == s2) &&
              (rk == r2) &&
              (rk == sc2) &&
              (rk == rc2) &&
              (rk == rk2) &&
              (rk == rkc2) &&
              (rk == d2) &&
              (rkc == s2) &&
              (rkc == r2) &&
              (rkc == sc2) &&
              (rkc == rc2) &&
              (rkc == rk2) &&
              (rkc == rkc2) &&
              (rkc == d2) &&
              (d == s2) &&
              (d == r2) &&
              (d == sc2) &&
              (d == rc2) &&
              (d == rk2) &&
              (d == rkc2) &&
              (d == d2) &&
#endif
              PR_TRUE));
  }
