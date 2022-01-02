/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "gtest_utils.h"
#include "tls_connect.h"

namespace nss_test {

extern "C" {
extern FILE* ssl_trace_iob;

#ifdef NSS_ALLOW_SSLKEYLOGFILE
extern FILE* ssl_keylog_iob;
#endif
}

// These tests ensure that when the associated environment variables are unset
// that the lazily-initialized defaults are what they are supposed to be.

#ifdef DEBUG
TEST_P(TlsConnectGeneric, DebugEnvTraceFileNotSet) {
  char* ev = PR_GetEnvSecure("SSLDEBUGFILE");
  if (ev && ev[0]) {
    GTEST_SKIP();
  }

  Connect();
  EXPECT_EQ(stderr, ssl_trace_iob);
}
#endif

#ifdef NSS_ALLOW_SSLKEYLOGFILE
TEST_P(TlsConnectGeneric, DebugEnvKeylogFileNotSet) {
  char* ev = PR_GetEnvSecure("SSLKEYLOGFILE");
  if (ev && ev[0]) {
    GTEST_SKIP();
  }

  Connect();
  EXPECT_EQ(nullptr, ssl_keylog_iob);
}
#endif

}  // namespace nss_test
