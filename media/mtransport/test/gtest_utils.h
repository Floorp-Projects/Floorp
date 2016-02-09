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
#ifndef gtest_utils__h__
#define gtest_utils__h__

#include <iostream>

#include "nspr.h"
#include "prinrval.h"
#include "prthread.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

#include "gtest_ringbuffer_dumper.h"
#include "mtransport_test_utils.h"
#include "nss.h"
#include "ssl.h"

extern "C" {
#include "registry.h"
#include "transport_addr.h"
}

// Wait up to timeout seconds for expression to be true
#define WAIT(expression, timeout) \
  do { \
    for (PRIntervalTime start = PR_IntervalNow(); !(expression) && \
           ! ((PR_IntervalNow() - start) > PR_MillisecondsToInterval(timeout));) { \
      PR_Sleep(10); \
    } \
  } while(0)

// Same as GTEST_WAIT, but stores the result in res. Used when
// you also want the result of expression but wish to avoid
// double evaluation.
#define WAIT_(expression, timeout, res) \
  do { \
    for (PRIntervalTime start = PR_IntervalNow(); !(res = (expression)) && \
           ! ((PR_IntervalNow() - start) > PR_MillisecondsToInterval(timeout));) { \
      PR_Sleep(10); \
    } \
  } while(0)

#define ASSERT_TRUE_WAIT(expression, timeout) \
  do { \
    bool res; \
    WAIT_(expression, timeout, res); \
    ASSERT_TRUE(res); \
  } while(0)

#define EXPECT_TRUE_WAIT(expression, timeout) \
  do { \
    bool res; \
    WAIT_(expression, timeout, res); \
    EXPECT_TRUE(res); \
  } while(0)

#define ASSERT_EQ_WAIT(expected, actual, timeout) \
  do { \
    WAIT(expected == actual, timeout); \
    ASSERT_EQ(expected, actual); \
  } while(0)

using test::RingbufferDumper;

class MtransportTest : public ::testing::Test {
public:
  MtransportTest()
    : test_utils_(nullptr)
    , dumper_(nullptr)
  {
  }

  void SetUp() override {
    test_utils_ = new MtransportTestUtils();
    NSS_NoDB_Init(nullptr);
    NSS_SetDomesticPolicy();

    NR_reg_init(NR_REG_MODE_LOCAL);

    // Attempt to load env vars used by tests.
    GetEnvironment("TURN_SERVER_ADDRESS", turn_server_);
    GetEnvironment("TURN_SERVER_USER", turn_user_);
    GetEnvironment("TURN_SERVER_PASSWORD", turn_password_);
    GetEnvironment("STUN_SERVER_ADDRESS", stun_server_address_);
    GetEnvironment("STUN_SERVER_HOSTNAME", stun_server_hostname_);

    std::string disable_non_local;
    GetEnvironment("MOZ_DISABLE_NONLOCAL_CONNECTIONS", disable_non_local);
    std::string upload_dir;
    GetEnvironment("MOZ_UPLOAD_DIR", upload_dir);

    if ((!disable_non_local.empty() && disable_non_local != "0") ||
        !upload_dir.empty()) {
      // We're assuming that MOZ_UPLOAD_DIR is only set on tbpl;
      // MOZ_DISABLE_NONLOCAL_CONNECTIONS probably should be set when running the
      // cpp unit-tests, but is not presently.
      stun_server_address_ = "";
      stun_server_hostname_ = "";
      turn_server_ = "";
    }

    // Some tests are flaky and need to check if they're supposed to run.
    webrtc_enabled_ = CheckEnvironmentFlag("MOZ_WEBRTC_TESTS");

    ::testing::TestEventListeners& listeners =
        ::testing::UnitTest::GetInstance()->listeners();

    dumper_ = new RingbufferDumper(test_utils_);
    listeners.Append(dumper_);
  }

  void TearDown() override {
    ::testing::UnitTest::GetInstance()->listeners().Release(dumper_);
    delete dumper_;
    delete test_utils_;
  }

  void GetEnvironment(const char* aVar, std::string& out) {
    char* value = getenv(aVar);
    if (value) {
      out = value;
    }
  }

  bool CheckEnvironmentFlag(const char* aVar) {
    std::string value;
    GetEnvironment(aVar, value);
    return value == "1";
  }

  bool WarnIfTurnNotConfigured() const {
    bool configured =
        !turn_server_.empty() &&
        !turn_user_.empty() &&
        !turn_password_.empty();

    if (configured) {
      nr_transport_addr addr;
      if (nr_str_port_to_transport_addr(turn_server_.c_str(), 3478,
                                        IPPROTO_UDP, &addr)) {
        printf("Invalid TURN_SERVER_ADDRESS \"%s\". Only IP numbers supported.\n",
               turn_server_.c_str());
        configured = false;
      }
    } else {
      printf(
        "Set TURN_SERVER_ADDRESS, TURN_SERVER_USER, and TURN_SERVER_PASSWORD\n"
        "environment variables to run this test\n");
    }

    return !configured;
  }

  MtransportTestUtils* test_utils_;
  RingbufferDumper* dumper_;

  std::string turn_server_;
  std::string turn_user_;
  std::string turn_password_;
  std::string stun_server_address_;
  std::string stun_server_hostname_;

  bool webrtc_enabled_;
};
#endif
