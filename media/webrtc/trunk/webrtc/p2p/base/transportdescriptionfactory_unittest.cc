/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>
#include <vector>

#include "webrtc/p2p/base/constants.h"
#include "webrtc/p2p/base/transportdescription.h"
#include "webrtc/p2p/base/transportdescriptionfactory.h"
#include "webrtc/base/fakesslidentity.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/ssladapter.h"

using rtc::scoped_ptr;
using cricket::TransportDescriptionFactory;
using cricket::TransportDescription;
using cricket::TransportOptions;

class TransportDescriptionFactoryTest : public testing::Test {
 public:
  TransportDescriptionFactoryTest()
      : id1_(new rtc::FakeSSLIdentity("User1")),
        id2_(new rtc::FakeSSLIdentity("User2")) {
  }

  void CheckDesc(const TransportDescription* desc, const std::string& type,
                 const std::string& opt, const std::string& ice_ufrag,
                 const std::string& ice_pwd, const std::string& dtls_alg) {
    ASSERT_TRUE(desc != NULL);
    EXPECT_EQ(type, desc->transport_type);
    EXPECT_EQ(!opt.empty(), desc->HasOption(opt));
    if (ice_ufrag.empty() && ice_pwd.empty()) {
      EXPECT_EQ(static_cast<size_t>(cricket::ICE_UFRAG_LENGTH),
                desc->ice_ufrag.size());
      EXPECT_EQ(static_cast<size_t>(cricket::ICE_PWD_LENGTH),
                desc->ice_pwd.size());
    } else {
      EXPECT_EQ(ice_ufrag, desc->ice_ufrag);
      EXPECT_EQ(ice_pwd, desc->ice_pwd);
    }
    if (dtls_alg.empty()) {
      EXPECT_TRUE(desc->identity_fingerprint.get() == NULL);
    } else {
      ASSERT_TRUE(desc->identity_fingerprint.get() != NULL);
      EXPECT_EQ(desc->identity_fingerprint->algorithm, dtls_alg);
      EXPECT_GT(desc->identity_fingerprint->digest.size(), 0U);
    }
  }

  // This test ice restart by doing two offer answer exchanges. On the second
  // exchange ice is restarted. The test verifies that the ufrag and password
  // in the offer and answer is changed.
  // If |dtls| is true, the test verifies that the finger print is not changed.
  void TestIceRestart(bool dtls) {
    if (dtls) {
      f1_.set_secure(cricket::SEC_ENABLED);
      f2_.set_secure(cricket::SEC_ENABLED);
      f1_.set_identity(id1_.get());
      f2_.set_identity(id2_.get());
    } else {
      f1_.set_secure(cricket::SEC_DISABLED);
      f2_.set_secure(cricket::SEC_DISABLED);
    }

    cricket::TransportOptions options;
    // The initial offer / answer exchange.
    rtc::scoped_ptr<TransportDescription> offer(f1_.CreateOffer(
        options, NULL));
    rtc::scoped_ptr<TransportDescription> answer(
        f2_.CreateAnswer(offer.get(),
                         options, NULL));

    // Create an updated offer where we restart ice.
    options.ice_restart = true;
    rtc::scoped_ptr<TransportDescription> restart_offer(f1_.CreateOffer(
        options, offer.get()));

    VerifyUfragAndPasswordChanged(dtls, offer.get(), restart_offer.get());

    // Create a new answer. The transport ufrag and password is changed since
    // |options.ice_restart == true|
    rtc::scoped_ptr<TransportDescription> restart_answer(
        f2_.CreateAnswer(restart_offer.get(), options, answer.get()));
    ASSERT_TRUE(restart_answer.get() != NULL);

    VerifyUfragAndPasswordChanged(dtls, answer.get(), restart_answer.get());
  }

  void VerifyUfragAndPasswordChanged(bool dtls,
                                     const TransportDescription* org_desc,
                                     const TransportDescription* restart_desc) {
    EXPECT_NE(org_desc->ice_pwd, restart_desc->ice_pwd);
    EXPECT_NE(org_desc->ice_ufrag, restart_desc->ice_ufrag);
    EXPECT_EQ(static_cast<size_t>(cricket::ICE_UFRAG_LENGTH),
              restart_desc->ice_ufrag.size());
    EXPECT_EQ(static_cast<size_t>(cricket::ICE_PWD_LENGTH),
              restart_desc->ice_pwd.size());
    // If DTLS is enabled, make sure the finger print is unchanged.
    if (dtls) {
      EXPECT_FALSE(
          org_desc->identity_fingerprint->GetRfc4572Fingerprint().empty());
      EXPECT_EQ(org_desc->identity_fingerprint->GetRfc4572Fingerprint(),
                restart_desc->identity_fingerprint->GetRfc4572Fingerprint());
    }
  }

 protected:
  TransportDescriptionFactory f1_;
  TransportDescriptionFactory f2_;
  scoped_ptr<rtc::SSLIdentity> id1_;
  scoped_ptr<rtc::SSLIdentity> id2_;
};

// Test that in the default case, we generate the expected G-ICE offer.
TEST_F(TransportDescriptionFactoryTest, TestOfferGice) {
  f1_.set_protocol(cricket::ICEPROTO_GOOGLE);
  scoped_ptr<TransportDescription> desc(f1_.CreateOffer(
      TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_GINGLE_P2P, "", "", "", "");
}

// Test generating a hybrid offer.
TEST_F(TransportDescriptionFactoryTest, TestOfferHybrid) {
  f1_.set_protocol(cricket::ICEPROTO_HYBRID);
  scoped_ptr<TransportDescription> desc(f1_.CreateOffer(
      TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "google-ice", "", "", "");
}

// Test generating an ICE-only offer.
TEST_F(TransportDescriptionFactoryTest, TestOfferIce) {
  f1_.set_protocol(cricket::ICEPROTO_RFC5245);
  scoped_ptr<TransportDescription> desc(f1_.CreateOffer(
      TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "", "", "", "");
}

// Test generating a hybrid offer with DTLS.
TEST_F(TransportDescriptionFactoryTest, TestOfferHybridDtls) {
  f1_.set_protocol(cricket::ICEPROTO_HYBRID);
  f1_.set_secure(cricket::SEC_ENABLED);
  f1_.set_identity(id1_.get());
  std::string digest_alg;
  ASSERT_TRUE(id1_->certificate().GetSignatureDigestAlgorithm(&digest_alg));
  scoped_ptr<TransportDescription> desc(f1_.CreateOffer(
      TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "google-ice", "", "",
            digest_alg);
  // Ensure it also works with SEC_REQUIRED.
  f1_.set_secure(cricket::SEC_REQUIRED);
  desc.reset(f1_.CreateOffer(TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "google-ice", "", "",
            digest_alg);
}

// Test generating a hybrid offer with DTLS fails with no identity.
TEST_F(TransportDescriptionFactoryTest, TestOfferHybridDtlsWithNoIdentity) {
  f1_.set_protocol(cricket::ICEPROTO_HYBRID);
  f1_.set_secure(cricket::SEC_ENABLED);
  scoped_ptr<TransportDescription> desc(f1_.CreateOffer(
      TransportOptions(), NULL));
  ASSERT_TRUE(desc.get() == NULL);
}

// Test updating a hybrid offer with DTLS to pick ICE.
// The ICE credentials should stay the same in the new offer.
TEST_F(TransportDescriptionFactoryTest, TestOfferHybridDtlsReofferIceDtls) {
  f1_.set_protocol(cricket::ICEPROTO_HYBRID);
  f1_.set_secure(cricket::SEC_ENABLED);
  f1_.set_identity(id1_.get());
  std::string digest_alg;
  ASSERT_TRUE(id1_->certificate().GetSignatureDigestAlgorithm(&digest_alg));
  scoped_ptr<TransportDescription> old_desc(f1_.CreateOffer(
      TransportOptions(), NULL));
  ASSERT_TRUE(old_desc.get() != NULL);
  f1_.set_protocol(cricket::ICEPROTO_RFC5245);
  scoped_ptr<TransportDescription> desc(
      f1_.CreateOffer(TransportOptions(), old_desc.get()));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "",
            old_desc->ice_ufrag, old_desc->ice_pwd, digest_alg);
}

// Test that we can answer a GICE offer with GICE.
TEST_F(TransportDescriptionFactoryTest, TestAnswerGiceToGice) {
  f1_.set_protocol(cricket::ICEPROTO_GOOGLE);
  f2_.set_protocol(cricket::ICEPROTO_GOOGLE);
  scoped_ptr<TransportDescription> offer(f1_.CreateOffer(
      TransportOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  scoped_ptr<TransportDescription> desc(f2_.CreateAnswer(
      offer.get(), TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_GINGLE_P2P, "", "", "", "");
  // Should get the same result when answering as hybrid.
  f2_.set_protocol(cricket::ICEPROTO_HYBRID);
  desc.reset(f2_.CreateAnswer(offer.get(), TransportOptions(),
                              NULL));
  CheckDesc(desc.get(), cricket::NS_GINGLE_P2P, "", "", "", "");
}

// Test that we can answer a hybrid offer with GICE.
TEST_F(TransportDescriptionFactoryTest, TestAnswerGiceToHybrid) {
  f1_.set_protocol(cricket::ICEPROTO_HYBRID);
  f2_.set_protocol(cricket::ICEPROTO_GOOGLE);
  scoped_ptr<TransportDescription> offer(f1_.CreateOffer(
      TransportOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  scoped_ptr<TransportDescription> desc(
      f2_.CreateAnswer(offer.get(), TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_GINGLE_P2P, "", "", "", "");
}

// Test that we can answer a hybrid offer with ICE.
TEST_F(TransportDescriptionFactoryTest, TestAnswerIceToHybrid) {
  f1_.set_protocol(cricket::ICEPROTO_HYBRID);
  f2_.set_protocol(cricket::ICEPROTO_RFC5245);
  scoped_ptr<TransportDescription> offer(f1_.CreateOffer(
      TransportOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  scoped_ptr<TransportDescription> desc(
      f2_.CreateAnswer(offer.get(), TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "", "", "", "");
  // Should get the same result when answering as hybrid.
  f2_.set_protocol(cricket::ICEPROTO_HYBRID);
  desc.reset(f2_.CreateAnswer(offer.get(), TransportOptions(),
                              NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "", "", "", "");
}

// Test that we can answer an ICE offer with ICE.
TEST_F(TransportDescriptionFactoryTest, TestAnswerIceToIce) {
  f1_.set_protocol(cricket::ICEPROTO_RFC5245);
  f2_.set_protocol(cricket::ICEPROTO_RFC5245);
  scoped_ptr<TransportDescription> offer(f1_.CreateOffer(
      TransportOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  scoped_ptr<TransportDescription> desc(f2_.CreateAnswer(
      offer.get(), TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "", "", "", "");
  // Should get the same result when answering as hybrid.
  f2_.set_protocol(cricket::ICEPROTO_HYBRID);
  desc.reset(f2_.CreateAnswer(offer.get(), TransportOptions(),
                              NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "", "", "", "");
}

// Test that we can't answer a GICE offer with ICE.
TEST_F(TransportDescriptionFactoryTest, TestAnswerIceToGice) {
  f1_.set_protocol(cricket::ICEPROTO_GOOGLE);
  f2_.set_protocol(cricket::ICEPROTO_RFC5245);
  scoped_ptr<TransportDescription> offer(
      f1_.CreateOffer(TransportOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  scoped_ptr<TransportDescription> desc(
      f2_.CreateAnswer(offer.get(), TransportOptions(), NULL));
  ASSERT_TRUE(desc.get() == NULL);
}

// Test that we can't answer an ICE offer with GICE.
TEST_F(TransportDescriptionFactoryTest, TestAnswerGiceToIce) {
  f1_.set_protocol(cricket::ICEPROTO_RFC5245);
  f2_.set_protocol(cricket::ICEPROTO_GOOGLE);
  scoped_ptr<TransportDescription> offer(
      f1_.CreateOffer(TransportOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  scoped_ptr<TransportDescription> desc(f2_.CreateAnswer(
      offer.get(), TransportOptions(), NULL));
  ASSERT_TRUE(desc.get() == NULL);
}

// Test that we can update an answer properly; ICE credentials shouldn't change.
TEST_F(TransportDescriptionFactoryTest, TestAnswerIceToIceReanswer) {
  f1_.set_protocol(cricket::ICEPROTO_RFC5245);
  f2_.set_protocol(cricket::ICEPROTO_RFC5245);
  scoped_ptr<TransportDescription> offer(
      f1_.CreateOffer(TransportOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  scoped_ptr<TransportDescription> old_desc(
      f2_.CreateAnswer(offer.get(), TransportOptions(), NULL));
  ASSERT_TRUE(old_desc.get() != NULL);
  scoped_ptr<TransportDescription> desc(
      f2_.CreateAnswer(offer.get(), TransportOptions(),
                       old_desc.get()));
  ASSERT_TRUE(desc.get() != NULL);
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "",
            old_desc->ice_ufrag, old_desc->ice_pwd, "");
}

// Test that we handle answering an offer with DTLS with no DTLS.
TEST_F(TransportDescriptionFactoryTest, TestAnswerHybridToHybridDtls) {
  f1_.set_protocol(cricket::ICEPROTO_HYBRID);
  f1_.set_secure(cricket::SEC_ENABLED);
  f1_.set_identity(id1_.get());
  f2_.set_protocol(cricket::ICEPROTO_HYBRID);
  scoped_ptr<TransportDescription> offer(
      f1_.CreateOffer(TransportOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  scoped_ptr<TransportDescription> desc(
      f2_.CreateAnswer(offer.get(), TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "", "", "", "");
}

// Test that we handle answering an offer without DTLS if we have DTLS enabled,
// but fail if we require DTLS.
TEST_F(TransportDescriptionFactoryTest, TestAnswerHybridDtlsToHybrid) {
  f1_.set_protocol(cricket::ICEPROTO_HYBRID);
  f2_.set_protocol(cricket::ICEPROTO_HYBRID);
  f2_.set_secure(cricket::SEC_ENABLED);
  f2_.set_identity(id2_.get());
  scoped_ptr<TransportDescription> offer(
      f1_.CreateOffer(TransportOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  scoped_ptr<TransportDescription> desc(
      f2_.CreateAnswer(offer.get(), TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "", "", "", "");
  f2_.set_secure(cricket::SEC_REQUIRED);
  desc.reset(f2_.CreateAnswer(offer.get(), TransportOptions(),
                              NULL));
  ASSERT_TRUE(desc.get() == NULL);
}

// Test that we handle answering an DTLS offer with DTLS, both if we have
// DTLS enabled and required.
TEST_F(TransportDescriptionFactoryTest, TestAnswerHybridDtlsToHybridDtls) {
  f1_.set_protocol(cricket::ICEPROTO_HYBRID);
  f1_.set_secure(cricket::SEC_ENABLED);
  f1_.set_identity(id1_.get());

  f2_.set_protocol(cricket::ICEPROTO_HYBRID);
  f2_.set_secure(cricket::SEC_ENABLED);
  f2_.set_identity(id2_.get());
  // f2_ produces the answer that is being checked in this test, so the
  // answer must contain fingerprint lines with id2_'s digest algorithm.
  std::string digest_alg2;
  ASSERT_TRUE(id2_->certificate().GetSignatureDigestAlgorithm(&digest_alg2));

  scoped_ptr<TransportDescription> offer(
      f1_.CreateOffer(TransportOptions(), NULL));
  ASSERT_TRUE(offer.get() != NULL);
  scoped_ptr<TransportDescription> desc(
      f2_.CreateAnswer(offer.get(), TransportOptions(), NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "", "", "", digest_alg2);
  f2_.set_secure(cricket::SEC_REQUIRED);
  desc.reset(f2_.CreateAnswer(offer.get(), TransportOptions(),
                              NULL));
  CheckDesc(desc.get(), cricket::NS_JINGLE_ICE_UDP, "", "", "", digest_alg2);
}

// Test that ice ufrag and password is changed in an updated offer and answer
// if |TransportDescriptionOptions::ice_restart| is true.
TEST_F(TransportDescriptionFactoryTest, TestIceRestart) {
  TestIceRestart(false);
}

// Test that ice ufrag and password is changed in an updated offer and answer
// if |TransportDescriptionOptions::ice_restart| is true and DTLS is enabled.
TEST_F(TransportDescriptionFactoryTest, TestIceRestartWithDtls) {
  TestIceRestart(true);
}
