/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nss.h"
#include "secerr.h"
#include "ssl.h"
#include "ssl3prot.h"
#include "sslerr.h"
#include "sslproto.h"

#include "gtest_utils.h"
#include "nss_scoped_ptrs.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

#include <iostream>

namespace nss_test {

std::string GetSSLVersionString(uint16_t v) {
  switch (v) {
    case SSL_LIBRARY_VERSION_3_0:
      return "ssl3";
    case SSL_LIBRARY_VERSION_TLS_1_0:
      return "tls1.0";
    case SSL_LIBRARY_VERSION_TLS_1_1:
      return "tls1.1";
    case SSL_LIBRARY_VERSION_TLS_1_2:
      return "tls1.2";
    case SSL_LIBRARY_VERSION_TLS_1_3:
      return "tls1.3";
    case SSL_LIBRARY_VERSION_NONE:
      return "NONE";
  }
  if (v < SSL_LIBRARY_VERSION_3_0) {
    return "undefined-too-low";
  }
  return "undefined-too-high";
}

inline std::ostream& operator<<(std::ostream& stream,
                                const SSLVersionRange& vr) {
  return stream << GetSSLVersionString(vr.min) << ","
                << GetSSLVersionString(vr.max);
}

class VersionRangeWithLabel {
 public:
  VersionRangeWithLabel(const std::string& txt, const SSLVersionRange& vr)
      : label_(txt), vr_(vr) {}
  VersionRangeWithLabel(const std::string& txt, uint16_t start, uint16_t end)
      : label_(txt) {
    vr_.min = start;
    vr_.max = end;
  }
  VersionRangeWithLabel(const std::string& label) : label_(label) {
    vr_.min = vr_.max = SSL_LIBRARY_VERSION_NONE;
  }

  void WriteStream(std::ostream& stream) const {
    stream << " " << label_ << ": " << vr_;
  }

  uint16_t min() const { return vr_.min; }
  uint16_t max() const { return vr_.max; }
  SSLVersionRange range() const { return vr_; }

 private:
  std::string label_;
  SSLVersionRange vr_;
};

inline std::ostream& operator<<(std::ostream& stream,
                                const VersionRangeWithLabel& vrwl) {
  vrwl.WriteStream(stream);
  return stream;
}

typedef std::tuple<SSLProtocolVariant,  // variant
                   uint16_t,            // policy min
                   uint16_t,            // policy max
                   uint16_t,            // input min
                   uint16_t>            // input max
    PolicyVersionRangeInput;

class TestPolicyVersionRange
    : public TlsConnectTestBase,
      public ::testing::WithParamInterface<PolicyVersionRangeInput> {
 public:
  TestPolicyVersionRange()
      : TlsConnectTestBase(std::get<0>(GetParam()), 0),
        variant_(std::get<0>(GetParam())),
        policy_("policy", std::get<1>(GetParam()), std::get<2>(GetParam())),
        input_("input", std::get<3>(GetParam()), std::get<4>(GetParam())),
        library_("supported-by-library",
                 ((variant_ == ssl_variant_stream)
                      ? SSL_LIBRARY_VERSION_MIN_SUPPORTED_STREAM
                      : SSL_LIBRARY_VERSION_MIN_SUPPORTED_DATAGRAM),
                 SSL_LIBRARY_VERSION_MAX_SUPPORTED) {
    TlsConnectTestBase::SkipVersionChecks();
  }

  void SetPolicy(const SSLVersionRange& policy) {
    NSS_SetAlgorithmPolicy(SEC_OID_APPLY_SSL_POLICY, NSS_USE_POLICY_IN_SSL, 0);

    SECStatus rv;
    rv = NSS_OptionSet(NSS_TLS_VERSION_MIN_POLICY, policy.min);
    ASSERT_EQ(SECSuccess, rv);
    rv = NSS_OptionSet(NSS_TLS_VERSION_MAX_POLICY, policy.max);
    ASSERT_EQ(SECSuccess, rv);
    rv = NSS_OptionSet(NSS_DTLS_VERSION_MIN_POLICY, policy.min);
    ASSERT_EQ(SECSuccess, rv);
    rv = NSS_OptionSet(NSS_DTLS_VERSION_MAX_POLICY, policy.max);
    ASSERT_EQ(SECSuccess, rv);
  }

  void CreateDummySocket(std::shared_ptr<DummyPrSocket>* dummy_socket,
                         ScopedPRFileDesc* ssl_fd) {
    (*dummy_socket).reset(new DummyPrSocket("dummy", variant_));
    *ssl_fd = (*dummy_socket)->CreateFD();
    if (variant_ == ssl_variant_stream) {
      SSL_ImportFD(nullptr, ssl_fd->get());
    } else {
      DTLS_ImportFD(nullptr, ssl_fd->get());
    }
  }

  bool GetOverlap(const SSLVersionRange& r1, const SSLVersionRange& r2,
                  SSLVersionRange* overlap) {
    if (r1.min == SSL_LIBRARY_VERSION_NONE ||
        r1.max == SSL_LIBRARY_VERSION_NONE ||
        r2.min == SSL_LIBRARY_VERSION_NONE ||
        r2.max == SSL_LIBRARY_VERSION_NONE) {
      return false;
    }

    SSLVersionRange temp;
    temp.min = PR_MAX(r1.min, r2.min);
    temp.max = PR_MIN(r1.max, r2.max);

    if (temp.min > temp.max) {
      return false;
    }

    *overlap = temp;
    return true;
  }

  bool IsValidInputForVersionRangeSet(SSLVersionRange* expectedEffectiveRange) {
    if (input_.min() <= SSL_LIBRARY_VERSION_3_0 &&
        input_.max() >= SSL_LIBRARY_VERSION_TLS_1_3) {
      // This is always invalid input, independent of policy
      return false;
    }

    if (input_.min() < library_.min() || input_.max() > library_.max() ||
        input_.min() > input_.max()) {
      // Asking for unsupported ranges is invalid input for VersionRangeSet
      // APIs, regardless of overlap.
      return false;
    }

    SSLVersionRange overlap_with_library;
    if (!GetOverlap(input_.range(), library_.range(), &overlap_with_library)) {
      return false;
    }

    SSLVersionRange overlap_with_library_and_policy;
    if (!GetOverlap(overlap_with_library, policy_.range(),
                    &overlap_with_library_and_policy)) {
      return false;
    }

    RemoveConflictingVersions(variant_, &overlap_with_library_and_policy);
    *expectedEffectiveRange = overlap_with_library_and_policy;
    return true;
  }

  void RemoveConflictingVersions(SSLProtocolVariant variant,
                                 SSLVersionRange* r) {
    ASSERT_TRUE(r != nullptr);
    if (r->max >= SSL_LIBRARY_VERSION_TLS_1_3 &&
        r->min < SSL_LIBRARY_VERSION_TLS_1_0) {
      r->min = SSL_LIBRARY_VERSION_TLS_1_0;
    }
  }

  void SetUp() override {
    TlsConnectTestBase::SetUp();
    SetPolicy(policy_.range());
  }

  void TearDown() override {
    TlsConnectTestBase::TearDown();
    saved_version_policy_.RestoreOriginalPolicy();
  }

 protected:
  class VersionPolicy {
   public:
    VersionPolicy() { SaveOriginalPolicy(); }

    void RestoreOriginalPolicy() {
      SECStatus rv;
      rv = NSS_OptionSet(NSS_TLS_VERSION_MIN_POLICY, saved_min_tls_);
      ASSERT_EQ(SECSuccess, rv);
      rv = NSS_OptionSet(NSS_TLS_VERSION_MAX_POLICY, saved_max_tls_);
      ASSERT_EQ(SECSuccess, rv);
      rv = NSS_OptionSet(NSS_DTLS_VERSION_MIN_POLICY, saved_min_dtls_);
      ASSERT_EQ(SECSuccess, rv);
      rv = NSS_OptionSet(NSS_DTLS_VERSION_MAX_POLICY, saved_max_dtls_);
      ASSERT_EQ(SECSuccess, rv);
    }

   private:
    void SaveOriginalPolicy() {
      SECStatus rv;
      rv = NSS_OptionGet(NSS_TLS_VERSION_MIN_POLICY, &saved_min_tls_);
      ASSERT_EQ(SECSuccess, rv);
      rv = NSS_OptionGet(NSS_TLS_VERSION_MAX_POLICY, &saved_max_tls_);
      ASSERT_EQ(SECSuccess, rv);
      rv = NSS_OptionGet(NSS_DTLS_VERSION_MIN_POLICY, &saved_min_dtls_);
      ASSERT_EQ(SECSuccess, rv);
      rv = NSS_OptionGet(NSS_DTLS_VERSION_MAX_POLICY, &saved_max_dtls_);
      ASSERT_EQ(SECSuccess, rv);
    }

    int32_t saved_min_tls_;
    int32_t saved_max_tls_;
    int32_t saved_min_dtls_;
    int32_t saved_max_dtls_;
  };

  VersionPolicy saved_version_policy_;

  SSLProtocolVariant variant_;
  const VersionRangeWithLabel policy_;
  const VersionRangeWithLabel input_;
  const VersionRangeWithLabel library_;
};

static const uint16_t kExpandedVersionsArr[] = {
    /* clang-format off */
    SSL_LIBRARY_VERSION_3_0 - 1,
    SSL_LIBRARY_VERSION_3_0,
    SSL_LIBRARY_VERSION_TLS_1_0,
    SSL_LIBRARY_VERSION_TLS_1_1,
    SSL_LIBRARY_VERSION_TLS_1_2,
#ifndef NSS_DISABLE_TLS_1_3
    SSL_LIBRARY_VERSION_TLS_1_3,
#endif
    SSL_LIBRARY_VERSION_MAX_SUPPORTED + 1
    /* clang-format on */
};
static ::testing::internal::ParamGenerator<uint16_t> kExpandedVersions =
    ::testing::ValuesIn(kExpandedVersionsArr);

TEST_P(TestPolicyVersionRange, TestAllTLSVersionsAndPolicyCombinations) {
  ASSERT_TRUE(variant_ == ssl_variant_stream ||
              variant_ == ssl_variant_datagram)
      << "testing unsupported ssl variant";

  std::cerr << "testing: " << variant_ << policy_ << input_ << library_
            << std::endl;

  SSLVersionRange supported_range;
  SECStatus rv = SSL_VersionRangeGetSupported(variant_, &supported_range);
  VersionRangeWithLabel supported("SSL_VersionRangeGetSupported",
                                  supported_range);

  std::cerr << supported << std::endl;

  std::shared_ptr<DummyPrSocket> dummy_socket;
  ScopedPRFileDesc ssl_fd;
  CreateDummySocket(&dummy_socket, &ssl_fd);

  SECStatus rv_socket;
  SSLVersionRange overlap_policy_and_lib;
  if (!GetOverlap(policy_.range(), library_.range(), &overlap_policy_and_lib)) {
    EXPECT_EQ(SECFailure, rv)
        << "expected SSL_VersionRangeGetSupported to fail with invalid policy";

    SSLVersionRange enabled_range;
    rv = SSL_VersionRangeGetDefault(variant_, &enabled_range);
    EXPECT_EQ(SECFailure, rv)
        << "expected SSL_VersionRangeGetDefault to fail with invalid policy";

    SSLVersionRange enabled_range_on_socket;
    rv_socket = SSL_VersionRangeGet(ssl_fd.get(), &enabled_range_on_socket);
    EXPECT_EQ(SECFailure, rv_socket)
        << "expected SSL_VersionRangeGet to fail with invalid policy";

    ConnectExpectFail();
    return;
  }

  EXPECT_EQ(SECSuccess, rv)
      << "expected SSL_VersionRangeGetSupported to succeed with valid policy";

  EXPECT_TRUE(supported_range.min != SSL_LIBRARY_VERSION_NONE &&
              supported_range.max != SSL_LIBRARY_VERSION_NONE)
      << "expected SSL_VersionRangeGetSupported to return real values with "
         "valid policy";

  RemoveConflictingVersions(variant_, &overlap_policy_and_lib);
  VersionRangeWithLabel overlap_info("overlap", overlap_policy_and_lib);

  EXPECT_TRUE(supported_range == overlap_policy_and_lib)
      << "expected range from GetSupported to be identical with calculated "
         "overlap "
      << overlap_info;

  // We don't know which versions are "enabled by default" by the library,
  // therefore we don't know if there's overlap between the default
  // and the policy, and therefore, we don't if TLS connections should
  // be successful or fail in this combination.
  // Therefore we don't test if we can connect, without having configured a
  // version range explicitly.

  // Now start testing with supplied input.

  SSLVersionRange expected_effective_range;
  bool is_valid_input =
      IsValidInputForVersionRangeSet(&expected_effective_range);

  SSLVersionRange temp_input = input_.range();
  rv = SSL_VersionRangeSetDefault(variant_, &temp_input);
  rv_socket = SSL_VersionRangeSet(ssl_fd.get(), &temp_input);

  if (!is_valid_input) {
    EXPECT_EQ(SECFailure, rv)
        << "expected failure return from SSL_VersionRangeSetDefault";

    EXPECT_EQ(SECFailure, rv_socket)
        << "expected failure return from SSL_VersionRangeSet";
    return;
  }

  EXPECT_EQ(SECSuccess, rv)
      << "expected successful return from SSL_VersionRangeSetDefault";

  EXPECT_EQ(SECSuccess, rv_socket)
      << "expected successful return from SSL_VersionRangeSet";

  SSLVersionRange effective;
  SSLVersionRange effective_socket;

  rv = SSL_VersionRangeGetDefault(variant_, &effective);
  EXPECT_EQ(SECSuccess, rv)
      << "expected successful return from SSL_VersionRangeGetDefault";

  rv_socket = SSL_VersionRangeGet(ssl_fd.get(), &effective_socket);
  EXPECT_EQ(SECSuccess, rv_socket)
      << "expected successful return from SSL_VersionRangeGet";

  VersionRangeWithLabel expected_info("expectation", expected_effective_range);
  VersionRangeWithLabel effective_info("effectively-enabled", effective);

  EXPECT_TRUE(expected_effective_range == effective)
      << "range returned by SSL_VersionRangeGetDefault doesn't match "
         "expectation: "
      << expected_info << effective_info;

  EXPECT_TRUE(expected_effective_range == effective_socket)
      << "range returned by SSL_VersionRangeGet doesn't match "
         "expectation: "
      << expected_info << effective_info;

  // Because we found overlap between policy and supported versions,
  // and because we have used SetDefault to enable at least one version,
  // it should be possible to execute an SSL/TLS connection.
  Connect();
}

INSTANTIATE_TEST_CASE_P(TLSVersionRanges, TestPolicyVersionRange,
                        ::testing::Combine(TlsConnectTestBase::kTlsVariantsAll,
                                           kExpandedVersions, kExpandedVersions,
                                           kExpandedVersions,
                                           kExpandedVersions));
}  // namespace nss_test
