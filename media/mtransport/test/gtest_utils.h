/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Utilities to wrap gtest, based on libjingle's gunit

// Some sections of this code are under the following license:

/*
 * libjingle
 * Copyright 2004--2008, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Original author: ekr@rtfm.com
#ifndef gtest_utils_h__
#define gtest_utils_h__

#include <iostream>

#include "nspr.h"
#include "prinrval.h"
#include "prthread.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

// Wait up to timeout seconds for expression to be true
#define WAIT(expression, timeout) \
  do { \
  for (PRIntervalTime start = PR_IntervalNow(); !(expression) &&        \
           ! ((PR_IntervalNow() - start) > PR_MillisecondsToInterval(timeout));) \
    PR_Sleep(10); \
  } while(0)

// Same as GTEST_WAIT, but stores the result in res. Used when
// you also want the result of expression but wish to avoid
// double evaluation.
#define WAIT_(expression, timeout, res)                      \
  do { \
  for (PRIntervalTime start = PR_IntervalNow(); !(res = (expression)) && \
           ! ((PR_IntervalNow() - start) > PR_MillisecondsToInterval(timeout));) \
    PR_Sleep(10); \
  } while(0)

#define ASSERT_TRUE_WAIT(expression, timeout) \
  do { \
  bool res; \
  WAIT_(expression, timeout, res); \
  ASSERT_TRUE(res); \
  } while(0);

#define EXPECT_TRUE_WAIT(expression, timeout) \
  do { \
  bool res; \
  WAIT_(expression, timeout, res); \
  EXPECT_TRUE(res); \
  } while(0);

#endif



