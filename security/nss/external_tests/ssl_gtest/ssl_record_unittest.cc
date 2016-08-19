/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "ssl.h"
#include "nss.h"
#include "sslimpl.h"

#include "databuffer.h"
#include "gtest_utils.h"

namespace nss_test {

const static size_t kMacSize = 20;

class TlsPaddingTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<size_t, bool>> {
 public:
  TlsPaddingTest() : plaintext_len_(std::get<0>(GetParam())) {
    size_t extra =
        (plaintext_len_ + 1) % 16;  // Bytes past a block (1 == pad len)
    // Minimal padding.
    pad_len_ = extra ? 16 - extra : 0;
    if (std::get<1>(GetParam())) {
      // Maximal padding.
      pad_len_ += 240;
    }
    MakePaddedPlaintext();
  }

  // Makes a plaintext record with correct padding.
  void MakePaddedPlaintext() {
    EXPECT_EQ(0UL, (plaintext_len_ + pad_len_ + 1) % 16);
    size_t i = 0;
    plaintext_.Allocate(plaintext_len_ + pad_len_ + 1);
    for (; i < plaintext_len_; ++i) {
      plaintext_.Write(i, 'A', 1);
    }

    for (; i < plaintext_len_ + pad_len_ + 1; ++i) {
      plaintext_.Write(i, pad_len_, 1);
    }
  }

  void Unpad(bool expect_success) {
    std::cerr << "Content length=" << plaintext_len_
              << " padding length=" << pad_len_
              << " total length=" << plaintext_.len() << std::endl;
    std::cerr << "Plaintext: " << plaintext_ << std::endl;
    sslBuffer s;
    s.buf = const_cast<unsigned char *>(
        static_cast<const unsigned char *>(plaintext_.data()));
    s.len = plaintext_.len();
    SECStatus rv = ssl_RemoveTLSCBCPadding(&s, kMacSize);
    if (expect_success) {
      EXPECT_EQ(SECSuccess, rv);
      EXPECT_EQ(plaintext_len_, static_cast<size_t>(s.len));
    } else {
      EXPECT_EQ(SECFailure, rv);
    }
  }

 protected:
  size_t plaintext_len_;
  size_t pad_len_;
  DataBuffer plaintext_;
};

TEST_P(TlsPaddingTest, Correct) {
  if (plaintext_len_ >= kMacSize) {
    Unpad(true);
  } else {
    Unpad(false);
  }
}

TEST_P(TlsPaddingTest, PadTooLong) {
  if (plaintext_.len() < 255) {
    plaintext_.Write(plaintext_.len() - 1, plaintext_.len(), 1);
    Unpad(false);
  }
}

TEST_P(TlsPaddingTest, FirstByteOfPadWrong) {
  if (pad_len_) {
    plaintext_.Write(plaintext_len_, plaintext_.data()[plaintext_len_] + 1, 1);
    Unpad(false);
  }
}

TEST_P(TlsPaddingTest, LastByteOfPadWrong) {
  if (pad_len_) {
    plaintext_.Write(plaintext_.len() - 2,
                     plaintext_.data()[plaintext_.len() - 1] + 1, 1);
    Unpad(false);
  }
}

const static size_t kContentSizesArr[] = {
    1, kMacSize - 1, kMacSize, 30, 31, 32, 36, 256, 257, 287, 288};

auto kContentSizes = ::testing::ValuesIn(kContentSizesArr);
const static bool kTrueFalseArr[] = {true, false};
auto kTrueFalse = ::testing::ValuesIn(kTrueFalseArr);

INSTANTIATE_TEST_CASE_P(TlsPadding, TlsPaddingTest,
                        ::testing::Combine(kContentSizes, kTrueFalse));
}  // namespace nspr_test
