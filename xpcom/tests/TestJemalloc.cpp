/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
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
 * The Original Code is c++ array template tests.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Nicholas Nethercote <nnethercote@mozilla.com>
 *  
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/*
 * Ideally, this test would be in memory/test/.  But I couldn't get it to build
 * there (couldn't find TestHarness.h).  I think memory/ is processed too early
 * in the build.  So it's here.
 */

#include "TestHarness.h"
#include "jemalloc.h"

static inline bool
TestOne(size_t size)
{
    size_t req = size;
    size_t adv = je_malloc_usable_size_in_advance(req);
    char* p = (char*)malloc(req);
    size_t usable = moz_malloc_usable_size(p);
    if (adv != usable) {
      fail("je_malloc_usable_size_in_advance(%d) --> %d; "
           "malloc_usable_size(%d) --> %d",
           req, adv, req, usable);
      return false;
    }
    free(p);
    return true;
}

static inline bool
TestThree(size_t size)
{
    return TestOne(size - 1) && TestOne(size) && TestOne(size + 1);
}

static nsresult
TestJemallocUsableSizeInAdvance()
{
  #define K   * 1024
  #define M   * 1024 * 1024

  /*
   * Test every size up to a certain point, then (N-1, N, N+1) triplets for a
   * various sizes beyond that.
   */

  for (size_t n = 0; n < 16 K; n++)
    if (!TestOne(n))
      return NS_ERROR_UNEXPECTED;

  for (size_t n = 16 K; n < 1 M; n += 4 K)
    if (!TestThree(n))
      return NS_ERROR_UNEXPECTED;

  for (size_t n = 1 M; n < 8 M; n += 128 K)
    if (!TestThree(n))
      return NS_ERROR_UNEXPECTED;

  passed("je_malloc_usable_size_in_advance");

  return NS_OK;
}

int main(int argc, char** argv)
{
  int rv = 0;
  ScopedXPCOM xpcom("jemalloc");
  if (xpcom.failed())
      return 1;

  if (NS_FAILED(TestJemallocUsableSizeInAdvance()))
    rv = 1;

  return rv;
}

