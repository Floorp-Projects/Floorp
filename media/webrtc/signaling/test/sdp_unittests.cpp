/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "timecard.h"

#include "CSFLog.h"

#include <string>
#include <sstream>

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

#include "nspr.h"
#include "nss.h"
#include "ssl.h"

#include "nsThreadUtils.h"
#include "FakeMediaStreams.h"
#include "FakeMediaStreamsImpl.h"
#include "FakeLogging.h"
#include "PeerConnectionImpl.h"
#include "PeerConnectionCtx.h"

#include "mtransport_test_utils.h"
MtransportTestUtils *test_utils;
nsCOMPtr<nsIThread> gThread;

#include "signaling/src/sdp/SipccSdpParser.h"
#include "signaling/src/sdp/SdpMediaSection.h"
#include "signaling/src/sdp/SdpAttribute.h"

extern "C" {
#include "signaling/src/sdp/sipcc/sdp.h"
#include "signaling/src/sdp/sipcc/sdp_private.h"
}

#ifdef CRLF
#undef CRLF
#endif
#define CRLF "\r\n"

#include "FakeIPC.h"
#include "FakeIPC.cpp"

#include "TestHarness.h"

using namespace mozilla;

namespace test {

static bool SetupGlobalThread() {
  if (!gThread) {
    nsIThread *thread;

    nsresult rv = NS_NewNamedThread("pseudo-main",&thread);
    if (NS_FAILED(rv))
      return false;

    gThread = thread;
    PeerConnectionCtx::InitializeGlobal(gThread,
                                               test_utils->sts_target());
  }
  return true;
}

class SdpTest : public ::testing::Test {
  public:
    SdpTest() : sdp_ptr_(nullptr) {
    }

    ~SdpTest() {
      sdp_free_description(sdp_ptr_);
    }

    static void SetUpTestCase() {
      ASSERT_TRUE(SetupGlobalThread());
    }

    void SetUp() {
      final_level_ = 0;
      sdp_ptr_ = nullptr;
    }

    static void TearDownTestCase() {
      if (gThread) {
        gThread->Shutdown();
      }
      gThread = nullptr;
    }

    void ResetSdp() {
      if (!sdp_ptr_) {
        sdp_free_description(sdp_ptr_);
      }

      sdp_media_e supported_media[] = {
        SDP_MEDIA_AUDIO,
        SDP_MEDIA_VIDEO,
        SDP_MEDIA_APPLICATION,
        SDP_MEDIA_DATA,
        SDP_MEDIA_CONTROL,
        SDP_MEDIA_NAS_RADIUS,
        SDP_MEDIA_NAS_TACACS,
        SDP_MEDIA_NAS_DIAMETER,
        SDP_MEDIA_NAS_L2TP,
        SDP_MEDIA_NAS_LOGIN,
        SDP_MEDIA_NAS_NONE,
        SDP_MEDIA_IMAGE,
      };

      sdp_conf_options_t *config_p = sdp_init_config();
      unsigned int i;
      for (i = 0; i < sizeof(supported_media) / sizeof(sdp_media_e); i++) {
        sdp_media_supported(config_p, supported_media[i], true);
      }
      sdp_nettype_supported(config_p, SDP_NT_INTERNET, true);
      sdp_addrtype_supported(config_p, SDP_AT_IP4, true);
      sdp_addrtype_supported(config_p, SDP_AT_IP6, true);
      sdp_transport_supported(config_p, SDP_TRANSPORT_RTPSAVPF, true);
      sdp_transport_supported(config_p, SDP_TRANSPORT_UDPTL, true);
      sdp_require_session_name(config_p, false);

      sdp_ptr_ = sdp_init_description(config_p);
      if (!sdp_ptr_) {
        sdp_free_config(config_p);
      }
    }

    void ParseSdp(const std::string &sdp_str) {
      const char *buf = sdp_str.data();
      ResetSdp();
      ASSERT_EQ(sdp_parse(sdp_ptr_, buf, sdp_str.size()), SDP_SUCCESS);
    }

    void InitLocalSdp() {
      ResetSdp();
      ASSERT_EQ(sdp_set_version(sdp_ptr_, 0), SDP_SUCCESS);
      ASSERT_EQ(sdp_set_owner_username(sdp_ptr_, "-"), SDP_SUCCESS);
      ASSERT_EQ(sdp_set_owner_sessionid(sdp_ptr_, "132954853"), SDP_SUCCESS);
      ASSERT_EQ(sdp_set_owner_version(sdp_ptr_, "0"), SDP_SUCCESS);
      ASSERT_EQ(sdp_set_owner_network_type(sdp_ptr_, SDP_NT_INTERNET),
                SDP_SUCCESS);
      ASSERT_EQ(sdp_set_owner_address_type(sdp_ptr_, SDP_AT_IP4), SDP_SUCCESS);
      ASSERT_EQ(sdp_set_owner_address(sdp_ptr_, "198.51.100.7"), SDP_SUCCESS);
      ASSERT_EQ(sdp_set_session_name(sdp_ptr_, "SDP Unit Test"), SDP_SUCCESS);
      ASSERT_EQ(sdp_set_time_start(sdp_ptr_, "0"), SDP_SUCCESS);
      ASSERT_EQ(sdp_set_time_stop(sdp_ptr_, "0"), SDP_SUCCESS);
    }

    std::string SerializeSdp() {
      flex_string fs;
      flex_string_init(&fs);
      EXPECT_EQ(sdp_build(sdp_ptr_, &fs), SDP_SUCCESS);
      std::string body(fs.buffer);
      flex_string_free(&fs);
      return body;
    }

    // Returns "level" for new media section
    int AddNewMedia(sdp_media_e type) {
      final_level_++;
      EXPECT_EQ(sdp_insert_media_line(sdp_ptr_, final_level_), SDP_SUCCESS);
      EXPECT_EQ(sdp_set_conn_nettype(sdp_ptr_, final_level_, SDP_NT_INTERNET),
                SDP_SUCCESS);
      EXPECT_EQ(sdp_set_conn_addrtype(sdp_ptr_, final_level_, SDP_AT_IP4),
                SDP_SUCCESS);
      EXPECT_EQ(sdp_set_conn_address(sdp_ptr_, final_level_, "198.51.100.7"),
                SDP_SUCCESS);
      EXPECT_EQ(sdp_set_media_type(sdp_ptr_, final_level_, SDP_MEDIA_VIDEO),
                SDP_SUCCESS);
      EXPECT_EQ(sdp_set_media_transport(sdp_ptr_, final_level_,
                                        SDP_TRANSPORT_RTPAVP),
                SDP_SUCCESS);
      EXPECT_EQ(sdp_set_media_portnum(sdp_ptr_, final_level_, 12345, 0),
                SDP_SUCCESS);
      EXPECT_EQ(sdp_add_media_payload_type(sdp_ptr_, final_level_, 120,
                                           SDP_PAYLOAD_NUMERIC),
                SDP_SUCCESS);
      return final_level_;
    }

    uint16_t AddNewRtcpFbAck(int level, sdp_rtcp_fb_ack_type_e type,
                         uint16_t payload = SDP_ALL_PAYLOADS) {
      uint16_t inst_num = 0;
      EXPECT_EQ(sdp_add_new_attr(sdp_ptr_, level, 0, SDP_ATTR_RTCP_FB,
                                 &inst_num), SDP_SUCCESS);
      EXPECT_EQ(sdp_attr_set_rtcp_fb_ack(sdp_ptr_, level, payload, inst_num,
                                         type), SDP_SUCCESS);
      return inst_num;
    }

    uint16_t AddNewRtcpFbNack(int level, sdp_rtcp_fb_nack_type_e type,
                         uint16_t payload = SDP_ALL_PAYLOADS) {
      uint16_t inst_num = 0;
      EXPECT_EQ(sdp_add_new_attr(sdp_ptr_, level, 0, SDP_ATTR_RTCP_FB,
                                 &inst_num), SDP_SUCCESS);
      EXPECT_EQ(sdp_attr_set_rtcp_fb_nack(sdp_ptr_, level, payload, inst_num,
                                          type), SDP_SUCCESS);
      return inst_num;
    }

    uint16_t AddNewRtcpFbTrrInt(int level, uint32_t interval,
                         uint16_t payload = SDP_ALL_PAYLOADS) {
      uint16_t inst_num = 0;
      EXPECT_EQ(sdp_add_new_attr(sdp_ptr_, level, 0, SDP_ATTR_RTCP_FB,
                                 &inst_num), SDP_SUCCESS);
      EXPECT_EQ(sdp_attr_set_rtcp_fb_trr_int(sdp_ptr_, level, payload, inst_num,
                                             interval), SDP_SUCCESS);
      return inst_num;
    }

    uint16_t AddNewRtcpFbCcm(int level, sdp_rtcp_fb_ccm_type_e type,
                         uint16_t payload = SDP_ALL_PAYLOADS) {
      uint16_t inst_num = 0;
      EXPECT_EQ(sdp_add_new_attr(sdp_ptr_, level, 0, SDP_ATTR_RTCP_FB,
                                 &inst_num), SDP_SUCCESS);
      EXPECT_EQ(sdp_attr_set_rtcp_fb_ccm(sdp_ptr_, level, payload, inst_num,
                                         type), SDP_SUCCESS);
      return inst_num;
    }
    uint16_t AddNewExtMap(int level, const char* uri) {
      uint16_t inst_num = 0;
      EXPECT_EQ(sdp_add_new_attr(sdp_ptr_, level, 0, SDP_ATTR_EXTMAP,
                                 &inst_num), SDP_SUCCESS);
      EXPECT_EQ(sdp_attr_set_extmap(sdp_ptr_, level, inst_num,
                                    uri, inst_num), SDP_SUCCESS);
      return inst_num;
    }

    uint16_t AddNewFmtpMaxFs(int level, uint32_t max_fs) {
      uint16_t inst_num = 0;
      EXPECT_EQ(sdp_add_new_attr(sdp_ptr_, level, 0, SDP_ATTR_FMTP,
                                 &inst_num), SDP_SUCCESS);
      EXPECT_EQ(sdp_attr_set_fmtp_payload_type(sdp_ptr_, level, 0, inst_num,
                                               120), SDP_SUCCESS);
      EXPECT_EQ(sdp_attr_set_fmtp_max_fs(sdp_ptr_, level, 0, inst_num, max_fs),
                                         SDP_SUCCESS);
      return inst_num;
    }

    uint16_t AddNewFmtpMaxFr(int level, uint32_t max_fr) {
      uint16_t inst_num = 0;
      EXPECT_EQ(sdp_add_new_attr(sdp_ptr_, level, 0, SDP_ATTR_FMTP,
                                 &inst_num), SDP_SUCCESS);
      EXPECT_EQ(sdp_attr_set_fmtp_payload_type(sdp_ptr_, level, 0, inst_num,
                                               120), SDP_SUCCESS);
      EXPECT_EQ(sdp_attr_set_fmtp_max_fr(sdp_ptr_, level, 0, inst_num, max_fr),
                                         SDP_SUCCESS);
      return inst_num;
    }

     uint16_t AddNewFmtpMaxFsFr(int level, uint32_t max_fs, uint32_t max_fr) {
      uint16_t inst_num = 0;
      EXPECT_EQ(sdp_add_new_attr(sdp_ptr_, level, 0, SDP_ATTR_FMTP,
                                 &inst_num), SDP_SUCCESS);
      EXPECT_EQ(sdp_attr_set_fmtp_payload_type(sdp_ptr_, level, 0, inst_num,
                                               120), SDP_SUCCESS);
      EXPECT_EQ(sdp_attr_set_fmtp_max_fs(sdp_ptr_, level, 0, inst_num, max_fs),
                                         SDP_SUCCESS);
      EXPECT_EQ(sdp_attr_set_fmtp_max_fr(sdp_ptr_, level, 0, inst_num, max_fr),
                                         SDP_SUCCESS);
      return inst_num;
    }

  protected:
    int final_level_;
    sdp_t *sdp_ptr_;
};

static const std::string kVideoSdp =
  "v=0\r\n"
  "o=- 4294967296 2 IN IP4 127.0.0.1\r\n"
  "s=SIP Call\r\n"
  "c=IN IP4 198.51.100.7\r\n"
  "t=0 0\r\n"
  "m=video 56436 RTP/SAVPF 120\r\n"
  "a=rtpmap:120 VP8/90000\r\n";

TEST_F(SdpTest, parseRtcpFbAckRpsi) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 ack rpsi\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ack(sdp_ptr_, 1, 120, 1),
            SDP_RTCP_FB_ACK_RPSI);
}

TEST_F(SdpTest, parseRtcpFbAckApp) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 ack app\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ack(sdp_ptr_, 1, 120, 1), SDP_RTCP_FB_ACK_APP);
}

TEST_F(SdpTest, parseRtcpFbAckAppFoo) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 ack app foo\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ack(sdp_ptr_, 1, 120, 1), SDP_RTCP_FB_ACK_APP);
}

TEST_F(SdpTest, parseRtcpFbAckFooBar) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 ack foo bar\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ack(sdp_ptr_, 1, 120, 1),
            SDP_RTCP_FB_ACK_UNKNOWN);
}

TEST_F(SdpTest, parseRtcpFbAckFooBarBaz) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 ack foo bar baz\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ack(sdp_ptr_, 1, 120, 1),
            SDP_RTCP_FB_ACK_UNKNOWN);
}

TEST_F(SdpTest, parseRtcpFbNack) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 nack\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_nack(sdp_ptr_, 1, 120, 1),
            SDP_RTCP_FB_NACK_BASIC);
}

TEST_F(SdpTest, parseRtcpFbNackPli) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 nack pli\r\n");
}

TEST_F(SdpTest, parseRtcpFbNackSli) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 nack sli\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_nack(sdp_ptr_, 1, 120, 1),
            SDP_RTCP_FB_NACK_SLI);
}

TEST_F(SdpTest, parseRtcpFbNackRpsi) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 nack rpsi\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_nack(sdp_ptr_, 1, 120, 1),
            SDP_RTCP_FB_NACK_RPSI);
}

TEST_F(SdpTest, parseRtcpFbNackApp) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 nack app\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_nack(sdp_ptr_, 1, 120, 1),
            SDP_RTCP_FB_NACK_APP);
}

TEST_F(SdpTest, parseRtcpFbNackAppFoo) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 nack app foo\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_nack(sdp_ptr_, 1, 120, 1),
            SDP_RTCP_FB_NACK_APP);
}

TEST_F(SdpTest, parseRtcpFbNackAppFooBar) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 nack app foo bar\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_nack(sdp_ptr_, 1, 120, 1),
            SDP_RTCP_FB_NACK_APP);
}

TEST_F(SdpTest, parseRtcpFbNackFooBarBaz) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 nack foo bar baz\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_nack(sdp_ptr_, 1, 120, 1),
            SDP_RTCP_FB_NACK_UNKNOWN);
}

TEST_F(SdpTest, parseRtcpFbTrrInt0) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 trr-int 0\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_trr_int(sdp_ptr_, 1, 120, 1), 0U);
}

TEST_F(SdpTest, parseRtcpFbTrrInt123) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 trr-int 123\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_trr_int(sdp_ptr_, 1, 120, 1), 123U);
}

TEST_F(SdpTest, parseRtcpFbCcmFir) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 ccm fir\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ccm(sdp_ptr_, 1, 120, 1), SDP_RTCP_FB_CCM_FIR);
}

TEST_F(SdpTest, parseRtcpFbCcmTmmbr) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 ccm tmmbr\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ccm(sdp_ptr_, 1, 120, 1),
            SDP_RTCP_FB_CCM_TMMBR);
}

TEST_F(SdpTest, parseRtcpFbCcmTmmbrSmaxpr) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 ccm tmmbr smaxpr=456\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ccm(sdp_ptr_, 1, 120, 1),
            SDP_RTCP_FB_CCM_TMMBR);
}

TEST_F(SdpTest, parseRtcpFbCcmTstr) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 ccm tstr\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ccm(sdp_ptr_, 1, 120, 1),
            SDP_RTCP_FB_CCM_TSTR);
}

TEST_F(SdpTest, parseRtcpFbCcmVbcm) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 ccm vbcm 123 456 789\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ccm(sdp_ptr_, 1, 120, 1),
                                     SDP_RTCP_FB_CCM_VBCM);
  // We don't currently parse out VBCM submessage types, since we don't have
  // any use for them.
}

TEST_F(SdpTest, parseRtcpFbCcmFoo) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 ccm foo\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ccm(sdp_ptr_, 1, 120, 1),
            SDP_RTCP_FB_CCM_UNKNOWN);
}

TEST_F(SdpTest, parseRtcpFbCcmFooBarBaz) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 ccm foo bar baz\r\n");
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ccm(sdp_ptr_, 1, 120, 1),
            SDP_RTCP_FB_CCM_UNKNOWN);
}

TEST_F(SdpTest, parseRtcpFbFoo) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 foo\r\n");
}

TEST_F(SdpTest, parseRtcpFbFooBar) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 foo bar\r\n");
}

TEST_F(SdpTest, parseRtcpFbFooBarBaz) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:120 foo bar baz\r\n");
}


TEST_F(SdpTest, parseRtcpFbKitchenSink) {
  ParseSdp(kVideoSdp +
    "a=rtcp-fb:120 ack rpsi\r\n"
    "a=rtcp-fb:120 ack app\r\n"
    "a=rtcp-fb:120 ack app foo\r\n"
    "a=rtcp-fb:120 ack foo bar\r\n"
    "a=rtcp-fb:120 ack foo bar baz\r\n"
    "a=rtcp-fb:120 nack\r\n"
    "a=rtcp-fb:120 nack pli\r\n"
    "a=rtcp-fb:120 nack sli\r\n"
    "a=rtcp-fb:120 nack rpsi\r\n"
    "a=rtcp-fb:120 nack app\r\n"
    "a=rtcp-fb:120 nack app foo\r\n"
    "a=rtcp-fb:120 nack app foo bar\r\n"
    "a=rtcp-fb:120 nack foo bar baz\r\n"
    "a=rtcp-fb:120 trr-int 0\r\n"
    "a=rtcp-fb:120 trr-int 123\r\n"
    "a=rtcp-fb:120 ccm fir\r\n"
    "a=rtcp-fb:120 ccm tmmbr\r\n"
    "a=rtcp-fb:120 ccm tmmbr smaxpr=456\r\n"
    "a=rtcp-fb:120 ccm tstr\r\n"
    "a=rtcp-fb:120 ccm vbcm 123 456 789\r\n"
    "a=rtcp-fb:120 ccm foo\r\n"
    "a=rtcp-fb:120 ccm foo bar baz\r\n"
    "a=rtcp-fb:120 foo\r\n"
    "a=rtcp-fb:120 foo bar\r\n"
    "a=rtcp-fb:120 foo bar baz\r\n");

  ASSERT_EQ(sdp_attr_get_rtcp_fb_ack(sdp_ptr_, 1, 120, 1), SDP_RTCP_FB_ACK_RPSI);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ack(sdp_ptr_, 1, 120, 2), SDP_RTCP_FB_ACK_APP);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ack(sdp_ptr_, 1, 120, 3), SDP_RTCP_FB_ACK_APP);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ack(sdp_ptr_, 1, 120, 4),
            SDP_RTCP_FB_ACK_UNKNOWN);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ack(sdp_ptr_, 1, 120, 5),
            SDP_RTCP_FB_ACK_UNKNOWN);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ack(sdp_ptr_, 1, 120, 6),
            SDP_RTCP_FB_ACK_NOT_FOUND);

  ASSERT_EQ(sdp_attr_get_rtcp_fb_nack(sdp_ptr_, 1, 120, 1),
            SDP_RTCP_FB_NACK_BASIC);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_nack(sdp_ptr_, 1, 120, 2),
            SDP_RTCP_FB_NACK_PLI);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_nack(sdp_ptr_, 1, 120, 3),
            SDP_RTCP_FB_NACK_SLI);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_nack(sdp_ptr_, 1, 120, 4),
            SDP_RTCP_FB_NACK_RPSI);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_nack(sdp_ptr_, 1, 120, 5),
            SDP_RTCP_FB_NACK_APP);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_nack(sdp_ptr_, 1, 120, 6),
            SDP_RTCP_FB_NACK_APP);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_nack(sdp_ptr_, 1, 120, 7),
            SDP_RTCP_FB_NACK_APP);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_nack(sdp_ptr_, 1, 120, 8),
            SDP_RTCP_FB_NACK_UNKNOWN);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_nack(sdp_ptr_, 1, 120, 9),
            SDP_RTCP_FB_NACK_NOT_FOUND);

  ASSERT_EQ(sdp_attr_get_rtcp_fb_trr_int(sdp_ptr_, 1, 120, 1), 0U);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_trr_int(sdp_ptr_, 1, 120, 2), 123U);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_trr_int(sdp_ptr_, 1, 120, 3), 0xFFFFFFFF);

  ASSERT_EQ(sdp_attr_get_rtcp_fb_ccm(sdp_ptr_, 1, 120, 1), SDP_RTCP_FB_CCM_FIR);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ccm(sdp_ptr_, 1, 120, 2),
            SDP_RTCP_FB_CCM_TMMBR);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ccm(sdp_ptr_, 1, 120, 3),
            SDP_RTCP_FB_CCM_TMMBR);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ccm(sdp_ptr_, 1, 120, 4),
            SDP_RTCP_FB_CCM_TSTR);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ccm(sdp_ptr_, 1, 120, 5),
            SDP_RTCP_FB_CCM_VBCM);
  // We don't currently parse out VBCM submessage types, since we don't have
  // any use for them.
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ccm(sdp_ptr_, 1, 120, 6),
            SDP_RTCP_FB_CCM_UNKNOWN);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ccm(sdp_ptr_, 1, 120, 7),
            SDP_RTCP_FB_CCM_UNKNOWN);
  ASSERT_EQ(sdp_attr_get_rtcp_fb_ccm(sdp_ptr_, 1, 120, 8),
            SDP_RTCP_FB_CCM_NOT_FOUND);
}

TEST_F(SdpTest, addRtcpFbAckRpsi) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbAck(level, SDP_RTCP_FB_ACK_RPSI, 120);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:120 ack rpsi\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbAckRpsiAllPt) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbAck(level, SDP_RTCP_FB_ACK_RPSI);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:* ack rpsi\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbAckApp) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbAck(level, SDP_RTCP_FB_ACK_APP, 120);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:120 ack app\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbAckAppAllPt) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbAck(level, SDP_RTCP_FB_ACK_APP);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:* ack app\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbNack) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbNack(level, SDP_RTCP_FB_NACK_BASIC, 120);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:120 nack\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbNackAllPt) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbNack(level, SDP_RTCP_FB_NACK_BASIC);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:* nack\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbNackSli) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbNack(level, SDP_RTCP_FB_NACK_SLI, 120);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:120 nack sli\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbNackSliAllPt) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbNack(level, SDP_RTCP_FB_NACK_SLI);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:* nack sli\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbNackPli) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbNack(level, SDP_RTCP_FB_NACK_PLI, 120);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:120 nack pli\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbNackPliAllPt) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbNack(level, SDP_RTCP_FB_NACK_PLI);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:* nack pli\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbNackRpsi) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbNack(level, SDP_RTCP_FB_NACK_RPSI, 120);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:120 nack rpsi\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbNackRpsiAllPt) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbNack(level, SDP_RTCP_FB_NACK_RPSI);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:* nack rpsi\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbNackApp) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbNack(level, SDP_RTCP_FB_NACK_APP, 120);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:120 nack app\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbNackAppAllPt) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbNack(level, SDP_RTCP_FB_NACK_APP);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:* nack app\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbNackRai) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbNack(level, SDP_RTCP_FB_NACK_RAI, 120);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:120 nack rai\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbNackRaiAllPt) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbNack(level, SDP_RTCP_FB_NACK_RAI);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:* nack rai\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbNackTllei) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbNack(level, SDP_RTCP_FB_NACK_TLLEI, 120);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:120 nack tllei\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbNackTlleiAllPt) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbNack(level, SDP_RTCP_FB_NACK_TLLEI);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:* nack tllei\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbNackPslei) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbNack(level, SDP_RTCP_FB_NACK_PSLEI, 120);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:120 nack pslei\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbNackPsleiAllPt) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbNack(level, SDP_RTCP_FB_NACK_PSLEI);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:* nack pslei\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbNackEcn) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbNack(level, SDP_RTCP_FB_NACK_ECN, 120);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:120 nack ecn\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbNackEcnAllPt) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbNack(level, SDP_RTCP_FB_NACK_ECN);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:* nack ecn\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbTrrInt) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbTrrInt(level, 12345, 120);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:120 trr-int 12345\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbNackTrrIntAllPt) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbTrrInt(level, 0);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:* trr-int 0\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbCcmFir) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbCcm(level, SDP_RTCP_FB_CCM_FIR, 120);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:120 ccm fir\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbCcmFirAllPt) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbCcm(level, SDP_RTCP_FB_CCM_FIR);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:* ccm fir\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbCcmTmmbr) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbCcm(level, SDP_RTCP_FB_CCM_TMMBR, 120);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:120 ccm tmmbr\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbCcmTmmbrAllPt) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbCcm(level, SDP_RTCP_FB_CCM_TMMBR);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:* ccm tmmbr\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbCcmTstr) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbCcm(level, SDP_RTCP_FB_CCM_TSTR, 120);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:120 ccm tstr\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbCcmTstrAllPt) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbCcm(level, SDP_RTCP_FB_CCM_TSTR);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:* ccm tstr\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbCcmVbcm) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbCcm(level, SDP_RTCP_FB_CCM_VBCM, 120);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:120 ccm vbcm\r\n"), std::string::npos);
}

TEST_F(SdpTest, addRtcpFbCcmVbcmAllPt) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewRtcpFbCcm(level, SDP_RTCP_FB_CCM_VBCM);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=rtcp-fb:* ccm vbcm\r\n"), std::string::npos);
}

TEST_F(SdpTest, parseRtcpFbAllPayloads) {
  ParseSdp(kVideoSdp + "a=rtcp-fb:* ack rpsi\r\n");
  for (int i = 0; i < 128; i++) {
    ASSERT_EQ(sdp_attr_get_rtcp_fb_ack(sdp_ptr_, 1, i, 1),
              SDP_RTCP_FB_ACK_RPSI);
  }
}
TEST_F(SdpTest, addExtMap) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewExtMap(level, SDP_EXTMAP_AUDIO_LEVEL);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\n"), std::string::npos);
}

TEST_F(SdpTest, parseExtMap) {
  ParseSdp(kVideoSdp +
    "a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\n");
  ASSERT_STREQ(sdp_attr_get_extmap_uri(sdp_ptr_, 1, 1),
            SDP_EXTMAP_AUDIO_LEVEL);
  ASSERT_EQ(sdp_attr_get_extmap_id(sdp_ptr_, 1, 1),
            1);

}

TEST_F(SdpTest, parseFmtpMaxFs) {
  uint32_t val = 0;
  ParseSdp(kVideoSdp + "a=fmtp:120 max-fs=300;max-fr=30\r\n");
  ASSERT_EQ(sdp_attr_get_fmtp_max_fs(sdp_ptr_, 1, 0, 1, &val), SDP_SUCCESS);
  ASSERT_EQ(val, 300U);
}
TEST_F(SdpTest, parseFmtpMaxFr) {
  uint32_t val = 0;
  ParseSdp(kVideoSdp + "a=fmtp:120 max-fs=300;max-fr=30\r\n");
  ASSERT_EQ(sdp_attr_get_fmtp_max_fr(sdp_ptr_, 1, 0, 1, &val), SDP_SUCCESS);
  ASSERT_EQ(val, 30U);
}

TEST_F(SdpTest, addFmtpMaxFs) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewFmtpMaxFs(level, 300);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=fmtp:120 max-fs=300\r\n"), std::string::npos);
}

TEST_F(SdpTest, addFmtpMaxFr) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewFmtpMaxFr(level, 30);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=fmtp:120 max-fr=30\r\n"), std::string::npos);
}

TEST_F(SdpTest, addFmtpMaxFsFr) {
  InitLocalSdp();
  int level = AddNewMedia(SDP_MEDIA_VIDEO);
  AddNewFmtpMaxFsFr(level, 300, 30);
  std::string body = SerializeSdp();
  ASSERT_NE(body.find("a=fmtp:120 max-fs=300;max-fr=30\r\n"),
            std::string::npos);
}

static const std::string kBrokenFmtp =
  "v=0\r\n"
  "o=- 4294967296 2 IN IP4 127.0.0.1\r\n"
  "s=SIP Call\r\n"
  "t=0 0\r\n"
  "m=video 56436 RTP/SAVPF 120\r\n"
  "c=IN IP4 198.51.100.7\r\n"
  "a=rtpmap:120 VP8/90000\r\n"
  /* Note: the \0 in this string triggered bz://1089207
   */
  "a=fmtp:120 max-fs=300;max\0fr=30";

TEST_F(SdpTest, parseBrokenFmtp) {
  uint32_t val = 0;
  const char *buf = kBrokenFmtp.data();
  ResetSdp();
  /* We need to manually invoke the parser here to be able to specify the length
   * of the string beyond the \0 in last line of the string.
   */
  ASSERT_EQ(sdp_parse(sdp_ptr_, buf, 165), SDP_SUCCESS);
  ASSERT_EQ(sdp_attr_get_fmtp_max_fs(sdp_ptr_, 1, 0, 1, &val), SDP_INVALID_PARAMETER);
}

TEST_F(SdpTest, addIceLite) {
    InitLocalSdp();
    uint16_t inst_num = 0;
    EXPECT_EQ(sdp_add_new_attr(sdp_ptr_, SDP_SESSION_LEVEL, 0,
                               SDP_ATTR_ICE_LITE, &inst_num), SDP_SUCCESS);
    std::string body = SerializeSdp();
    ASSERT_NE(body.find("a=ice-lite\r\n"), std::string::npos);
}

TEST_F(SdpTest, parseIceLite) {
    std::string sdp =
        "v=0\r\n"
        "o=- 4294967296 2 IN IP4 127.0.0.1\r\n"
        "s=SIP Call\r\n"
        "t=0 0\r\n"
        "a=ice-lite\r\n";
  ParseSdp(sdp);
  ASSERT_TRUE(sdp_attr_is_present(sdp_ptr_, SDP_ATTR_ICE_LITE,
                                  SDP_SESSION_LEVEL, 0));
}

class NewSdpTest : public ::testing::Test,
                   public ::testing::WithParamInterface<bool> {
  public:
    NewSdpTest() {}

    void ParseSdp(const std::string &sdp, bool expectSuccess = true) {
      mSdp = mozilla::Move(mParser.Parse(sdp));

      // Are we configured to do a parse and serialize before actually
      // running the test?
      if (GetParam()) {
        std::stringstream os;

        if (expectSuccess) {
          ASSERT_TRUE(!!mSdp) << "Parse failed on first pass: "
                              << GetParseErrors();
        }

        if (mSdp) {
          // Serialize and re-parse
          mSdp->Serialize(os);
          mSdp = mozilla::Move(mParser.Parse(os.str()));

          // Whether we expected the parse to work or not, it should
          // succeed the second time if it succeeded the first.
          ASSERT_TRUE(!!mSdp) << "Parse failed on second pass, SDP was: "
            << std::endl << os.str() <<  std::endl
            << "Errors were: " << GetParseErrors();

          // Serialize again and compare
          std::stringstream os2;
          mSdp->Serialize(os2);
          ASSERT_EQ(os.str(), os2.str());
        }
      }

      if (expectSuccess) {
        ASSERT_TRUE(!!mSdp) << "Parse failed: " << GetParseErrors();
        ASSERT_EQ(0U, mParser.GetParseErrors().size())
                  << "Got unexpected parse errors/warnings: "
                  << GetParseErrors();
      }
    }

    // For streaming parse errors
    std::string GetParseErrors() const {
      std::stringstream output;
      for (auto e = mParser.GetParseErrors().begin();
           e != mParser.GetParseErrors().end();
           ++e) {
        output << e->first << ": " << e->second << std::endl;
      }
      return output.str();
    }

    void CheckRtpmap(const std::string& expected_pt,
                     SdpRtpmapAttributeList::CodecType codec,
                     const std::string& name,
                     uint32_t clock,
                     uint16_t channels,
                     const std::string& search_pt,
                     const SdpRtpmapAttributeList& rtpmaps) const {
      ASSERT_TRUE(rtpmaps.HasEntry(search_pt));
      auto attr = rtpmaps.GetEntry(search_pt);
      ASSERT_EQ(expected_pt, attr.pt);
      ASSERT_EQ(codec, attr.codec);
      ASSERT_EQ(name, attr.name);
      ASSERT_EQ(clock, attr.clock);
      ASSERT_EQ(channels, attr.channels);
    }

    void CheckSctpmap(const std::string& expected_pt,
                      const std::string& name,
                      uint16_t streams,
                      const std::string& search_pt,
                      const SdpSctpmapAttributeList& sctpmaps) const {
      ASSERT_TRUE(sctpmaps.HasEntry(search_pt));
      auto attr = sctpmaps.GetEntry(search_pt);
      ASSERT_EQ(expected_pt, search_pt);
      ASSERT_EQ(expected_pt, attr.pt);
      ASSERT_EQ(name, attr.name);
      ASSERT_EQ(streams, attr.streams);
    }

    void CheckRtcpFb(const SdpRtcpFbAttributeList::Feedback& feedback,
                     const std::string& pt,
                     SdpRtcpFbAttributeList::Type type,
                     const std::string& first_parameter,
                     const std::string& extra = "") const {
      ASSERT_EQ(pt, feedback.pt);
      ASSERT_EQ(type, feedback.type);
      ASSERT_EQ(first_parameter, feedback.parameter);
      ASSERT_EQ(extra, feedback.extra);
    }

    void CheckSerialize(const std::string& expected,
                        const SdpAttribute& attr) const {
      std::stringstream str;
      attr.Serialize(str);
      ASSERT_EQ(expected, str.str());
    }

    SipccSdpParser mParser;
    mozilla::UniquePtr<Sdp> mSdp;
}; // class NewSdpTest

TEST_P(NewSdpTest, CreateDestroy) {
}

TEST_P(NewSdpTest, ParseEmpty) {
  ParseSdp("", false);
  ASSERT_FALSE(mSdp);
  ASSERT_NE(0U, mParser.GetParseErrors().size())
    << "Expected at least one parse error.";
}

const std::string kBadSdp = "This is SDPARTA!!!!";

TEST_P(NewSdpTest, ParseGarbage) {
  ParseSdp(kBadSdp, false);
  ASSERT_FALSE(mSdp);
  ASSERT_NE(0U, mParser.GetParseErrors().size())
    << "Expected at least one parse error.";
}

TEST_P(NewSdpTest, ParseGarbageTwice) {
  ParseSdp(kBadSdp, false);
  ASSERT_FALSE(mSdp);
  size_t errorCount = mParser.GetParseErrors().size();
  ASSERT_NE(0U, errorCount)
    << "Expected at least one parse error.";
  ParseSdp(kBadSdp, false);
  ASSERT_FALSE(mSdp);
  ASSERT_EQ(errorCount, mParser.GetParseErrors().size())
    << "Expected same error count for same SDP.";
}

TEST_P(NewSdpTest, ParseMinimal) {
  ParseSdp(kVideoSdp);
  ASSERT_EQ(0U, mParser.GetParseErrors().size()) <<
    "Got parse errors: " << GetParseErrors();
}

TEST_P(NewSdpTest, CheckOriginGetUsername) {
  ParseSdp(kVideoSdp);
  ASSERT_EQ("-", mSdp->GetOrigin().GetUsername())
    << "Wrong username in origin";
}

TEST_P(NewSdpTest, CheckOriginGetSessionId) {
  ParseSdp(kVideoSdp);
  ASSERT_EQ(4294967296U, mSdp->GetOrigin().GetSessionId())
    << "Wrong session id in origin";
}

TEST_P(NewSdpTest, CheckOriginGetSessionVersion) {
  ParseSdp(kVideoSdp);
  ASSERT_EQ(2U , mSdp->GetOrigin().GetSessionVersion())
    << "Wrong version in origin";
}

TEST_P(NewSdpTest, CheckOriginGetAddrType) {
  ParseSdp(kVideoSdp);
  ASSERT_EQ(sdp::kIPv4, mSdp->GetOrigin().GetAddrType())
    << "Wrong address type in origin";
}

TEST_P(NewSdpTest, CheckOriginGetAddress) {
  ParseSdp(kVideoSdp);
  ASSERT_EQ("127.0.0.1" , mSdp->GetOrigin().GetAddress())
    << "Wrong address in origin";
}

TEST_P(NewSdpTest, CheckGetMissingBandwidth) {
  ParseSdp(kVideoSdp);
  ASSERT_EQ(0U, mSdp->GetBandwidth("CT"))
    << "Wrong bandwidth in session";
}

TEST_P(NewSdpTest, CheckGetBandwidth) {
  ParseSdp("v=0" CRLF
           "o=- 4294967296 2 IN IP4 127.0.0.1" CRLF
           "s=SIP Call" CRLF
           "c=IN IP4 198.51.100.7" CRLF
           "b=CT:5000" CRLF
           "b=FOOBAR:10" CRLF
           "b=AS:4" CRLF
           "t=0 0" CRLF
           "m=video 56436 RTP/SAVPF 120" CRLF
           "a=rtpmap:120 VP8/90000" CRLF
           );
  ASSERT_EQ(5000U, mSdp->GetBandwidth("CT"))
    << "Wrong CT bandwidth in session";
  ASSERT_EQ(0U, mSdp->GetBandwidth("FOOBAR"))
    << "Wrong FOOBAR bandwidth in session";
  ASSERT_EQ(4U, mSdp->GetBandwidth("AS"))
    << "Wrong AS bandwidth in session";
}

TEST_P(NewSdpTest, CheckGetMediaSectionsCount) {
  ParseSdp(kVideoSdp);
  ASSERT_EQ(1U, mSdp->GetMediaSectionCount())
    << "Wrong number of media sections";
}

TEST_P(NewSdpTest, CheckMediaSectionGetMediaType) {
  ParseSdp(kVideoSdp);
  ASSERT_EQ(SdpMediaSection::kVideo, mSdp->GetMediaSection(0).GetMediaType())
    << "Wrong type for first media section";
}

TEST_P(NewSdpTest, CheckMediaSectionGetProtocol) {
  ParseSdp(kVideoSdp);
  ASSERT_EQ(SdpMediaSection::kRtpSavpf, mSdp->GetMediaSection(0).GetProtocol())
    << "Wrong protocol for video";
}

TEST_P(NewSdpTest, CheckMediaSectionGetFormats) {
  ParseSdp(kVideoSdp);
  auto video_formats = mSdp->GetMediaSection(0).GetFormats();
  ASSERT_EQ(1U, video_formats.size()) << "Wrong number of formats for video";
  ASSERT_EQ("120", video_formats[0]);
}

TEST_P(NewSdpTest, CheckMediaSectionGetPort) {
  ParseSdp(kVideoSdp);
  ASSERT_EQ(56436U, mSdp->GetMediaSection(0).GetPort())
    << "Wrong port number in media section";
}

TEST_P(NewSdpTest, CheckMediaSectionGetMissingPortCount) {
  ParseSdp(kVideoSdp);
  ASSERT_EQ(0U, mSdp->GetMediaSection(0).GetPortCount())
    << "Wrong port count in media section";
}

TEST_P(NewSdpTest, CheckMediaSectionGetPortCount) {
  ParseSdp(kVideoSdp +
      "m=audio 12345/2 RTP/SAVPF 0" CRLF
      "a=rtpmap:0 PCMU/8000" CRLF
      );
  ASSERT_EQ(2U, mSdp->GetMediaSectionCount())
    << "Wrong number of media sections";
  ASSERT_EQ(2U, mSdp->GetMediaSection(1).GetPortCount())
    << "Wrong port count in media section";
}

TEST_P(NewSdpTest, CheckMediaSectionGetMissingBandwidth) {
  ParseSdp(kVideoSdp);
  ASSERT_EQ(0U, mSdp->GetMediaSection(0).GetBandwidth("CT"))
    << "Wrong bandwidth in media section";
}

TEST_P(NewSdpTest, CheckMediaSectionGetBandwidth) {
  ParseSdp("v=0\r\n"
           "o=- 4294967296 2 IN IP4 127.0.0.1\r\n"
           "c=IN IP4 198.51.100.7\r\n"
           "t=0 0\r\n"
           "m=video 56436 RTP/SAVPF 120\r\n"
           "b=CT:1000\r\n"
           "a=rtpmap:120 VP8/90000\r\n");
  ASSERT_EQ(1000U, mSdp->GetMediaSection(0).GetBandwidth("CT"))
    << "Wrong bandwidth in media section";
}

// Define a string that is 258 characters long. We use a long string here so
// that we can test that we are able to parse and handle a string longer than
// the default maximum length of 256 in sipcc.
#define ID_A "1234567890abcdef"
#define ID_B ID_A ID_A ID_A ID_A
#define LONG_IDENTITY ID_B ID_B ID_B ID_B "xx"

// SDP from a basic A/V apprtc call FFX/FFX
const std::string kBasicAudioVideoOffer =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=ice-ufrag:4a799b2e" CRLF
"a=ice-pwd:e4cc12a910f106a0a744719425510e17" CRLF
"a=ice-lite" CRLF
"a=ice-options:trickle foo" CRLF
"a=msid-semantic:WMS stream streama" CRLF
"a=msid-semantic:foo stream" CRLF
"a=fingerprint:sha-256 DF:2E:AC:8A:FD:0A:8E:99:BF:5D:E8:3C:E7:FA:FB:08:3B:3C:54:1D:D7:D4:05:77:A0:72:9B:14:08:6D:0F:4C" CRLF
"a=identity:" LONG_IDENTITY CRLF
"a=group:BUNDLE first second" CRLF
"a=group:BUNDLE third" CRLF
"a=group:LS first third" CRLF
"m=audio 9 RTP/SAVPF 109 9 0 8 101" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=mid:first" CRLF
"a=rtpmap:109 opus/48000/2" CRLF
"a=fmtp:109 maxplaybackrate=32000;stereo=1" CRLF
"a=ptime:20" CRLF
"a=maxptime:20" CRLF
"a=rtpmap:9 G722/8000" CRLF
"a=rtpmap:0 PCMU/8000" CRLF
"a=rtpmap:8 PCMA/8000" CRLF
"a=rtpmap:101 telephone-event/8000" CRLF
"a=fmtp:101 0-15" CRLF
"a=ice-ufrag:00000000" CRLF
"a=ice-pwd:0000000000000000000000000000000" CRLF
"a=sendonly" CRLF
"a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level" CRLF
"a=setup:actpass" CRLF
"a=rtcp-mux" CRLF
"a=msid:stream track" CRLF
"a=candidate:0 1 UDP 2130379007 10.0.0.36 62453 typ host" CRLF
"a=candidate:2 1 UDP 1694236671 24.6.134.204 62453 typ srflx raddr 10.0.0.36 rport 62453" CRLF
"a=candidate:3 1 UDP 100401151 162.222.183.171 49761 typ relay raddr 162.222.183.171 rport 49761" CRLF
"a=candidate:6 1 UDP 16515071 162.222.183.171 51858 typ relay raddr 162.222.183.171 rport 51858" CRLF
"a=candidate:3 2 UDP 100401150 162.222.183.171 62454 typ relay raddr 162.222.183.171 rport 62454" CRLF
"a=candidate:2 2 UDP 1694236670 24.6.134.204 55428 typ srflx raddr 10.0.0.36 rport 55428" CRLF
"a=candidate:6 2 UDP 16515070 162.222.183.171 50340 typ relay raddr 162.222.183.171 rport 50340" CRLF
"a=candidate:0 2 UDP 2130379006 10.0.0.36 55428 typ host" CRLF
"a=rtcp:62454 IN IP4 162.222.183.171" CRLF
"a=end-of-candidates" CRLF
"a=ssrc:5150" CRLF
"m=video 9 RTP/SAVPF 120 121" CRLF
"c=IN IP6 ::1" CRLF
"a=fingerprint:sha-1 DF:FA:FB:08:3B:3C:54:1D:D7:D4:05:77:A0:72:9B:14:08:6D:0F:4C:2E:AC:8A:FD:0A:8E:99:BF:5D:E8:3C:E7" CRLF
"a=mid:second" CRLF
"a=rtpmap:120 VP8/90000" CRLF
"a=fmtp:120 max-fs=3600;max-fr=30" CRLF
"a=rtpmap:121 VP9/90000" CRLF
"a=fmtp:121 max-fs=3600;max-fr=30" CRLF
"a=recvonly" CRLF
"a=rtcp-fb:120 nack" CRLF
"a=rtcp-fb:120 nack pli" CRLF
"a=rtcp-fb:120 ccm fir" CRLF
"a=rtcp-fb:121 nack" CRLF
"a=rtcp-fb:121 nack pli" CRLF
"a=rtcp-fb:121 ccm fir" CRLF
"a=setup:active" CRLF
"a=rtcp-mux" CRLF
"a=msid:streama tracka" CRLF
"a=msid:streamb trackb" CRLF
"a=candidate:0 1 UDP 2130379007 10.0.0.36 59530 typ host" CRLF
"a=candidate:0 2 UDP 2130379006 10.0.0.36 64378 typ host" CRLF
"a=candidate:2 2 UDP 1694236670 24.6.134.204 64378 typ srflx raddr 10.0.0.36 rport 64378" CRLF
"a=candidate:6 2 UDP 16515070 162.222.183.171 64941 typ relay raddr 162.222.183.171 rport 64941" CRLF
"a=candidate:6 1 UDP 16515071 162.222.183.171 64800 typ relay raddr 162.222.183.171 rport 64800" CRLF
"a=candidate:2 1 UDP 1694236671 24.6.134.204 59530 typ srflx raddr 10.0.0.36 rport 59530" CRLF
"a=candidate:3 1 UDP 100401151 162.222.183.171 62935 typ relay raddr 162.222.183.171 rport 62935" CRLF
"a=candidate:3 2 UDP 100401150 162.222.183.171 61026 typ relay raddr 162.222.183.171 rport 61026" CRLF
"a=rtcp:61026" CRLF
"a=end-of-candidates" CRLF
"a=ssrc:1111 foo" CRLF
"a=ssrc:1111 foo:bar" CRLF
"a=imageattr:120 send * recv *" CRLF
"a=imageattr:121 send [x=640,y=480] recv [x=640,y=480]" CRLF
"a=simulcast:recv pt=120;121" CRLF
"a=rid:bar recv pt=96;max-width=800;max-height=600" CRLF
"m=audio 9 RTP/SAVPF 0" CRLF
"a=mid:third" CRLF
"a=rtpmap:0 PCMU/8000" CRLF
"a=ice-lite" CRLF
"a=ice-options:foo bar" CRLF
"a=msid:noappdata" CRLF
"a=bundle-only" CRLF;

TEST_P(NewSdpTest, BasicAudioVideoSdpParse) {
  ParseSdp(kBasicAudioVideoOffer);
}

TEST_P(NewSdpTest, CheckIceUfrag) {
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_TRUE(!!mSdp) << "Parse failed: " << GetParseErrors();
  ASSERT_TRUE(mSdp->GetAttributeList().HasAttribute(
        SdpAttribute::kIceUfragAttribute));
  auto ice_ufrag = mSdp->GetAttributeList().GetIceUfrag();
  ASSERT_EQ("4a799b2e", ice_ufrag) << "Wrong ice-ufrag value";

  ice_ufrag = mSdp->GetMediaSection(0)
      .GetAttributeList().GetIceUfrag();
  ASSERT_EQ("00000000", ice_ufrag) << "ice-ufrag isn't overridden";

  ice_ufrag = mSdp->GetMediaSection(1)
      .GetAttributeList().GetIceUfrag();
  ASSERT_EQ("4a799b2e", ice_ufrag) << "ice-ufrag isn't carried to m-section";
}

TEST_P(NewSdpTest, CheckIcePwd) {
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_TRUE(!!mSdp) << "Parse failed: " << GetParseErrors();
  ASSERT_TRUE(mSdp->GetAttributeList().HasAttribute(
        SdpAttribute::kIcePwdAttribute));
  auto ice_pwd = mSdp->GetAttributeList().GetIcePwd();
  ASSERT_EQ("e4cc12a910f106a0a744719425510e17", ice_pwd) << "Wrong ice-pwd value";

  ice_pwd = mSdp->GetMediaSection(0)
      .GetAttributeList().GetIcePwd();
  ASSERT_EQ("0000000000000000000000000000000", ice_pwd)
      << "ice-pwd isn't overridden";

  ice_pwd = mSdp->GetMediaSection(1)
      .GetAttributeList().GetIcePwd();
  ASSERT_EQ("e4cc12a910f106a0a744719425510e17", ice_pwd)
      << "ice-pwd isn't carried to m-section";
}

TEST_P(NewSdpTest, CheckIceOptions) {
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_TRUE(!!mSdp) << "Parse failed: " << GetParseErrors();
  ASSERT_TRUE(mSdp->GetAttributeList().HasAttribute(
        SdpAttribute::kIceOptionsAttribute));
  auto ice_options = mSdp->GetAttributeList().GetIceOptions();
  ASSERT_EQ(2U, ice_options.mValues.size()) << "Wrong ice-options size";
  ASSERT_EQ("trickle", ice_options.mValues[0]) << "Wrong ice-options value";
  ASSERT_EQ("foo", ice_options.mValues[1]) << "Wrong ice-options value";

  ASSERT_TRUE(mSdp->GetMediaSection(2).GetAttributeList().HasAttribute(
        SdpAttribute::kIceOptionsAttribute));
  auto ice_options_media_level =
    mSdp->GetMediaSection(2).GetAttributeList().GetIceOptions();
  ASSERT_EQ(2U, ice_options_media_level.mValues.size()) << "Wrong ice-options size";
  ASSERT_EQ("foo", ice_options_media_level.mValues[0]) << "Wrong ice-options value";
  ASSERT_EQ("bar", ice_options_media_level.mValues[1]) << "Wrong ice-options value";
}

TEST_P(NewSdpTest, CheckFingerprint) {
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_TRUE(!!mSdp) << "Parse failed: " << GetParseErrors();
  ASSERT_TRUE(mSdp->GetAttributeList().HasAttribute(
        SdpAttribute::kFingerprintAttribute));
  auto fingerprints = mSdp->GetAttributeList().GetFingerprint();
  ASSERT_EQ(1U, fingerprints.mFingerprints.size());
  ASSERT_EQ(SdpFingerprintAttributeList::kSha256,
      fingerprints.mFingerprints[0].hashFunc)
    << "Wrong hash function";
  ASSERT_EQ("DF:2E:AC:8A:FD:0A:8E:99:BF:5D:E8:3C:E7:FA:FB:08:"
            "3B:3C:54:1D:D7:D4:05:77:A0:72:9B:14:08:6D:0F:4C",
            SdpFingerprintAttributeList::FormatFingerprint(
                fingerprints.mFingerprints[0].fingerprint))
    << "Wrong fingerprint";
  ASSERT_EQ(0xdfU, fingerprints.mFingerprints[0].fingerprint[0])
      << "first fingerprint element is iffy";

  ASSERT_EQ(3U, mSdp->GetMediaSectionCount());

  // Fallback to session level
  ASSERT_TRUE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
        SdpAttribute::kFingerprintAttribute));
  fingerprints = mSdp->GetMediaSection(0).GetAttributeList().GetFingerprint();
  ASSERT_EQ(1U, fingerprints.mFingerprints.size());
  ASSERT_EQ(SdpFingerprintAttributeList::kSha256,
      fingerprints.mFingerprints[0].hashFunc)
    << "Wrong hash function";
  ASSERT_EQ("DF:2E:AC:8A:FD:0A:8E:99:BF:5D:E8:3C:E7:FA:FB:08:"
            "3B:3C:54:1D:D7:D4:05:77:A0:72:9B:14:08:6D:0F:4C",
            SdpFingerprintAttributeList::FormatFingerprint(
                fingerprints.mFingerprints[0].fingerprint))
    << "Wrong fingerprint";
  ASSERT_EQ(0xdfU, fingerprints.mFingerprints[0].fingerprint[0])
      << "first fingerprint element is iffy";

  // Media level
  ASSERT_TRUE(mSdp->GetMediaSection(1).GetAttributeList().HasAttribute(
        SdpAttribute::kFingerprintAttribute));
  fingerprints = mSdp->GetMediaSection(1).GetAttributeList().GetFingerprint();
  ASSERT_EQ(1U, fingerprints.mFingerprints.size());
  ASSERT_EQ(SdpFingerprintAttributeList::kSha1,
      fingerprints.mFingerprints[0].hashFunc)
    << "Wrong hash function";
  ASSERT_EQ("DF:FA:FB:08:3B:3C:54:1D:D7:D4:05:77:A0:72:9B:14:"
            "08:6D:0F:4C:2E:AC:8A:FD:0A:8E:99:BF:5D:E8:3C:E7",
            SdpFingerprintAttributeList::FormatFingerprint(
                fingerprints.mFingerprints[0].fingerprint))
    << "Wrong fingerprint";
  ASSERT_EQ(0xdfU, fingerprints.mFingerprints[0].fingerprint[0])
      << "first fingerprint element is iffy";
}

TEST_P(NewSdpTest, CheckIdentity) {
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_TRUE(!!mSdp) << "Parse failed: " << GetParseErrors();
  ASSERT_TRUE(mSdp->GetAttributeList().HasAttribute(
        SdpAttribute::kIdentityAttribute));
  auto identity = mSdp->GetAttributeList().GetIdentity();
  ASSERT_EQ(LONG_IDENTITY, identity) << "Wrong identity assertion";
}

TEST_P(NewSdpTest, CheckNumberOfMediaSections) {
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_TRUE(!!mSdp) << "Parse failed: " << GetParseErrors();
  ASSERT_EQ(3U, mSdp->GetMediaSectionCount()) << "Wrong number of media sections";
}

TEST_P(NewSdpTest, CheckMlines) {
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_TRUE(!!mSdp) << "Parse failed: " << GetParseErrors();
  ASSERT_EQ(3U, mSdp->GetMediaSectionCount()) << "Wrong number of media sections";
  ASSERT_EQ(SdpMediaSection::kAudio, mSdp->GetMediaSection(0).GetMediaType())
    << "Wrong type for first media section";
  ASSERT_EQ(SdpMediaSection::kRtpSavpf,
            mSdp->GetMediaSection(0).GetProtocol())
    << "Wrong protocol for audio";
  auto audio_formats = mSdp->GetMediaSection(0).GetFormats();
  ASSERT_EQ(5U, audio_formats.size()) << "Wrong number of formats for audio";
  ASSERT_EQ("109", audio_formats[0]);
  ASSERT_EQ("9",   audio_formats[1]);
  ASSERT_EQ("0",   audio_formats[2]);
  ASSERT_EQ("8",   audio_formats[3]);
  ASSERT_EQ("101", audio_formats[4]);

  ASSERT_EQ(SdpMediaSection::kVideo, mSdp->GetMediaSection(1).GetMediaType())
    << "Wrong type for second media section";
  ASSERT_EQ(SdpMediaSection::kRtpSavpf,
            mSdp->GetMediaSection(1).GetProtocol())
    << "Wrong protocol for video";
  auto video_formats = mSdp->GetMediaSection(1).GetFormats();
  ASSERT_EQ(2U, video_formats.size()) << "Wrong number of formats for video";
  ASSERT_EQ("120", video_formats[0]);
  ASSERT_EQ("121", video_formats[1]);

  ASSERT_EQ(SdpMediaSection::kAudio, mSdp->GetMediaSection(2).GetMediaType())
    << "Wrong type for third media section";
}

TEST_P(NewSdpTest, CheckSetup) {
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_TRUE(!!mSdp) << "Parse failed: " << GetParseErrors();
  ASSERT_EQ(3U, mSdp->GetMediaSectionCount()) << "Wrong number of media sections";

  ASSERT_TRUE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
      SdpAttribute::kSetupAttribute));
  ASSERT_EQ(SdpSetupAttribute::kActpass,
      mSdp->GetMediaSection(0).GetAttributeList().GetSetup().mRole);
  ASSERT_TRUE(mSdp->GetMediaSection(1).GetAttributeList().HasAttribute(
      SdpAttribute::kSetupAttribute));
  ASSERT_EQ(SdpSetupAttribute::kActive,
      mSdp->GetMediaSection(1).GetAttributeList().GetSetup().mRole);
  ASSERT_FALSE(mSdp->GetMediaSection(2).GetAttributeList().HasAttribute(
        SdpAttribute::kSetupAttribute));
}

TEST_P(NewSdpTest, CheckSsrc)
{
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_TRUE(!!mSdp) << "Parse failed: " << GetParseErrors();
  ASSERT_EQ(3U, mSdp->GetMediaSectionCount()) << "Wrong number of media sections";

  ASSERT_TRUE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
      SdpAttribute::kSsrcAttribute));
  auto ssrcs = mSdp->GetMediaSection(0).GetAttributeList().GetSsrc().mSsrcs;
  ASSERT_EQ(1U, ssrcs.size());
  ASSERT_EQ(5150U, ssrcs[0].ssrc);
  ASSERT_EQ("", ssrcs[0].attribute);

  ASSERT_TRUE(mSdp->GetMediaSection(1).GetAttributeList().HasAttribute(
      SdpAttribute::kSsrcAttribute));
  ssrcs = mSdp->GetMediaSection(1).GetAttributeList().GetSsrc().mSsrcs;
  ASSERT_EQ(2U, ssrcs.size());
  ASSERT_EQ(1111U, ssrcs[0].ssrc);
  ASSERT_EQ("foo", ssrcs[0].attribute);
  ASSERT_EQ(1111U, ssrcs[1].ssrc);
  ASSERT_EQ("foo:bar", ssrcs[1].attribute);
}

TEST_P(NewSdpTest, CheckRtpmap) {
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_TRUE(!!mSdp) << "Parse failed: " << GetParseErrors();
  ASSERT_EQ(3U, mSdp->GetMediaSectionCount())
    << "Wrong number of media sections";

  const SdpMediaSection& audiosec = mSdp->GetMediaSection(0);
  const SdpRtpmapAttributeList& rtpmap = audiosec.GetAttributeList().GetRtpmap();
  ASSERT_EQ(5U, rtpmap.mRtpmaps.size())
    << "Wrong number of rtpmap attributes for audio";

  // Need to know name of type
  CheckRtpmap("109",
              SdpRtpmapAttributeList::kOpus,
              "opus",
              48000,
              2,
              audiosec.GetFormats()[0],
              rtpmap);

  CheckRtpmap("9",
              SdpRtpmapAttributeList::kG722,
              "G722",
              8000,
              1,
              audiosec.GetFormats()[1],
              rtpmap);

  CheckRtpmap("0",
              SdpRtpmapAttributeList::kPCMU,
              "PCMU",
              8000,
              1,
              audiosec.GetFormats()[2],
              rtpmap);

  CheckRtpmap("8",
              SdpRtpmapAttributeList::kPCMA,
              "PCMA",
              8000,
              1,
              audiosec.GetFormats()[3],
              rtpmap);

  CheckRtpmap("101",
              SdpRtpmapAttributeList::kOtherCodec,
              "telephone-event",
              8000,
              1,
              audiosec.GetFormats()[4],
              rtpmap);

  const SdpMediaSection& videosec1 = mSdp->GetMediaSection(1);
  CheckRtpmap("120",
              SdpRtpmapAttributeList::kVP8,
              "VP8",
              90000,
              0,
              videosec1.GetFormats()[0],
              videosec1.GetAttributeList().GetRtpmap());

  const SdpMediaSection& videosec2 = mSdp->GetMediaSection(1);
  CheckRtpmap("121",
              SdpRtpmapAttributeList::kVP9,
              "VP9",
              90000,
              0,
              videosec2.GetFormats()[1],
              videosec2.GetAttributeList().GetRtpmap());
}

const std::string kH264AudioVideoOffer =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=ice-ufrag:4a799b2e" CRLF
"a=ice-pwd:e4cc12a910f106a0a744719425510e17" CRLF
"a=ice-lite" CRLF
"a=msid-semantic:WMS stream streama" CRLF
"a=fingerprint:sha-256 DF:2E:AC:8A:FD:0A:8E:99:BF:5D:E8:3C:E7:FA:FB:08:3B:3C:54:1D:D7:D4:05:77:A0:72:9B:14:08:6D:0F:4C" CRLF
"a=group:BUNDLE first second" CRLF
"a=group:BUNDLE third" CRLF
"a=group:LS first third" CRLF
"m=audio 9 RTP/SAVPF 109 9 0 8 101" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=mid:first" CRLF
"a=rtpmap:109 opus/48000/2" CRLF
"a=ptime:20" CRLF
"a=maxptime:20" CRLF
"a=rtpmap:9 G722/8000" CRLF
"a=rtpmap:0 PCMU/8000" CRLF
"a=rtpmap:8 PCMA/8000" CRLF
"a=rtpmap:101 telephone-event/8000" CRLF
"a=fmtp:109 maxplaybackrate=32000;stereo=1" CRLF
"a=ice-ufrag:00000000" CRLF
"a=ice-pwd:0000000000000000000000000000000" CRLF
"a=sendonly" CRLF
"a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level" CRLF
"a=setup:actpass" CRLF
"a=rtcp-mux" CRLF
"a=msid:stream track" CRLF
"a=candidate:0 1 UDP 2130379007 10.0.0.36 62453 typ host" CRLF
"a=candidate:2 1 UDP 1694236671 24.6.134.204 62453 typ srflx raddr 10.0.0.36 rport 62453" CRLF
"a=candidate:3 1 UDP 100401151 162.222.183.171 49761 typ relay raddr 162.222.183.171 rport 49761" CRLF
"a=candidate:6 1 UDP 16515071 162.222.183.171 51858 typ relay raddr 162.222.183.171 rport 51858" CRLF
"a=candidate:3 2 UDP 100401150 162.222.183.171 62454 typ relay raddr 162.222.183.171 rport 62454" CRLF
"a=candidate:2 2 UDP 1694236670 24.6.134.204 55428 typ srflx raddr 10.0.0.36 rport 55428" CRLF
"a=candidate:6 2 UDP 16515070 162.222.183.171 50340 typ relay raddr 162.222.183.171 rport 50340" CRLF
"a=candidate:0 2 UDP 2130379006 10.0.0.36 55428 typ host" CRLF
"m=video 9 RTP/SAVPF 97 98 120" CRLF
"c=IN IP6 ::1" CRLF
"a=mid:second" CRLF
"a=rtpmap:97 H264/90000" CRLF
"a=fmtp:97 profile-level-id=42a01e" CRLF
"a=rtpmap:98 H264/90000" CRLF
"a=fmtp:98 PROFILE=0;LEVEL=0;profile-level-id=42a00d;packetization-mode=1;level-asymmetry-allowed=1;max-mbps=42000;max-fs=1400;max-cpb=1000;max-dpb=1000;max-br=180000;parameter-add=1;usedtx=0;stereo=0;useinbandfec=0;cbr=0" CRLF
"a=rtpmap:120 VP8/90000" CRLF
"a=fmtp:120 max-fs=3601;max-fr=31" CRLF
"a=recvonly" CRLF
"a=setup:active" CRLF
"a=rtcp-mux" CRLF
"a=msid:streama tracka" CRLF
"a=msid:streamb trackb" CRLF
"a=candidate:0 1 UDP 2130379007 10.0.0.36 59530 typ host" CRLF
"a=candidate:0 2 UDP 2130379006 10.0.0.36 64378 typ host" CRLF
"a=candidate:2 2 UDP 1694236670 24.6.134.204 64378 typ srflx raddr 10.0.0.36 rport 64378" CRLF
"a=candidate:6 2 UDP 16515070 162.222.183.171 64941 typ relay raddr 162.222.183.171 rport 64941" CRLF
"a=candidate:6 1 UDP 16515071 162.222.183.171 64800 typ relay raddr 162.222.183.171 rport 64800" CRLF
"a=candidate:2 1 UDP 1694236671 24.6.134.204 59530 typ srflx raddr 10.0.0.36 rport 59530" CRLF
"a=candidate:3 1 UDP 100401151 162.222.183.171 62935 typ relay raddr 162.222.183.171 rport 62935" CRLF
"a=candidate:3 2 UDP 100401150 162.222.183.171 61026 typ relay raddr 162.222.183.171 rport 61026" CRLF
"m=audio 9 RTP/SAVPF 0" CRLF
"a=mid:third" CRLF
"a=rtpmap:0 PCMU/8000" CRLF
"a=ice-lite" CRLF
"a=msid:noappdata" CRLF;

TEST_P(NewSdpTest, CheckFormatParameters) {
  ParseSdp(kH264AudioVideoOffer);
  ASSERT_TRUE(!!mSdp) << "Parse failed: " << GetParseErrors();
  ASSERT_EQ(3U, mSdp->GetMediaSectionCount())
    << "Wrong number of media sections";

  ASSERT_TRUE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
      SdpAttribute::kFmtpAttribute));
  auto audio_format_params =
      mSdp->GetMediaSection(0).GetAttributeList().GetFmtp().mFmtps;
  ASSERT_EQ(1U, audio_format_params.size());
  ASSERT_EQ("109", audio_format_params[0].format);
  ASSERT_TRUE(!!audio_format_params[0].parameters);
  const SdpFmtpAttributeList::OpusParameters* opus_parameters =
    static_cast<SdpFmtpAttributeList::OpusParameters*>(
        audio_format_params[0].parameters.get());
  ASSERT_EQ(32000U, opus_parameters->maxplaybackrate);
  ASSERT_EQ(1U, opus_parameters->stereo);

  ASSERT_TRUE(mSdp->GetMediaSection(1).GetAttributeList().HasAttribute(
      SdpAttribute::kFmtpAttribute));
  auto video_format_params =
      mSdp->GetMediaSection(1).GetAttributeList().GetFmtp().mFmtps;
  ASSERT_EQ(3U, video_format_params.size());
  ASSERT_EQ("97", video_format_params[0].format);
  ASSERT_TRUE(!!video_format_params[0].parameters);
  ASSERT_EQ(SdpRtpmapAttributeList::kH264,
            video_format_params[0].parameters->codec_type);
  const SdpFmtpAttributeList::H264Parameters *h264_parameters(
      static_cast<SdpFmtpAttributeList::H264Parameters*>(
        video_format_params[0].parameters.get()));
  ASSERT_EQ((uint32_t)0x42a01e, h264_parameters->profile_level_id);
  ASSERT_EQ(0U, h264_parameters->packetization_mode);
  ASSERT_FALSE(static_cast<bool>(h264_parameters->level_asymmetry_allowed));
  ASSERT_EQ(0U, h264_parameters->max_mbps);
  ASSERT_EQ(0U, h264_parameters->max_fs);
  ASSERT_EQ(0U, h264_parameters->max_cpb);
  ASSERT_EQ(0U, h264_parameters->max_dpb);
  ASSERT_EQ(0U, h264_parameters->max_br);

  ASSERT_EQ("98", video_format_params[1].format);
  ASSERT_TRUE(!!video_format_params[1].parameters);
  ASSERT_EQ(SdpRtpmapAttributeList::kH264,
            video_format_params[1].parameters->codec_type);
  h264_parameters =
      static_cast<SdpFmtpAttributeList::H264Parameters*>(
        video_format_params[1].parameters.get());
  ASSERT_EQ((uint32_t)0x42a00d, h264_parameters->profile_level_id);
  ASSERT_EQ(1U, h264_parameters->packetization_mode);
  ASSERT_TRUE(static_cast<bool>(h264_parameters->level_asymmetry_allowed));
  ASSERT_EQ(42000U, h264_parameters->max_mbps);
  ASSERT_EQ(1400U, h264_parameters->max_fs);
  ASSERT_EQ(1000U, h264_parameters->max_cpb);
  ASSERT_EQ(1000U, h264_parameters->max_dpb);
  ASSERT_EQ(180000U, h264_parameters->max_br);

  ASSERT_EQ("120", video_format_params[2].format);
  ASSERT_TRUE(!!video_format_params[2].parameters);
  ASSERT_EQ(SdpRtpmapAttributeList::kVP8,
            video_format_params[2].parameters->codec_type);
  const SdpFmtpAttributeList::VP8Parameters *vp8_parameters =
      static_cast<SdpFmtpAttributeList::VP8Parameters*>(
        video_format_params[2].parameters.get());
  ASSERT_EQ(3601U, vp8_parameters->max_fs);
  ASSERT_EQ(31U, vp8_parameters->max_fr);

  ASSERT_FALSE(mSdp->GetMediaSection(2).GetAttributeList().HasAttribute(
      SdpAttribute::kFmtpAttribute));
}

TEST_P(NewSdpTest, CheckPtime) {
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_EQ(20U, mSdp->GetMediaSection(0).GetAttributeList().GetPtime());
  ASSERT_FALSE(mSdp->GetMediaSection(1).GetAttributeList().HasAttribute(
      SdpAttribute::kPtimeAttribute));
}

TEST_P(NewSdpTest, CheckFlags) {
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_TRUE(mSdp->GetAttributeList().HasAttribute(
      SdpAttribute::kIceLiteAttribute));
  ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
      SdpAttribute::kIceLiteAttribute));
  ASSERT_FALSE(mSdp->GetMediaSection(1).GetAttributeList().HasAttribute(
      SdpAttribute::kIceLiteAttribute));
  ASSERT_FALSE(mSdp->GetMediaSection(2).GetAttributeList().HasAttribute(
      SdpAttribute::kIceLiteAttribute));

  ASSERT_TRUE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
      SdpAttribute::kRtcpMuxAttribute));
  ASSERT_FALSE(mSdp->GetMediaSection(2).GetAttributeList().HasAttribute(
      SdpAttribute::kRtcpMuxAttribute));

  ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
      SdpAttribute::kBundleOnlyAttribute));
  ASSERT_TRUE(mSdp->GetMediaSection(2).GetAttributeList().HasAttribute(
      SdpAttribute::kBundleOnlyAttribute));

  ASSERT_TRUE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
      SdpAttribute::kEndOfCandidatesAttribute));
  ASSERT_TRUE(mSdp->GetMediaSection(1).GetAttributeList().HasAttribute(
      SdpAttribute::kEndOfCandidatesAttribute));
  ASSERT_FALSE(mSdp->GetMediaSection(2).GetAttributeList().HasAttribute(
      SdpAttribute::kEndOfCandidatesAttribute));
}

TEST_P(NewSdpTest, CheckConnectionLines) {
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_TRUE(!!mSdp) << "Parse failed: " << GetParseErrors();
  ASSERT_EQ(3U, mSdp->GetMediaSectionCount())
    << "Wrong number of media sections";

  const SdpConnection& conn1 = mSdp->GetMediaSection(0).GetConnection();
  ASSERT_EQ(sdp::kIPv4, conn1.GetAddrType());
  ASSERT_EQ("0.0.0.0", conn1.GetAddress());
  ASSERT_EQ(0U, conn1.GetTtl());
  ASSERT_EQ(0U, conn1.GetCount());

  const SdpConnection& conn2 = mSdp->GetMediaSection(1).GetConnection();
  ASSERT_EQ(sdp::kIPv6, conn2.GetAddrType());
  ASSERT_EQ("::1", conn2.GetAddress());
  ASSERT_EQ(0U, conn2.GetTtl());
  ASSERT_EQ(0U, conn2.GetCount());

  // tests that we can fall through to session level as appropriate
  const SdpConnection& conn3 = mSdp->GetMediaSection(2).GetConnection();
  ASSERT_EQ(sdp::kIPv4, conn3.GetAddrType());
  ASSERT_EQ("224.0.0.1", conn3.GetAddress());
  ASSERT_EQ(100U, conn3.GetTtl());
  ASSERT_EQ(12U, conn3.GetCount());
}

TEST_P(NewSdpTest, CheckDirections) {
  ParseSdp(kBasicAudioVideoOffer);

  ASSERT_TRUE(!!mSdp) << "Parse failed: " << GetParseErrors();
  ASSERT_EQ(SdpDirectionAttribute::kSendonly,
            mSdp->GetMediaSection(0).GetAttributeList().GetDirection());
  ASSERT_EQ(SdpDirectionAttribute::kRecvonly,
            mSdp->GetMediaSection(1).GetAttributeList().GetDirection());
  ASSERT_EQ(SdpDirectionAttribute::kSendrecv,
            mSdp->GetMediaSection(2).GetAttributeList().GetDirection());
}

TEST_P(NewSdpTest, CheckCandidates) {
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_TRUE(!!mSdp) << "Parse failed: " << GetParseErrors();
  ASSERT_EQ(3U, mSdp->GetMediaSectionCount()) << "Wrong number of media sections";

  ASSERT_TRUE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
      SdpAttribute::kCandidateAttribute));
  auto audio_candidates =
      mSdp->GetMediaSection(0).GetAttributeList().GetCandidate();
  ASSERT_EQ(8U, audio_candidates.size());
  ASSERT_EQ("0 1 UDP 2130379007 10.0.0.36 62453 typ host", audio_candidates[0]);
  ASSERT_EQ("2 1 UDP 1694236671 24.6.134.204 62453 typ srflx raddr 10.0.0.36 rport 62453", audio_candidates[1]);
  ASSERT_EQ("3 1 UDP 100401151 162.222.183.171 49761 typ relay raddr 162.222.183.171 rport 49761", audio_candidates[2]);
  ASSERT_EQ("6 1 UDP 16515071 162.222.183.171 51858 typ relay raddr 162.222.183.171 rport 51858", audio_candidates[3]);
  ASSERT_EQ("3 2 UDP 100401150 162.222.183.171 62454 typ relay raddr 162.222.183.171 rport 62454", audio_candidates[4]);
  ASSERT_EQ("2 2 UDP 1694236670 24.6.134.204 55428 typ srflx raddr 10.0.0.36 rport 55428", audio_candidates[5]);
  ASSERT_EQ("6 2 UDP 16515070 162.222.183.171 50340 typ relay raddr 162.222.183.171 rport 50340", audio_candidates[6]);
  ASSERT_EQ("0 2 UDP 2130379006 10.0.0.36 55428 typ host", audio_candidates[7]);

  ASSERT_TRUE(mSdp->GetMediaSection(1).GetAttributeList().HasAttribute(
      SdpAttribute::kCandidateAttribute));
  auto video_candidates =
      mSdp->GetMediaSection(1).GetAttributeList().GetCandidate();
  ASSERT_EQ(8U, video_candidates.size());
  ASSERT_EQ("0 1 UDP 2130379007 10.0.0.36 59530 typ host", video_candidates[0]);
  ASSERT_EQ("0 2 UDP 2130379006 10.0.0.36 64378 typ host", video_candidates[1]);
  ASSERT_EQ("2 2 UDP 1694236670 24.6.134.204 64378 typ srflx raddr 10.0.0.36 rport 64378", video_candidates[2]);
  ASSERT_EQ("6 2 UDP 16515070 162.222.183.171 64941 typ relay raddr 162.222.183.171 rport 64941", video_candidates[3]);
  ASSERT_EQ("6 1 UDP 16515071 162.222.183.171 64800 typ relay raddr 162.222.183.171 rport 64800", video_candidates[4]);
  ASSERT_EQ("2 1 UDP 1694236671 24.6.134.204 59530 typ srflx raddr 10.0.0.36 rport 59530", video_candidates[5]);
  ASSERT_EQ("3 1 UDP 100401151 162.222.183.171 62935 typ relay raddr 162.222.183.171 rport 62935", video_candidates[6]);
  ASSERT_EQ("3 2 UDP 100401150 162.222.183.171 61026 typ relay raddr 162.222.183.171 rport 61026", video_candidates[7]);

  ASSERT_FALSE(mSdp->GetMediaSection(2).GetAttributeList().HasAttribute(
      SdpAttribute::kCandidateAttribute));
}

TEST_P(NewSdpTest, CheckMid) {
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_EQ("first", mSdp->GetMediaSection(0).GetAttributeList().GetMid());
  ASSERT_EQ("second", mSdp->GetMediaSection(1).GetAttributeList().GetMid());
  ASSERT_EQ("third", mSdp->GetMediaSection(2).GetAttributeList().GetMid());
}

TEST_P(NewSdpTest, CheckMsid) {
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_TRUE(mSdp->GetAttributeList().HasAttribute(
      SdpAttribute::kMsidSemanticAttribute));
  auto semantics = mSdp->GetAttributeList().GetMsidSemantic().mMsidSemantics;
  ASSERT_EQ(2U, semantics.size());
  ASSERT_EQ("WMS", semantics[0].semantic);
  ASSERT_EQ(2U, semantics[0].msids.size());
  ASSERT_EQ("stream", semantics[0].msids[0]);
  ASSERT_EQ("streama", semantics[0].msids[1]);
  ASSERT_EQ("foo", semantics[1].semantic);
  ASSERT_EQ(1U, semantics[1].msids.size());
  ASSERT_EQ("stream", semantics[1].msids[0]);


  const SdpMsidAttributeList& msids1 =
      mSdp->GetMediaSection(0).GetAttributeList().GetMsid();
  ASSERT_EQ(1U, msids1.mMsids.size());
  ASSERT_EQ("stream", msids1.mMsids[0].identifier);
  ASSERT_EQ("track", msids1.mMsids[0].appdata);
  const SdpMsidAttributeList& msids2 =
      mSdp->GetMediaSection(1).GetAttributeList().GetMsid();
  ASSERT_EQ(2U, msids2.mMsids.size());
  ASSERT_EQ("streama", msids2.mMsids[0].identifier);
  ASSERT_EQ("tracka", msids2.mMsids[0].appdata);
  ASSERT_EQ("streamb", msids2.mMsids[1].identifier);
  ASSERT_EQ("trackb", msids2.mMsids[1].appdata);
  const SdpMsidAttributeList& msids3 =
      mSdp->GetMediaSection(2).GetAttributeList().GetMsid();
  ASSERT_EQ(1U, msids3.mMsids.size());
  ASSERT_EQ("noappdata", msids3.mMsids[0].identifier);
  ASSERT_EQ("", msids3.mMsids[0].appdata);
}

TEST_P(NewSdpTest, CheckRid)
{
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_TRUE(!!mSdp);
  ASSERT_EQ(3U, mSdp->GetMediaSectionCount()) << "Wrong number of media sections";

  ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
        SdpAttribute::kRidAttribute));
  ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
        SdpAttribute::kRidAttribute));
  ASSERT_TRUE(mSdp->GetMediaSection(1).GetAttributeList().HasAttribute(
        SdpAttribute::kRidAttribute));
  ASSERT_FALSE(mSdp->GetMediaSection(2).GetAttributeList().HasAttribute(
        SdpAttribute::kRidAttribute));

  const SdpRidAttributeList& rids =
    mSdp->GetMediaSection(1).GetAttributeList().GetRid();

  ASSERT_EQ(1U, rids.mRids.size());
  ASSERT_EQ("bar", rids.mRids[0].id);
  ASSERT_EQ(sdp::kRecv, rids.mRids[0].direction);
  ASSERT_EQ(1U, rids.mRids[0].formats.size());
  ASSERT_EQ(96U, rids.mRids[0].formats[0]);
  ASSERT_EQ(800U, rids.mRids[0].constraints.maxWidth);
  ASSERT_EQ(600U, rids.mRids[0].constraints.maxHeight);
}

TEST_P(NewSdpTest, CheckMediaLevelIceUfrag) {
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_TRUE(!!mSdp) << "Parse failed: " << GetParseErrors();
  ASSERT_EQ(3U, mSdp->GetMediaSectionCount()) << "Wrong number of media sections";

  ASSERT_TRUE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
        SdpAttribute::kIceUfragAttribute, true));
  ASSERT_EQ("00000000",
            mSdp->GetMediaSection(0).GetAttributeList().GetIceUfrag());

  ASSERT_TRUE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
        SdpAttribute::kIceUfragAttribute, false));

  ASSERT_TRUE(mSdp->GetMediaSection(1).GetAttributeList().HasAttribute(
        SdpAttribute::kIceUfragAttribute, true));
  ASSERT_EQ("4a799b2e",
            mSdp->GetMediaSection(1).GetAttributeList().GetIceUfrag());
}

TEST_P(NewSdpTest, CheckMediaLevelIcePwd) {
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_TRUE(!!mSdp) << "Parse failed: " << GetParseErrors();
  ASSERT_EQ(3U, mSdp->GetMediaSectionCount()) << "Wrong number of media sections";

  ASSERT_TRUE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
        SdpAttribute::kIcePwdAttribute));
  ASSERT_EQ("0000000000000000000000000000000",
            mSdp->GetMediaSection(0).GetAttributeList().GetIcePwd());

  ASSERT_TRUE(mSdp->GetMediaSection(1).GetAttributeList().HasAttribute(
        SdpAttribute::kIcePwdAttribute));
  ASSERT_EQ("e4cc12a910f106a0a744719425510e17",
            mSdp->GetMediaSection(1).GetAttributeList().GetIcePwd());
}

TEST_P(NewSdpTest, CheckGroups) {
  ParseSdp(kBasicAudioVideoOffer);
  const SdpGroupAttributeList& group = mSdp->GetAttributeList().GetGroup();
  const SdpGroupAttributeList::Group& group1 = group.mGroups[0];
  ASSERT_EQ(SdpGroupAttributeList::kBundle, group1.semantics);
  ASSERT_EQ(2U, group1.tags.size());
  ASSERT_EQ("first", group1.tags[0]);
  ASSERT_EQ("second", group1.tags[1]);

  const SdpGroupAttributeList::Group& group2 = group.mGroups[1];
  ASSERT_EQ(SdpGroupAttributeList::kBundle, group2.semantics);
  ASSERT_EQ(1U, group2.tags.size());
  ASSERT_EQ("third", group2.tags[0]);

  const SdpGroupAttributeList::Group& group3 = group.mGroups[2];
  ASSERT_EQ(SdpGroupAttributeList::kLs, group3.semantics);
  ASSERT_EQ(2U, group3.tags.size());
  ASSERT_EQ("first", group3.tags[0]);
  ASSERT_EQ("third", group3.tags[1]);
}

// SDP from a basic A/V call with data channel FFX/FFX
const std::string kBasicAudioVideoDataOffer =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 27987 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"t=0 0" CRLF
"a=ice-ufrag:8a39d2ae" CRLF
"a=ice-pwd:601d53aba51a318351b3ecf5ee00048f" CRLF
"a=fingerprint:sha-256 30:FF:8E:2B:AC:9D:ED:70:18:10:67:C8:AE:9E:68:F3:86:53:51:B0:AC:31:B7:BE:6D:CF:A4:2E:D3:6E:B4:28" CRLF
"m=audio 9 RTP/SAVPF 109 9 0 8 101" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:109 opus/48000/2" CRLF
"a=ptime:20" CRLF
"a=rtpmap:9 G722/8000" CRLF
"a=rtpmap:0 PCMU/8000" CRLF
"a=rtpmap:8 PCMA/8000" CRLF
"a=rtpmap:101 telephone-event/8000" CRLF
"a=fmtp:101 0-15" CRLF
"a=sendrecv" CRLF
"a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level" CRLF
"a=extmap:2/sendonly some_extension" CRLF
"a=extmap:3 some_other_extension some_params some more params" CRLF
"a=setup:actpass" CRLF
"a=rtcp-mux" CRLF
"m=video 9 RTP/SAVPF 120 126 97" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:120 VP8/90000" CRLF
"a=rtpmap:126 H264/90000" CRLF
"a=fmtp:126 profile-level-id=42e01f;packetization-mode=1" CRLF
"a=rtpmap:97 H264/90000" CRLF
"a=fmtp:97 profile-level-id=42e01f" CRLF
"a=sendrecv" CRLF
// sipcc barfs on this, despite that it is valid syntax
// Do we care about fixing?
//"a=rtcp-fb:120 ack" CRLF // Should be ignored by sipcc
"a=rtcp-fb:120 ack rpsi" CRLF
"a=rtcp-fb:120 ack app foo" CRLF
"a=rtcp-fb:120 ack foo" CRLF // Should be ignored
"a=rtcp-fb:120 nack" CRLF
"a=rtcp-fb:120 nack sli" CRLF
"a=rtcp-fb:120 nack pli" CRLF
"a=rtcp-fb:120 nack rpsi" CRLF
"a=rtcp-fb:120 nack app foo" CRLF
"a=rtcp-fb:120 nack foo" CRLF // Should be ignored
"a=rtcp-fb:120 ccm fir" CRLF
"a=rtcp-fb:120 ccm tmmbr" CRLF
"a=rtcp-fb:120 ccm tstr" CRLF
"a=rtcp-fb:120 ccm vbcm" CRLF
"a=rtcp-fb:120 ccm foo" CRLF // Should be ignored
"a=rtcp-fb:120 trr-int 10" CRLF
"a=rtcp-fb:120 foo" CRLF // Should be ignored
"a=rtcp-fb:126 nack" CRLF
"a=rtcp-fb:126 nack pli" CRLF
"a=rtcp-fb:126 ccm fir" CRLF
"a=rtcp-fb:97 nack" CRLF
"a=rtcp-fb:97 nack pli" CRLF
"a=rtcp-fb:97 ccm fir" CRLF
"a=rtcp-fb:* ccm tmmbr" CRLF
"a=setup:actpass" CRLF
"a=rtcp-mux" CRLF
"m=application 9 DTLS/SCTP 5000" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=sctpmap:5000 webrtc-datachannel 16" CRLF
"a=setup:actpass" CRLF;

TEST_P(NewSdpTest, BasicAudioVideoDataSdpParse) {
  ParseSdp(kBasicAudioVideoDataOffer);
  ASSERT_EQ(0U, mParser.GetParseErrors().size()) <<
    "Got parse errors: " << GetParseErrors();
}

TEST_P(NewSdpTest, CheckApplicationParameters) {
  ParseSdp(kBasicAudioVideoDataOffer);
  ASSERT_TRUE(!!mSdp);
  ASSERT_EQ(3U, mSdp->GetMediaSectionCount()) << "Wrong number of media sections";
  ASSERT_EQ(SdpMediaSection::kAudio, mSdp->GetMediaSection(0).GetMediaType())
    << "Wrong type for first media section";
  ASSERT_EQ(SdpMediaSection::kVideo, mSdp->GetMediaSection(1).GetMediaType())
    << "Wrong type for second media section";
  ASSERT_EQ(SdpMediaSection::kApplication, mSdp->GetMediaSection(2).GetMediaType())
    << "Wrong type for third media section";

  ASSERT_EQ(SdpMediaSection::kDtlsSctp,
            mSdp->GetMediaSection(2).GetProtocol())
    << "Wrong protocol for application";
  auto app_formats = mSdp->GetMediaSection(2).GetFormats();
  ASSERT_EQ(1U, app_formats.size()) << "Wrong number of formats for audio";
  ASSERT_EQ("5000", app_formats[0]);

  const SdpConnection& conn3 = mSdp->GetMediaSection(2).GetConnection();
  ASSERT_EQ(sdp::kIPv4, conn3.GetAddrType());
  ASSERT_EQ("0.0.0.0", conn3.GetAddress());
  ASSERT_EQ(0U, conn3.GetTtl());
  ASSERT_EQ(0U, conn3.GetCount());

  ASSERT_TRUE(mSdp->GetMediaSection(2).GetAttributeList().HasAttribute(
      SdpAttribute::kSetupAttribute));
  ASSERT_EQ(SdpSetupAttribute::kActpass,
      mSdp->GetMediaSection(2).GetAttributeList().GetSetup().mRole);
}

TEST_P(NewSdpTest, CheckExtmap) {
  ParseSdp(kBasicAudioVideoDataOffer);
  ASSERT_TRUE(!!mSdp);
  ASSERT_EQ(3U, mSdp->GetMediaSectionCount()) << "Wrong number of media sections";

  ASSERT_TRUE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
        SdpAttribute::kExtmapAttribute));

  auto extmaps =
    mSdp->GetMediaSection(0).GetAttributeList().GetExtmap().mExtmaps;
  ASSERT_EQ(3U, extmaps.size());

  ASSERT_EQ(1U, extmaps[0].entry);
  ASSERT_FALSE(extmaps[0].direction_specified);
  ASSERT_EQ("urn:ietf:params:rtp-hdrext:ssrc-audio-level",
      extmaps[0].extensionname);
  ASSERT_EQ("",
      extmaps[0].extensionattributes);

  ASSERT_EQ(2U, extmaps[1].entry);
  ASSERT_TRUE(extmaps[1].direction_specified);
  ASSERT_EQ(SdpDirectionAttribute::kSendonly, extmaps[1].direction);
  ASSERT_EQ("some_extension",
      extmaps[1].extensionname);
  ASSERT_EQ("",
      extmaps[1].extensionattributes);

  ASSERT_EQ(3U, extmaps[2].entry);
  ASSERT_FALSE(extmaps[2].direction_specified);
  ASSERT_EQ("some_other_extension",
      extmaps[2].extensionname);
  ASSERT_EQ("some_params some more params",
      extmaps[2].extensionattributes);
}

TEST_P(NewSdpTest, CheckRtcpFb) {
  ParseSdp(kBasicAudioVideoDataOffer);
  ASSERT_TRUE(!!mSdp);
  ASSERT_EQ(3U, mSdp->GetMediaSectionCount()) << "Wrong number of media sections";

  auto& video_attrs = mSdp->GetMediaSection(1).GetAttributeList();
  ASSERT_TRUE(video_attrs.HasAttribute(SdpAttribute::kRtcpFbAttribute));
  auto& rtcpfbs = video_attrs.GetRtcpFb().mFeedbacks;
  ASSERT_EQ(19U, rtcpfbs.size());
  CheckRtcpFb(rtcpfbs[0], "120", SdpRtcpFbAttributeList::kAck, "rpsi");
  CheckRtcpFb(rtcpfbs[1], "120", SdpRtcpFbAttributeList::kAck, "app", "foo");
  CheckRtcpFb(rtcpfbs[2], "120", SdpRtcpFbAttributeList::kNack, "");
  CheckRtcpFb(rtcpfbs[3], "120", SdpRtcpFbAttributeList::kNack, "sli");
  CheckRtcpFb(rtcpfbs[4], "120", SdpRtcpFbAttributeList::kNack, "pli");
  CheckRtcpFb(rtcpfbs[5], "120", SdpRtcpFbAttributeList::kNack, "rpsi");
  CheckRtcpFb(rtcpfbs[6], "120", SdpRtcpFbAttributeList::kNack, "app", "foo");
  CheckRtcpFb(rtcpfbs[7], "120", SdpRtcpFbAttributeList::kCcm, "fir");
  CheckRtcpFb(rtcpfbs[8], "120", SdpRtcpFbAttributeList::kCcm, "tmmbr");
  CheckRtcpFb(rtcpfbs[9], "120", SdpRtcpFbAttributeList::kCcm, "tstr");
  CheckRtcpFb(rtcpfbs[10], "120", SdpRtcpFbAttributeList::kCcm, "vbcm");
  CheckRtcpFb(rtcpfbs[11], "120", SdpRtcpFbAttributeList::kTrrInt, "10");
  CheckRtcpFb(rtcpfbs[12], "126", SdpRtcpFbAttributeList::kNack, "");
  CheckRtcpFb(rtcpfbs[13], "126", SdpRtcpFbAttributeList::kNack, "pli");
  CheckRtcpFb(rtcpfbs[14], "126", SdpRtcpFbAttributeList::kCcm, "fir");
  CheckRtcpFb(rtcpfbs[15], "97",  SdpRtcpFbAttributeList::kNack, "");
  CheckRtcpFb(rtcpfbs[16], "97",  SdpRtcpFbAttributeList::kNack, "pli");
  CheckRtcpFb(rtcpfbs[17], "97", SdpRtcpFbAttributeList::kCcm, "fir");
  CheckRtcpFb(rtcpfbs[18], "*", SdpRtcpFbAttributeList::kCcm, "tmmbr");
}

TEST_P(NewSdpTest, CheckRtcp) {
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_TRUE(!!mSdp);
  ASSERT_EQ(3U, mSdp->GetMediaSectionCount()) << "Wrong number of media sections";

  ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
        SdpAttribute::kRtcpAttribute));
  ASSERT_TRUE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
        SdpAttribute::kRtcpAttribute));
  ASSERT_TRUE(mSdp->GetMediaSection(1).GetAttributeList().HasAttribute(
        SdpAttribute::kRtcpAttribute));
  ASSERT_FALSE(mSdp->GetMediaSection(2).GetAttributeList().HasAttribute(
        SdpAttribute::kRtcpAttribute));

  auto& rtcpAttr_0 = mSdp->GetMediaSection(0).GetAttributeList().GetRtcp();
  ASSERT_EQ(62454U, rtcpAttr_0.mPort);
  ASSERT_EQ(sdp::kInternet, rtcpAttr_0.mNetType);
  ASSERT_EQ(sdp::kIPv4, rtcpAttr_0.mAddrType);
  ASSERT_EQ("162.222.183.171", rtcpAttr_0.mAddress);

  auto& rtcpAttr_1 = mSdp->GetMediaSection(1).GetAttributeList().GetRtcp();
  ASSERT_EQ(61026U, rtcpAttr_1.mPort);
  ASSERT_EQ("", rtcpAttr_1.mAddress);
}

TEST_P(NewSdpTest, CheckImageattr)
{
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_TRUE(!!mSdp);
  ASSERT_EQ(3U, mSdp->GetMediaSectionCount()) << "Wrong number of media sections";

  ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
        SdpAttribute::kImageattrAttribute));
  ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
        SdpAttribute::kImageattrAttribute));
  ASSERT_TRUE(mSdp->GetMediaSection(1).GetAttributeList().HasAttribute(
        SdpAttribute::kImageattrAttribute));
  ASSERT_FALSE(mSdp->GetMediaSection(2).GetAttributeList().HasAttribute(
        SdpAttribute::kImageattrAttribute));

  const SdpImageattrAttributeList& imageattrs =
    mSdp->GetMediaSection(1).GetAttributeList().GetImageattr();

  ASSERT_EQ(2U, imageattrs.mImageattrs.size());
  const SdpImageattrAttributeList::Imageattr& imageattr_0(
      imageattrs.mImageattrs[0]);
  ASSERT_TRUE(imageattr_0.pt.isSome());
  ASSERT_EQ(120U, *imageattr_0.pt);
  ASSERT_TRUE(imageattr_0.sendAll);
  ASSERT_TRUE(imageattr_0.recvAll);

  const SdpImageattrAttributeList::Imageattr& imageattr_1(
      imageattrs.mImageattrs[1]);
  ASSERT_TRUE(imageattr_1.pt.isSome());
  ASSERT_EQ(121U, *imageattr_1.pt);
  ASSERT_FALSE(imageattr_1.sendAll);
  ASSERT_FALSE(imageattr_1.recvAll);
  ASSERT_EQ(1U, imageattr_1.sendSets.size());
  ASSERT_EQ(1U, imageattr_1.sendSets[0].xRange.discreteValues.size());
  ASSERT_EQ(640U, imageattr_1.sendSets[0].xRange.discreteValues.front());
  ASSERT_EQ(1U, imageattr_1.sendSets[0].yRange.discreteValues.size());
  ASSERT_EQ(480U, imageattr_1.sendSets[0].yRange.discreteValues.front());
  ASSERT_EQ(1U, imageattr_1.recvSets.size());
  ASSERT_EQ(1U, imageattr_1.recvSets[0].xRange.discreteValues.size());
  ASSERT_EQ(640U, imageattr_1.recvSets[0].xRange.discreteValues.front());
  ASSERT_EQ(1U, imageattr_1.recvSets[0].yRange.discreteValues.size());
  ASSERT_EQ(480U, imageattr_1.recvSets[0].yRange.discreteValues.front());
}

TEST_P(NewSdpTest, CheckSimulcast)
{
  ParseSdp(kBasicAudioVideoOffer);
  ASSERT_TRUE(!!mSdp);
  ASSERT_EQ(3U, mSdp->GetMediaSectionCount()) << "Wrong number of media sections";

  ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
        SdpAttribute::kSimulcastAttribute));
  ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
        SdpAttribute::kSimulcastAttribute));
  ASSERT_TRUE(mSdp->GetMediaSection(1).GetAttributeList().HasAttribute(
        SdpAttribute::kSimulcastAttribute));
  ASSERT_FALSE(mSdp->GetMediaSection(2).GetAttributeList().HasAttribute(
        SdpAttribute::kSimulcastAttribute));

  const SdpSimulcastAttribute& simulcast =
    mSdp->GetMediaSection(1).GetAttributeList().GetSimulcast();

  ASSERT_EQ(2U, simulcast.recvVersions.size());
  ASSERT_EQ(0U, simulcast.sendVersions.size());
  ASSERT_EQ(1U, simulcast.recvVersions[0].choices.size());
  ASSERT_EQ("120", simulcast.recvVersions[0].choices[0]);
  ASSERT_EQ(1U, simulcast.recvVersions[1].choices.size());
  ASSERT_EQ("121", simulcast.recvVersions[1].choices[0]);
  ASSERT_EQ(SdpSimulcastAttribute::Versions::kPt,
            simulcast.recvVersions.type);
}

TEST_P(NewSdpTest, CheckSctpmap) {
  ParseSdp(kBasicAudioVideoDataOffer);
  ASSERT_TRUE(!!mSdp) << "Parse failed: " << GetParseErrors();
  ASSERT_EQ(3U, mSdp->GetMediaSectionCount())
    << "Wrong number of media sections";

  const SdpMediaSection& appsec = mSdp->GetMediaSection(2);
  ASSERT_TRUE(
      appsec.GetAttributeList().HasAttribute(SdpAttribute::kSctpmapAttribute));
  const SdpSctpmapAttributeList& sctpmap =
    appsec.GetAttributeList().GetSctpmap();

  ASSERT_EQ(1U, sctpmap.mSctpmaps.size())
    << "Wrong number of sctpmap attributes";
  ASSERT_EQ(1U, appsec.GetFormats().size());

  // Need to know name of type
  CheckSctpmap("5000",
              "webrtc-datachannel",
              16,
              appsec.GetFormats()[0],
              sctpmap);
}

const std::string kNewSctpmapOfferDraft07 =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 27987 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"t=0 0" CRLF
"a=ice-ufrag:8a39d2ae" CRLF
"a=ice-pwd:601d53aba51a318351b3ecf5ee00048f" CRLF
"a=fingerprint:sha-256 30:FF:8E:2B:AC:9D:ED:70:18:10:67:C8:AE:9E:68:F3:86:53:51:B0:AC:31:B7:BE:6D:CF:A4:2E:D3:6E:B4:28" CRLF
"m=application 9 DTLS/SCTP webrtc-datachannel" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=fmtp:webrtc-datachannel max-message-size=100000" CRLF
"a=sctp-port 5000" CRLF
"a=setup:actpass" CRLF;

TEST_P(NewSdpTest, NewSctpmapSdpParse) {
  ParseSdp(kNewSctpmapOfferDraft07, false);
}

INSTANTIATE_TEST_CASE_P(RoundTripSerialize,
                        NewSdpTest,
                        ::testing::Values(false, true));

const std::string kCandidateInSessionSDP =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=candidate:0 1 UDP 2130379007 10.0.0.36 62453 typ host" CRLF
"m=audio 9 RTP/SAVPF 109 9 0 8 101" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:109 opus/48000/2" CRLF;

// This may or may not parse, but if it does, the errant candidate attribute
// should be ignored.
TEST_P(NewSdpTest, CheckCandidateInSessionLevel) {
  ParseSdp(kCandidateInSessionSDP, false);
  if (mSdp) {
    ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
          SdpAttribute::kCandidateAttribute));
    ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
          SdpAttribute::kCandidateAttribute));
  }
}

const std::string kBundleOnlyInSessionSDP =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=bundle-only" CRLF
"m=audio 9 RTP/SAVPF 109 9 0 8 101" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:109 opus/48000/2" CRLF;

// This may or may not parse, but if it does, the errant attribute
// should be ignored.
TEST_P(NewSdpTest, CheckBundleOnlyInSessionLevel) {
  ParseSdp(kBundleOnlyInSessionSDP, false);
  if (mSdp) {
    ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
          SdpAttribute::kBundleOnlyAttribute));
    ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
          SdpAttribute::kBundleOnlyAttribute));
  }
}

const std::string kFmtpInSessionSDP =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=fmtp:109 0-15" CRLF
"m=audio 9 RTP/SAVPF 109 9 0 8 101" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:109 opus/48000/2" CRLF;

// This may or may not parse, but if it does, the errant attribute
// should be ignored.
TEST_P(NewSdpTest, CheckFmtpInSessionLevel) {
  ParseSdp(kFmtpInSessionSDP, false);
  if (mSdp) {
    ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
          SdpAttribute::kFmtpAttribute));
    ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
          SdpAttribute::kFmtpAttribute));
  }
}

const std::string kIceMismatchInSessionSDP =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=ice-mismatch" CRLF
"m=audio 9 RTP/SAVPF 109 9 0 8 101" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:109 opus/48000/2" CRLF;

// This may or may not parse, but if it does, the errant attribute
// should be ignored.
TEST_P(NewSdpTest, CheckIceMismatchInSessionLevel) {
  ParseSdp(kIceMismatchInSessionSDP, false);
  if (mSdp) {
    ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
          SdpAttribute::kIceMismatchAttribute));
    ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
          SdpAttribute::kIceMismatchAttribute));
  }
}

const std::string kImageattrInSessionSDP =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=imageattr:120 send * recv *" CRLF
"m=video 9 RTP/SAVPF 120" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:120 VP8/90000" CRLF;

// This may or may not parse, but if it does, the errant attribute
// should be ignored.
TEST_P(NewSdpTest, CheckImageattrInSessionLevel) {
  ParseSdp(kImageattrInSessionSDP, false);
  if (mSdp) {
    ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
          SdpAttribute::kImageattrAttribute));
    ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
          SdpAttribute::kImageattrAttribute));
  }
}

const std::string kLabelInSessionSDP =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=label:foobar" CRLF
"m=video 9 RTP/SAVPF 120" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:120 VP8/90000" CRLF;

// This may or may not parse, but if it does, the errant attribute
// should be ignored.
TEST_P(NewSdpTest, CheckLabelInSessionLevel) {
  ParseSdp(kLabelInSessionSDP, false);
  if (mSdp) {
    ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
          SdpAttribute::kLabelAttribute));
    ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
          SdpAttribute::kLabelAttribute));
  }
}

const std::string kMaxptimeInSessionSDP =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=maxptime:100" CRLF
"m=video 9 RTP/SAVPF 120" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:120 VP8/90000" CRLF;

// This may or may not parse, but if it does, the errant attribute
// should be ignored.
TEST_P(NewSdpTest, CheckMaxptimeInSessionLevel) {
  ParseSdp(kMaxptimeInSessionSDP, false);
  if (mSdp) {
    ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
          SdpAttribute::kMaxptimeAttribute));
    ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
          SdpAttribute::kMaxptimeAttribute));
  }
}

const std::string kMidInSessionSDP =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=mid:foobar" CRLF
"m=video 9 RTP/SAVPF 120" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:120 VP8/90000" CRLF;

// This may or may not parse, but if it does, the errant attribute
// should be ignored.
TEST_P(NewSdpTest, CheckMidInSessionLevel) {
  ParseSdp(kMidInSessionSDP, false);
  if (mSdp) {
    ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
          SdpAttribute::kMidAttribute));
    ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
          SdpAttribute::kMidAttribute));
  }
}

const std::string kMsidInSessionSDP =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=msid:foobar" CRLF
"m=video 9 RTP/SAVPF 120" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:120 VP8/90000" CRLF;

// This may or may not parse, but if it does, the errant attribute
// should be ignored.
TEST_P(NewSdpTest, CheckMsidInSessionLevel) {
  ParseSdp(kMsidInSessionSDP, false);
  if (mSdp) {
    ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
          SdpAttribute::kMsidAttribute));
    ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
          SdpAttribute::kMsidAttribute));
  }
}

const std::string kPtimeInSessionSDP =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=ptime:50" CRLF
"m=video 9 RTP/SAVPF 120" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:120 VP8/90000" CRLF;

// This may or may not parse, but if it does, the errant attribute
// should be ignored.
TEST_P(NewSdpTest, CheckPtimeInSessionLevel) {
  ParseSdp(kPtimeInSessionSDP, false);
  if (mSdp) {
    ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
          SdpAttribute::kPtimeAttribute));
    ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
          SdpAttribute::kPtimeAttribute));
  }
}

const std::string kRemoteCandidatesInSessionSDP =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=remote-candidates:0 10.0.0.1 5555" CRLF
"m=video 9 RTP/SAVPF 120" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:120 VP8/90000" CRLF;

// This may or may not parse, but if it does, the errant attribute
// should be ignored.
TEST_P(NewSdpTest, CheckRemoteCandidatesInSessionLevel) {
  ParseSdp(kRemoteCandidatesInSessionSDP, false);
  if (mSdp) {
    ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
          SdpAttribute::kRemoteCandidatesAttribute));
    ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
          SdpAttribute::kRemoteCandidatesAttribute));
  }
}

const std::string kRtcpInSessionSDP =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=rtcp:5555" CRLF
"m=video 9 RTP/SAVPF 120" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:120 VP8/90000" CRLF;

// This may or may not parse, but if it does, the errant attribute
// should be ignored.
TEST_P(NewSdpTest, CheckRtcpInSessionLevel) {
  ParseSdp(kRtcpInSessionSDP, false);
  if (mSdp) {
    ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
          SdpAttribute::kRtcpAttribute));
    ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
          SdpAttribute::kRtcpAttribute));
  }
}

const std::string kRtcpFbInSessionSDP =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=rtcp-fb:120 nack" CRLF
"m=video 9 RTP/SAVPF 120" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:120 VP8/90000" CRLF;

// This may or may not parse, but if it does, the errant attribute
// should be ignored.
TEST_P(NewSdpTest, CheckRtcpFbInSessionLevel) {
  ParseSdp(kRtcpFbInSessionSDP, false);
  if (mSdp) {
    ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
          SdpAttribute::kRtcpFbAttribute));
    ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
          SdpAttribute::kRtcpFbAttribute));
  }
}

const std::string kRtcpMuxInSessionSDP =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=rtcp-mux" CRLF
"m=video 9 RTP/SAVPF 120" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:120 VP8/90000" CRLF;

// This may or may not parse, but if it does, the errant attribute
// should be ignored.
TEST_P(NewSdpTest, CheckRtcpMuxInSessionLevel) {
  ParseSdp(kRtcpMuxInSessionSDP, false);
  if (mSdp) {
    ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
          SdpAttribute::kRtcpMuxAttribute));
    ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
          SdpAttribute::kRtcpMuxAttribute));
  }
}

const std::string kRtcpRsizeInSessionSDP =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=rtcp-rsize" CRLF
"m=video 9 RTP/SAVPF 120" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:120 VP8/90000" CRLF;

// This may or may not parse, but if it does, the errant attribute
// should be ignored.
TEST_P(NewSdpTest, CheckRtcpRsizeInSessionLevel) {
  ParseSdp(kRtcpRsizeInSessionSDP, false);
  if (mSdp) {
    ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
          SdpAttribute::kRtcpRsizeAttribute));
    ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
          SdpAttribute::kRtcpRsizeAttribute));
  }
}

const std::string kRtpmapInSessionSDP =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=rtpmap:120 VP8/90000" CRLF
"m=video 9 RTP/SAVPF 120" CRLF
"c=IN IP4 0.0.0.0" CRLF;

// This may or may not parse, but if it does, the errant attribute
// should be ignored.
TEST_P(NewSdpTest, CheckRtpmapInSessionLevel) {
  ParseSdp(kRtpmapInSessionSDP, false);
  if (mSdp) {
    ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
          SdpAttribute::kRtpmapAttribute));
    ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
          SdpAttribute::kRtpmapAttribute));
  }
}

const std::string kSctpmapInSessionSDP =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=sctpmap:5000" CRLF
"m=video 9 RTP/SAVPF 120" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:120 VP8/90000" CRLF;

// This may or may not parse, but if it does, the errant attribute
// should be ignored.
TEST_P(NewSdpTest, CheckSctpmapInSessionLevel) {
  ParseSdp(kSctpmapInSessionSDP, false);
  if (mSdp) {
    ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
          SdpAttribute::kSctpmapAttribute));
    ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
          SdpAttribute::kSctpmapAttribute));
  }
}

const std::string kSsrcInSessionSDP =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=ssrc:5000" CRLF
"m=video 9 RTP/SAVPF 120" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:120 VP8/90000" CRLF;

// This may or may not parse, but if it does, the errant attribute
// should be ignored.
TEST_P(NewSdpTest, CheckSsrcInSessionLevel) {
  ParseSdp(kSsrcInSessionSDP, false);
  if (mSdp) {
    ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
          SdpAttribute::kSsrcAttribute));
    ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
          SdpAttribute::kSsrcAttribute));
  }
}

const std::string kSsrcGroupInSessionSDP =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"a=ssrc-group:FID 5000" CRLF
"m=video 9 RTP/SAVPF 120" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:120 VP8/90000" CRLF;

// This may or may not parse, but if it does, the errant attribute
// should be ignored.
TEST_P(NewSdpTest, CheckSsrcGroupInSessionLevel) {
  ParseSdp(kSsrcGroupInSessionSDP, false);
  if (mSdp) {
    ASSERT_FALSE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
          SdpAttribute::kSsrcGroupAttribute));
    ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
          SdpAttribute::kSsrcGroupAttribute));
  }
}

const std::string kMalformedImageattr =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"m=video 9 RTP/SAVPF 120" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:120 VP8/90000" CRLF
"a=imageattr:flob" CRLF;

TEST_P(NewSdpTest, CheckMalformedImageattr)
{
  if (GetParam()) {
    // Don't do a parse/serialize before running this test
    return;
  }

  ParseSdp(kMalformedImageattr, false);
  ASSERT_NE("", GetParseErrors());
}

TEST_P(NewSdpTest, ParseInvalidSimulcastNoSuchSendRid) {
  ParseSdp("v=0" CRLF
           "o=- 4294967296 2 IN IP4 127.0.0.1" CRLF
           "s=SIP Call" CRLF
           "c=IN IP4 198.51.100.7" CRLF
           "b=CT:5000" CRLF
           "t=0 0" CRLF
           "m=video 56436 RTP/SAVPF 120" CRLF
           "a=rtpmap:120 VP8/90000" CRLF
           "a=sendrecv" CRLF
           "a=simulcast: send rid=9" CRLF,
           false);
  ASSERT_NE("", GetParseErrors());
}

TEST_P(NewSdpTest, ParseInvalidSimulcastNoSuchRecvRid) {
  ParseSdp("v=0" CRLF
           "o=- 4294967296 2 IN IP4 127.0.0.1" CRLF
           "s=SIP Call" CRLF
           "c=IN IP4 198.51.100.7" CRLF
           "b=CT:5000" CRLF
           "t=0 0" CRLF
           "m=video 56436 RTP/SAVPF 120" CRLF
           "a=rtpmap:120 VP8/90000" CRLF
           "a=sendrecv" CRLF
           "a=simulcast: recv rid=9" CRLF,
           false);
  ASSERT_NE("", GetParseErrors());
}

TEST_P(NewSdpTest, ParseInvalidSimulcastNoSuchPt) {
  ParseSdp("v=0" CRLF
           "o=- 4294967296 2 IN IP4 127.0.0.1" CRLF
           "s=SIP Call" CRLF
           "c=IN IP4 198.51.100.7" CRLF
           "b=CT:5000" CRLF
           "t=0 0" CRLF
           "m=video 56436 RTP/SAVPF 120" CRLF
           "a=rtpmap:120 VP8/90000" CRLF
           "a=sendrecv" CRLF
           "a=simulcast: send pt=9" CRLF,
           false);
  ASSERT_NE("", GetParseErrors());
}

TEST_P(NewSdpTest, ParseInvalidSimulcastNotSending) {
  ParseSdp("v=0" CRLF
           "o=- 4294967296 2 IN IP4 127.0.0.1" CRLF
           "s=SIP Call" CRLF
           "c=IN IP4 198.51.100.7" CRLF
           "b=CT:5000" CRLF
           "t=0 0" CRLF
           "m=video 56436 RTP/SAVPF 120" CRLF
           "a=rtpmap:120 VP8/90000" CRLF
           "a=recvonly" CRLF
           "a=simulcast: send pt=120" CRLF,
           false);
  ASSERT_NE("", GetParseErrors());
}

TEST_P(NewSdpTest, ParseInvalidSimulcastNotReceiving) {
  ParseSdp("v=0" CRLF
           "o=- 4294967296 2 IN IP4 127.0.0.1" CRLF
           "s=SIP Call" CRLF
           "c=IN IP4 198.51.100.7" CRLF
           "b=CT:5000" CRLF
           "t=0 0" CRLF
           "m=video 56436 RTP/SAVPF 120" CRLF
           "a=rtpmap:120 VP8/90000" CRLF
           "a=sendonly" CRLF
           "a=simulcast: recv pt=120" CRLF,
           false);
  ASSERT_NE("", GetParseErrors());
}

const std::string kNoAttributes =
"v=0" CRLF
"o=Mozilla-SIPUA-35.0a1 5184 0 IN IP4 0.0.0.0" CRLF
"s=SIP Call" CRLF
"c=IN IP4 224.0.0.1/100/12" CRLF
"t=0 0" CRLF
"m=video 9 RTP/SAVPF 120" CRLF
"c=IN IP4 0.0.0.0" CRLF
"a=rtpmap:120 VP8/90000" CRLF;

TEST_P(NewSdpTest, CheckNoAttributes) {
  ParseSdp(kNoAttributes);

  for (auto a = static_cast<size_t>(SdpAttribute::kFirstAttribute);
       a <= static_cast<size_t>(SdpAttribute::kLastAttribute);
       ++a) {

    SdpAttribute::AttributeType type =
      static_cast<SdpAttribute::AttributeType>(a);

    // rtpmap is a special case right now, we throw parse errors if it is
    // missing, and then insert one.
    // direction is another special case that gets a default if not present
    if (type != SdpAttribute::kRtpmapAttribute &&
        type != SdpAttribute::kDirectionAttribute) {
      ASSERT_FALSE(
          mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(type))
        << "Attribute " << a << " should not have been present at media level";
      ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(type))
        << "Attribute " << a << " should not have been present at session level";
    }
  }

  ASSERT_FALSE(mSdp->GetAttributeList().HasAttribute(
        SdpAttribute::kRtpmapAttribute));

  ASSERT_TRUE(mSdp->GetMediaSection(0).GetAttributeList().HasAttribute(
        SdpAttribute::kDirectionAttribute));
  ASSERT_EQ(SdpDirectionAttribute::kSendrecv,
      mSdp->GetMediaSection(0).GetAttributeList().GetDirection());
  ASSERT_TRUE(mSdp->GetAttributeList().HasAttribute(
        SdpAttribute::kDirectionAttribute));
  ASSERT_EQ(SdpDirectionAttribute::kSendrecv,
      mSdp->GetAttributeList().GetDirection());
}

TEST(NewSdpTestNoFixture, CheckAttributeTypeSerialize) {
  for (auto a = static_cast<size_t>(SdpAttribute::kFirstAttribute);
       a <= static_cast<size_t>(SdpAttribute::kLastAttribute);
       ++a) {

    SdpAttribute::AttributeType type =
      static_cast<SdpAttribute::AttributeType>(a);

    // Direction attributes are handled a little differently
    if (type != SdpAttribute::kDirectionAttribute) {
      std::ostringstream os;
      os << type;
      ASSERT_NE("", os.str());
    }
  }
}

static SdpImageattrAttributeList::XYRange
ParseXYRange(const std::string& input)
{
  std::istringstream is(input + ",");
  std::string error;
  SdpImageattrAttributeList::XYRange range;
  EXPECT_TRUE(range.Parse(is, &error)) << error;
  EXPECT_EQ(',', is.get());
  EXPECT_EQ(EOF, is.get());
  return range;
}

TEST(NewSdpTestNoFixture, CheckImageattrXYRangeParseValid)
{
  {
    SdpImageattrAttributeList::XYRange range(ParseXYRange("640"));
    ASSERT_EQ(1U, range.discreteValues.size());
    ASSERT_EQ(640U, range.discreteValues[0]);
  }

  {
    SdpImageattrAttributeList::XYRange range(ParseXYRange("[320,640]"));
    ASSERT_EQ(2U, range.discreteValues.size());
    ASSERT_EQ(320U, range.discreteValues[0]);
    ASSERT_EQ(640U, range.discreteValues[1]);
  }

  {
    SdpImageattrAttributeList::XYRange range(ParseXYRange("[320,640,1024]"));
    ASSERT_EQ(3U, range.discreteValues.size());
    ASSERT_EQ(320U, range.discreteValues[0]);
    ASSERT_EQ(640U, range.discreteValues[1]);
    ASSERT_EQ(1024U, range.discreteValues[2]);
  }

  {
    SdpImageattrAttributeList::XYRange range(ParseXYRange("[320:640]"));
    ASSERT_EQ(0U, range.discreteValues.size());
    ASSERT_EQ(320U, range.min);
    ASSERT_EQ(1U, range.step);
    ASSERT_EQ(640U, range.max);
  }

  {
    SdpImageattrAttributeList::XYRange range(ParseXYRange("[320:16:640]"));
    ASSERT_EQ(0U, range.discreteValues.size());
    ASSERT_EQ(320U, range.min);
    ASSERT_EQ(16U, range.step);
    ASSERT_EQ(640U, range.max);
  }
}

template<typename T>
void
ParseInvalid(const std::string& input, size_t last)
{
  std::istringstream is(input);
  T parsed;
  std::string error;
  ASSERT_FALSE(parsed.Parse(is, &error))
    << "\'" << input << "\' should not have parsed successfully";
  is.clear();
  ASSERT_EQ(last, static_cast<size_t>(is.tellg()))
    << "Parse failed at unexpected location:" << std::endl
    << input << std::endl
    << std::string(is.tellg(), ' ') << "^" << std::endl;
  // For a human to eyeball to make sure the error strings look sane
  std::cout << "\"" << input << "\" - " << error << std::endl; \
}

TEST(NewSdpTestNoFixture, CheckImageattrXYRangeParseInvalid)
{
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[-1", 1);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[-", 1);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[-v", 1);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640:-1", 5);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640:16:-1", 8);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640,-1", 5);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640,-]", 5);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("-v", 0);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("-1", 0);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("", 0);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[", 1);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[v", 1);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[", 1);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[ 640", 1);
  // It looks like the overflow detection only happens once the whole number
  // is scanned...
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[99999999999999999:", 18);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640", 4);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640:", 5);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640:v", 5);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640:16", 7);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640:16:", 8);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640:16:v", 8);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640:16:320]", 11);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640:16:320", 11);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640:16:320v", 11);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640:1024", 9);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640:320]", 8);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640:1024v", 9);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640,", 5);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640,v", 5);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640]", 4);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640x", 4);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("[640,]", 5);
  ParseInvalid<SdpImageattrAttributeList::XYRange>(" ", 0);
  ParseInvalid<SdpImageattrAttributeList::XYRange>("v", 0);
}

static SdpImageattrAttributeList::SRange
ParseSRange(const std::string& input)
{
  std::istringstream is(input + ",");
  std::string error;
  SdpImageattrAttributeList::SRange range;
  EXPECT_TRUE(range.Parse(is, &error)) << error;
  EXPECT_EQ(',', is.get());
  EXPECT_EQ(EOF, is.get());
  return range;
}

TEST(NewSdpTestNoFixture, CheckImageattrSRangeParseValid)
{
  {
    SdpImageattrAttributeList::SRange range(ParseSRange("0.1"));
    ASSERT_EQ(1U, range.discreteValues.size());
    ASSERT_FLOAT_EQ(0.1f, range.discreteValues[0]);
  }

  {
    SdpImageattrAttributeList::SRange range(ParseSRange("[0.1,0.2]"));
    ASSERT_EQ(2U, range.discreteValues.size());
    ASSERT_FLOAT_EQ(0.1f, range.discreteValues[0]);
    ASSERT_FLOAT_EQ(0.2f, range.discreteValues[1]);
  }

  {
    SdpImageattrAttributeList::SRange range(ParseSRange("[0.1,0.2,0.3]"));
    ASSERT_EQ(3U, range.discreteValues.size());
    ASSERT_FLOAT_EQ(0.1f, range.discreteValues[0]);
    ASSERT_FLOAT_EQ(0.2f, range.discreteValues[1]);
    ASSERT_FLOAT_EQ(0.3f, range.discreteValues[2]);
  }

  {
    SdpImageattrAttributeList::SRange range(ParseSRange("[0.1-0.2]"));
    ASSERT_EQ(0U, range.discreteValues.size());
    ASSERT_FLOAT_EQ(0.1f, range.min);
    ASSERT_FLOAT_EQ(0.2f, range.max);
  }
}

TEST(NewSdpTestNoFixture, CheckImageattrSRangeParseInvalid)
{
  ParseInvalid<SdpImageattrAttributeList::SRange>("", 0);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[", 1);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[v", 1);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[-1", 1);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[", 1);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[-", 1);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[v", 1);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[ 0.2", 1);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[10.1-", 5);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[0.08-", 5);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[0.2", 4);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[0.2-", 5);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[0.2-v", 5);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[0.2--1", 5);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[0.2-0.3", 8);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[0.2-0.1]", 8);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[0.2-0.3v", 8);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[0.2,", 5);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[0.2,v", 5);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[0.2,-1", 5);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[0.2]", 4);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[0.2v", 4);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[0.2,]", 5);
  ParseInvalid<SdpImageattrAttributeList::SRange>("[0.2,-]", 5);
  ParseInvalid<SdpImageattrAttributeList::SRange>(" ", 0);
  ParseInvalid<SdpImageattrAttributeList::SRange>("v", 0);
  ParseInvalid<SdpImageattrAttributeList::SRange>("-v", 0);
  ParseInvalid<SdpImageattrAttributeList::SRange>("-1", 0);
}

static SdpImageattrAttributeList::PRange
ParsePRange(const std::string& input)
{
  std::istringstream is(input + ",");
  std::string error;
  SdpImageattrAttributeList::PRange range;
  EXPECT_TRUE(range.Parse(is, &error)) << error;
  EXPECT_EQ(',', is.get());
  EXPECT_EQ(EOF, is.get());
  return range;
}

TEST(NewSdpTestNoFixture, CheckImageattrPRangeParseValid)
{
  SdpImageattrAttributeList::PRange range(ParsePRange("[0.1000-9.9999]"));
  ASSERT_FLOAT_EQ(0.1f, range.min);
  ASSERT_FLOAT_EQ(9.9999f, range.max);
}

TEST(NewSdpTestNoFixture, CheckImageattrPRangeParseInvalid)
{
  ParseInvalid<SdpImageattrAttributeList::PRange>("", 0);
  ParseInvalid<SdpImageattrAttributeList::PRange>("[", 1);
  ParseInvalid<SdpImageattrAttributeList::PRange>("[v", 1);
  ParseInvalid<SdpImageattrAttributeList::PRange>("[-1", 1);
  ParseInvalid<SdpImageattrAttributeList::PRange>("[", 1);
  ParseInvalid<SdpImageattrAttributeList::PRange>("[-", 1);
  ParseInvalid<SdpImageattrAttributeList::PRange>("[v", 1);
  ParseInvalid<SdpImageattrAttributeList::PRange>("[ 0.2", 1);
  ParseInvalid<SdpImageattrAttributeList::PRange>("[10.1-", 5);
  ParseInvalid<SdpImageattrAttributeList::PRange>("[0.08-", 5);
  ParseInvalid<SdpImageattrAttributeList::PRange>("[0.2", 4);
  ParseInvalid<SdpImageattrAttributeList::PRange>("[0.2-", 5);
  ParseInvalid<SdpImageattrAttributeList::PRange>("[0.2-v", 5);
  ParseInvalid<SdpImageattrAttributeList::PRange>("[0.2--1", 5);
  ParseInvalid<SdpImageattrAttributeList::PRange>("[0.2-0.3", 8);
  ParseInvalid<SdpImageattrAttributeList::PRange>("[0.2-0.1]", 8);
  ParseInvalid<SdpImageattrAttributeList::PRange>("[0.2-0.3v", 8);
  ParseInvalid<SdpImageattrAttributeList::PRange>("[0.2,", 4);
  ParseInvalid<SdpImageattrAttributeList::PRange>("[0.2:", 4);
  ParseInvalid<SdpImageattrAttributeList::PRange>("[0.2]", 4);
  ParseInvalid<SdpImageattrAttributeList::PRange>("[0.2v", 4);
  ParseInvalid<SdpImageattrAttributeList::PRange>(" ", 0);
  ParseInvalid<SdpImageattrAttributeList::PRange>("v", 0);
  ParseInvalid<SdpImageattrAttributeList::PRange>("-x", 0);
  ParseInvalid<SdpImageattrAttributeList::PRange>("-1", 0);
}

static SdpImageattrAttributeList::Set
ParseSet(const std::string& input)
{
  std::istringstream is(input + " ");
  std::string error;
  SdpImageattrAttributeList::Set set;
  EXPECT_TRUE(set.Parse(is, &error)) << error;
  EXPECT_EQ(' ', is.get());
  EXPECT_EQ(EOF, is.get());
  return set;
}

TEST(NewSdpTestNoFixture, CheckImageattrSetParseValid)
{
  {
    SdpImageattrAttributeList::Set set(ParseSet("[x=320,y=240]"));
    ASSERT_EQ(1U, set.xRange.discreteValues.size());
    ASSERT_EQ(320U, set.xRange.discreteValues[0]);
    ASSERT_EQ(1U, set.yRange.discreteValues.size());
    ASSERT_EQ(240U, set.yRange.discreteValues[0]);
    ASSERT_FALSE(set.sRange.IsSet());
    ASSERT_FALSE(set.pRange.IsSet());
    ASSERT_FLOAT_EQ(0.5f, set.qValue);
  }

  {
    SdpImageattrAttributeList::Set set(ParseSet("[X=320,Y=240]"));
    ASSERT_EQ(1U, set.xRange.discreteValues.size());
    ASSERT_EQ(320U, set.xRange.discreteValues[0]);
    ASSERT_EQ(1U, set.yRange.discreteValues.size());
    ASSERT_EQ(240U, set.yRange.discreteValues[0]);
    ASSERT_FALSE(set.sRange.IsSet());
    ASSERT_FALSE(set.pRange.IsSet());
    ASSERT_FLOAT_EQ(0.5f, set.qValue);
  }

  {
    SdpImageattrAttributeList::Set set(ParseSet("[x=320,y=240,par=[0.1-0.2]]"));
    ASSERT_EQ(1U, set.xRange.discreteValues.size());
    ASSERT_EQ(320U, set.xRange.discreteValues[0]);
    ASSERT_EQ(1U, set.yRange.discreteValues.size());
    ASSERT_EQ(240U, set.yRange.discreteValues[0]);
    ASSERT_FALSE(set.sRange.IsSet());
    ASSERT_TRUE(set.pRange.IsSet());
    ASSERT_FLOAT_EQ(0.1f, set.pRange.min);
    ASSERT_FLOAT_EQ(0.2f, set.pRange.max);
    ASSERT_FLOAT_EQ(0.5f, set.qValue);
  }

  {
    SdpImageattrAttributeList::Set set(ParseSet("[x=320,y=240,sar=[0.1-0.2]]"));
    ASSERT_EQ(1U, set.xRange.discreteValues.size());
    ASSERT_EQ(320U, set.xRange.discreteValues[0]);
    ASSERT_EQ(1U, set.yRange.discreteValues.size());
    ASSERT_EQ(240U, set.yRange.discreteValues[0]);
    ASSERT_TRUE(set.sRange.IsSet());
    ASSERT_FLOAT_EQ(0.1f, set.sRange.min);
    ASSERT_FLOAT_EQ(0.2f, set.sRange.max);
    ASSERT_FALSE(set.pRange.IsSet());
    ASSERT_FLOAT_EQ(0.5f, set.qValue);
  }

  {
    SdpImageattrAttributeList::Set set(ParseSet("[x=320,y=240,q=0.1]"));
    ASSERT_EQ(1U, set.xRange.discreteValues.size());
    ASSERT_EQ(320U, set.xRange.discreteValues[0]);
    ASSERT_EQ(1U, set.yRange.discreteValues.size());
    ASSERT_EQ(240U, set.yRange.discreteValues[0]);
    ASSERT_FALSE(set.sRange.IsSet());
    ASSERT_FALSE(set.pRange.IsSet());
    ASSERT_FLOAT_EQ(0.1f, set.qValue);
  }

  {
    SdpImageattrAttributeList::Set set(
        ParseSet("[x=320,y=240,par=[0.1-0.2],sar=[0.3-0.4],q=0.6]"));
    ASSERT_EQ(1U, set.xRange.discreteValues.size());
    ASSERT_EQ(320U, set.xRange.discreteValues[0]);
    ASSERT_EQ(1U, set.yRange.discreteValues.size());
    ASSERT_EQ(240U, set.yRange.discreteValues[0]);
    ASSERT_TRUE(set.sRange.IsSet());
    ASSERT_FLOAT_EQ(0.3f, set.sRange.min);
    ASSERT_FLOAT_EQ(0.4f, set.sRange.max);
    ASSERT_TRUE(set.pRange.IsSet());
    ASSERT_FLOAT_EQ(0.1f, set.pRange.min);
    ASSERT_FLOAT_EQ(0.2f, set.pRange.max);
    ASSERT_FLOAT_EQ(0.6f, set.qValue);
  }

  {
    SdpImageattrAttributeList::Set set(ParseSet("[x=320,y=240,foo=bar,q=0.1]"));
    ASSERT_EQ(1U, set.xRange.discreteValues.size());
    ASSERT_EQ(320U, set.xRange.discreteValues[0]);
    ASSERT_EQ(1U, set.yRange.discreteValues.size());
    ASSERT_EQ(240U, set.yRange.discreteValues[0]);
    ASSERT_FALSE(set.sRange.IsSet());
    ASSERT_FALSE(set.pRange.IsSet());
    ASSERT_FLOAT_EQ(0.1f, set.qValue);
  }

  {
    SdpImageattrAttributeList::Set set(
        ParseSet("[x=320,y=240,foo=bar,q=0.1,bar=baz]"));
    ASSERT_EQ(1U, set.xRange.discreteValues.size());
    ASSERT_EQ(320U, set.xRange.discreteValues[0]);
    ASSERT_EQ(1U, set.yRange.discreteValues.size());
    ASSERT_EQ(240U, set.yRange.discreteValues[0]);
    ASSERT_FALSE(set.sRange.IsSet());
    ASSERT_FALSE(set.pRange.IsSet());
    ASSERT_FLOAT_EQ(0.1f, set.qValue);
  }

  {
    SdpImageattrAttributeList::Set set(
        ParseSet("[x=320,y=240,foo=[bar],q=0.1,bar=[baz]]"));
    ASSERT_EQ(1U, set.xRange.discreteValues.size());
    ASSERT_EQ(320U, set.xRange.discreteValues[0]);
    ASSERT_EQ(1U, set.yRange.discreteValues.size());
    ASSERT_EQ(240U, set.yRange.discreteValues[0]);
    ASSERT_FALSE(set.sRange.IsSet());
    ASSERT_FALSE(set.pRange.IsSet());
    ASSERT_FLOAT_EQ(0.1f, set.qValue);
  }

  {
    SdpImageattrAttributeList::Set set(
        ParseSet("[x=320,y=240,foo=[par=foo,sar=bar],q=0.1,bar=[baz]]"));
    ASSERT_EQ(1U, set.xRange.discreteValues.size());
    ASSERT_EQ(320U, set.xRange.discreteValues[0]);
    ASSERT_EQ(1U, set.yRange.discreteValues.size());
    ASSERT_EQ(240U, set.yRange.discreteValues[0]);
    ASSERT_FALSE(set.sRange.IsSet());
    ASSERT_FALSE(set.pRange.IsSet());
    ASSERT_FLOAT_EQ(0.1f, set.qValue);
  }
}

TEST(NewSdpTestNoFixture, CheckImageattrSetParseInvalid)
{
  ParseInvalid<SdpImageattrAttributeList::Set>("", 0);
  ParseInvalid<SdpImageattrAttributeList::Set>("x", 0);
  ParseInvalid<SdpImageattrAttributeList::Set>("[", 1);
  ParseInvalid<SdpImageattrAttributeList::Set>("[=", 2);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x", 2);
  ParseInvalid<SdpImageattrAttributeList::Set>("[y=", 3);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=[", 4);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320", 6);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320v", 6);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,", 7);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,=", 8);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,x", 8);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,x=", 9);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,y=[", 10);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,y=240", 12);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,y=240x", 12);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,y=240,", 13);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,y=240,q=", 15);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,y=240,q=v", 15);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,y=240,q=0.5", 18);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,y=240,q=0.5,", 19);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,y=240,q=0.5,]", 20);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,y=240,q=0.5,=]", 20);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,y=240,q=0.5,sar=v]", 23);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,y=240,q=0.5,q=0.4", 21);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,y=240,sar=", 17);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,y=240,sar=v", 17);
  ParseInvalid<SdpImageattrAttributeList::Set>(
      "[x=320,y=240,sar=[0.5-0.6],sar=[0.7-0.8]", 31);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,y=240,par=", 17);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,y=240,par=x", 17);
  ParseInvalid<SdpImageattrAttributeList::Set>(
      "[x=320,y=240,par=[0.5-0.6],par=[0.7-0.8]", 31);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,y=240,foo=", 17);
  ParseInvalid<SdpImageattrAttributeList::Set>("[x=320,y=240,foo=x", 18);
}

static SdpImageattrAttributeList::Imageattr
ParseImageattr(const std::string& input)
{
  std::istringstream is(input);
  std::string error;
  SdpImageattrAttributeList::Imageattr imageattr;
  EXPECT_TRUE(imageattr.Parse(is, &error)) << error;
  EXPECT_TRUE(is.eof());
  return imageattr;
}

TEST(NewSdpTestNoFixture, CheckImageattrParseValid)
{
  {
    SdpImageattrAttributeList::Imageattr imageattr(ParseImageattr("* send *"));
    ASSERT_FALSE(imageattr.pt.isSome());
    ASSERT_TRUE(imageattr.sendAll);
    ASSERT_TRUE(imageattr.sendSets.empty());
    ASSERT_FALSE(imageattr.recvAll);
    ASSERT_TRUE(imageattr.recvSets.empty());
  }

  {
    SdpImageattrAttributeList::Imageattr imageattr(ParseImageattr("* SEND *"));
    ASSERT_FALSE(imageattr.pt.isSome());
    ASSERT_TRUE(imageattr.sendAll);
    ASSERT_TRUE(imageattr.sendSets.empty());
    ASSERT_FALSE(imageattr.recvAll);
    ASSERT_TRUE(imageattr.recvSets.empty());
  }

  {
    SdpImageattrAttributeList::Imageattr imageattr(ParseImageattr("* recv *"));
    ASSERT_FALSE(imageattr.pt.isSome());
    ASSERT_FALSE(imageattr.sendAll);
    ASSERT_TRUE(imageattr.sendSets.empty());
    ASSERT_TRUE(imageattr.recvAll);
    ASSERT_TRUE(imageattr.recvSets.empty());
  }

  {
    SdpImageattrAttributeList::Imageattr imageattr(ParseImageattr("* RECV *"));
    ASSERT_FALSE(imageattr.pt.isSome());
    ASSERT_FALSE(imageattr.sendAll);
    ASSERT_TRUE(imageattr.sendSets.empty());
    ASSERT_TRUE(imageattr.recvAll);
    ASSERT_TRUE(imageattr.recvSets.empty());
  }

  {
    SdpImageattrAttributeList::Imageattr imageattr(
        ParseImageattr("* recv * send *"));
    ASSERT_FALSE(imageattr.pt.isSome());
    ASSERT_TRUE(imageattr.sendAll);
    ASSERT_TRUE(imageattr.sendSets.empty());
    ASSERT_TRUE(imageattr.recvAll);
    ASSERT_TRUE(imageattr.recvSets.empty());
  }

  {
    SdpImageattrAttributeList::Imageattr imageattr(
        ParseImageattr("* send * recv *"));
    ASSERT_FALSE(imageattr.pt.isSome());
    ASSERT_TRUE(imageattr.sendAll);
    ASSERT_TRUE(imageattr.sendSets.empty());
    ASSERT_TRUE(imageattr.recvAll);
    ASSERT_TRUE(imageattr.recvSets.empty());
  }

  {
    SdpImageattrAttributeList::Imageattr imageattr(
        ParseImageattr("8 send * recv *"));
    ASSERT_EQ(8U, *imageattr.pt);
    ASSERT_TRUE(imageattr.sendAll);
    ASSERT_TRUE(imageattr.sendSets.empty());
    ASSERT_TRUE(imageattr.recvAll);
    ASSERT_TRUE(imageattr.recvSets.empty());
  }

  {
    SdpImageattrAttributeList::Imageattr imageattr(
        ParseImageattr("8 send [x=320,y=240] recv *"));
    ASSERT_EQ(8U, *imageattr.pt);
    ASSERT_FALSE(imageattr.sendAll);
    ASSERT_EQ(1U, imageattr.sendSets.size());
    ASSERT_EQ(1U, imageattr.sendSets[0].xRange.discreteValues.size());
    ASSERT_EQ(320U, imageattr.sendSets[0].xRange.discreteValues[0]);
    ASSERT_EQ(1U, imageattr.sendSets[0].yRange.discreteValues.size());
    ASSERT_EQ(240U, imageattr.sendSets[0].yRange.discreteValues[0]);
    ASSERT_TRUE(imageattr.recvAll);
    ASSERT_TRUE(imageattr.recvSets.empty());
  }

  {
    SdpImageattrAttributeList::Imageattr imageattr(
        ParseImageattr("8 send [x=320,y=240] [x=640,y=480] recv *"));
    ASSERT_EQ(8U, *imageattr.pt);
    ASSERT_FALSE(imageattr.sendAll);
    ASSERT_EQ(2U, imageattr.sendSets.size());
    ASSERT_EQ(1U, imageattr.sendSets[0].xRange.discreteValues.size());
    ASSERT_EQ(320U, imageattr.sendSets[0].xRange.discreteValues[0]);
    ASSERT_EQ(1U, imageattr.sendSets[0].yRange.discreteValues.size());
    ASSERT_EQ(240U, imageattr.sendSets[0].yRange.discreteValues[0]);
    ASSERT_EQ(1U, imageattr.sendSets[1].xRange.discreteValues.size());
    ASSERT_EQ(640U, imageattr.sendSets[1].xRange.discreteValues[0]);
    ASSERT_EQ(1U, imageattr.sendSets[1].yRange.discreteValues.size());
    ASSERT_EQ(480U, imageattr.sendSets[1].yRange.discreteValues[0]);
    ASSERT_TRUE(imageattr.recvAll);
    ASSERT_TRUE(imageattr.recvSets.empty());
  }

  {
    SdpImageattrAttributeList::Imageattr imageattr(
        ParseImageattr("8 send * recv [x=320,y=240]"));
    ASSERT_EQ(8U, *imageattr.pt);
    ASSERT_FALSE(imageattr.recvAll);
    ASSERT_EQ(1U, imageattr.recvSets.size());
    ASSERT_EQ(1U, imageattr.recvSets[0].xRange.discreteValues.size());
    ASSERT_EQ(320U, imageattr.recvSets[0].xRange.discreteValues[0]);
    ASSERT_EQ(1U, imageattr.recvSets[0].yRange.discreteValues.size());
    ASSERT_EQ(240U, imageattr.recvSets[0].yRange.discreteValues[0]);
    ASSERT_TRUE(imageattr.sendAll);
    ASSERT_TRUE(imageattr.sendSets.empty());
  }

  {
    SdpImageattrAttributeList::Imageattr imageattr(
        ParseImageattr("8 send * recv [x=320,y=240] [x=640,y=480]"));
    ASSERT_EQ(8U, *imageattr.pt);
    ASSERT_FALSE(imageattr.recvAll);
    ASSERT_EQ(2U, imageattr.recvSets.size());
    ASSERT_EQ(1U, imageattr.recvSets[0].xRange.discreteValues.size());
    ASSERT_EQ(320U, imageattr.recvSets[0].xRange.discreteValues[0]);
    ASSERT_EQ(1U, imageattr.recvSets[0].yRange.discreteValues.size());
    ASSERT_EQ(240U, imageattr.recvSets[0].yRange.discreteValues[0]);
    ASSERT_EQ(1U, imageattr.recvSets[1].xRange.discreteValues.size());
    ASSERT_EQ(640U, imageattr.recvSets[1].xRange.discreteValues[0]);
    ASSERT_EQ(1U, imageattr.recvSets[1].yRange.discreteValues.size());
    ASSERT_EQ(480U, imageattr.recvSets[1].yRange.discreteValues[0]);
    ASSERT_TRUE(imageattr.sendAll);
    ASSERT_TRUE(imageattr.sendSets.empty());
  }
}

TEST(NewSdpTestNoFixture, CheckImageattrParseInvalid)
{
  ParseInvalid<SdpImageattrAttributeList::Imageattr>("", 0);
  ParseInvalid<SdpImageattrAttributeList::Imageattr>(" ", 0);
  ParseInvalid<SdpImageattrAttributeList::Imageattr>("-1", 0);
  ParseInvalid<SdpImageattrAttributeList::Imageattr>("99999 ", 5);
  ParseInvalid<SdpImageattrAttributeList::Imageattr>("*", 1);
  ParseInvalid<SdpImageattrAttributeList::Imageattr>("* sen", 5);
  ParseInvalid<SdpImageattrAttributeList::Imageattr>("* vcer *", 6);
  ParseInvalid<SdpImageattrAttributeList::Imageattr>("* send x", 7);
  ParseInvalid<SdpImageattrAttributeList::Imageattr>(
      "* send [x=640,y=480] [", 22);
  ParseInvalid<SdpImageattrAttributeList::Imageattr>("* send * sen", 12);
  ParseInvalid<SdpImageattrAttributeList::Imageattr>("* send * vcer *", 13);
  ParseInvalid<SdpImageattrAttributeList::Imageattr>("* send * send *", 13);
  ParseInvalid<SdpImageattrAttributeList::Imageattr>("* recv * recv *", 13);
  ParseInvalid<SdpImageattrAttributeList::Imageattr>("* send * recv x", 14);
  ParseInvalid<SdpImageattrAttributeList::Imageattr>(
      "* send * recv [x=640,y=480] [", 29);
  ParseInvalid<SdpImageattrAttributeList::Imageattr>(
      "* send * recv [x=640,y=480] *", 28);
  ParseInvalid<SdpImageattrAttributeList::Imageattr>(
      "* send * recv [x=640,y=480] foobajooba", 28);
}

TEST(NewSdpTestNoFixture, CheckImageattrXYRangeSerialization)
{
  SdpImageattrAttributeList::XYRange range;
  std::stringstream os;

  range.min = 320;
  range.max = 640;
  range.Serialize(os);
  ASSERT_EQ("[320:640]", os.str());
  os.str(""); // clear

  range.step = 16;
  range.Serialize(os);
  ASSERT_EQ("[320:16:640]", os.str());
  os.str(""); // clear

  range.min = 0;
  range.max = 0;
  range.discreteValues.push_back(320);
  range.Serialize(os);
  ASSERT_EQ("320", os.str());
  os.str("");

  range.discreteValues.push_back(640);
  range.Serialize(os);
  ASSERT_EQ("[320,640]", os.str());
}

TEST(NewSdpTestNoFixture, CheckImageattrSRangeSerialization)
{
  SdpImageattrAttributeList::SRange range;
  std::ostringstream os;

  range.min = 0.1f;
  range.max = 0.9999f;
  range.Serialize(os);
  ASSERT_EQ("[0.1000-0.9999]", os.str());
  os.str("");

  range.min = 0.0f;
  range.max = 0.0f;
  range.discreteValues.push_back(0.1f);
  range.Serialize(os);
  ASSERT_EQ("0.1000", os.str());
  os.str("");

  range.discreteValues.push_back(0.5f);
  range.Serialize(os);
  ASSERT_EQ("[0.1000,0.5000]", os.str());
}

TEST(NewSdpTestNoFixture, CheckImageattrPRangeSerialization)
{
  SdpImageattrAttributeList::PRange range;
  std::ostringstream os;

  range.min = 0.1f;
  range.max = 0.9999f;
  range.Serialize(os);
  ASSERT_EQ("[0.1000-0.9999]", os.str());
}

TEST(NewSdpTestNoFixture, CheckImageattrSetSerialization)
{
  SdpImageattrAttributeList::Set set;
  std::ostringstream os;

  set.xRange.discreteValues.push_back(640);
  set.yRange.discreteValues.push_back(480);
  set.Serialize(os);
  ASSERT_EQ("[x=640,y=480]", os.str());
  os.str("");

  set.qValue = 0.00f;
  set.Serialize(os);
  ASSERT_EQ("[x=640,y=480,q=0.00]", os.str());
  os.str("");

  set.qValue = 0.10f;
  set.Serialize(os);
  ASSERT_EQ("[x=640,y=480,q=0.10]", os.str());
  os.str("");

  set.qValue = 1.00f;
  set.Serialize(os);
  ASSERT_EQ("[x=640,y=480,q=1.00]", os.str());
  os.str("");

  set.sRange.discreteValues.push_back(1.1f);
  set.Serialize(os);
  ASSERT_EQ("[x=640,y=480,sar=1.1000,q=1.00]", os.str());
  os.str("");

  set.pRange.min = 0.9f;
  set.pRange.max = 1.1f;
  set.Serialize(os);
  ASSERT_EQ("[x=640,y=480,sar=1.1000,par=[0.9000-1.1000],q=1.00]", os.str());
  os.str("");
}

TEST(NewSdpTestNoFixture, CheckImageattrSerialization)
{
  SdpImageattrAttributeList::Imageattr imageattr;
  std::ostringstream os;

  imageattr.sendAll = true;
  imageattr.pt = Some<uint16_t>(8U);
  imageattr.Serialize(os);
  ASSERT_EQ("8 send *", os.str());
  os.str("");

  imageattr.pt.reset();;
  imageattr.Serialize(os);
  ASSERT_EQ("* send *", os.str());
  os.str("");

  imageattr.sendAll = false;
  imageattr.recvAll = true;
  imageattr.Serialize(os);
  ASSERT_EQ("* recv *", os.str());
  os.str("");

  imageattr.sendAll = true;
  imageattr.Serialize(os);
  ASSERT_EQ("* send * recv *", os.str());
  os.str("");

  imageattr.sendAll = false;
  imageattr.sendSets.push_back(SdpImageattrAttributeList::Set());
  imageattr.sendSets.back().xRange.discreteValues.push_back(320);
  imageattr.sendSets.back().yRange.discreteValues.push_back(240);
  imageattr.Serialize(os);
  ASSERT_EQ("* send [x=320,y=240] recv *", os.str());
  os.str("");

  imageattr.sendSets.push_back(SdpImageattrAttributeList::Set());
  imageattr.sendSets.back().xRange.discreteValues.push_back(640);
  imageattr.sendSets.back().yRange.discreteValues.push_back(480);
  imageattr.Serialize(os);
  ASSERT_EQ("* send [x=320,y=240] [x=640,y=480] recv *", os.str());
  os.str("");

  imageattr.recvAll = false;
  imageattr.recvSets.push_back(SdpImageattrAttributeList::Set());
  imageattr.recvSets.back().xRange.discreteValues.push_back(320);
  imageattr.recvSets.back().yRange.discreteValues.push_back(240);
  imageattr.Serialize(os);
  ASSERT_EQ("* send [x=320,y=240] [x=640,y=480] recv [x=320,y=240]", os.str());
  os.str("");

  imageattr.recvSets.push_back(SdpImageattrAttributeList::Set());
  imageattr.recvSets.back().xRange.discreteValues.push_back(640);
  imageattr.recvSets.back().yRange.discreteValues.push_back(480);
  imageattr.Serialize(os);
  ASSERT_EQ(
      "* send [x=320,y=240] [x=640,y=480] recv [x=320,y=240] [x=640,y=480]",
      os.str());
  os.str("");
}

TEST(NewSdpTestNoFixture, CheckSimulcastVersionSerialize)
{
  std::ostringstream os;

  SdpSimulcastAttribute::Version version;
  version.choices.push_back("8");
  version.Serialize(os);
  ASSERT_EQ("8", os.str());
  os.str("");

  version.choices.push_back("9");
  version.Serialize(os);
  ASSERT_EQ("8,9", os.str());
  os.str("");

  version.choices.push_back("0");
  version.Serialize(os);
  ASSERT_EQ("8,9,0", os.str());
  os.str("");
}

static SdpSimulcastAttribute::Version
ParseSimulcastVersion(const std::string& input)
{
  std::istringstream is(input + ";");
  std::string error;
  SdpSimulcastAttribute::Version version;
  EXPECT_TRUE(version.Parse(is, &error)) << error;
  EXPECT_EQ(';', is.get());
  EXPECT_EQ(EOF, is.get());
  return version;
}

TEST(NewSdpTestNoFixture, CheckSimulcastVersionValidParse)
{
  {
    SdpSimulcastAttribute::Version version(
        ParseSimulcastVersion("1"));
    ASSERT_EQ(1U, version.choices.size());
    ASSERT_EQ("1", version.choices[0]);
  }

  {
    SdpSimulcastAttribute::Version version(
        ParseSimulcastVersion("1,2"));
    ASSERT_EQ(2U, version.choices.size());
    ASSERT_EQ("1", version.choices[0]);
    ASSERT_EQ("2", version.choices[1]);
  }
}

TEST(NewSdpTestNoFixture, CheckSimulcastVersionInvalidParse)
{
  ParseInvalid<SdpSimulcastAttribute::Version>("", 0);
  ParseInvalid<SdpSimulcastAttribute::Version>(",", 0);
  ParseInvalid<SdpSimulcastAttribute::Version>(";", 0);
  ParseInvalid<SdpSimulcastAttribute::Version>(" ", 0);
  ParseInvalid<SdpSimulcastAttribute::Version>("8,", 2);
  ParseInvalid<SdpSimulcastAttribute::Version>("8, ", 2);
  ParseInvalid<SdpSimulcastAttribute::Version>("8,,", 2);
  ParseInvalid<SdpSimulcastAttribute::Version>("8,;", 2);
}

TEST(NewSdpTestNoFixture, CheckSimulcastVersionsSerialize)
{
  std::ostringstream os;

  SdpSimulcastAttribute::Versions versions;
  versions.type = SdpSimulcastAttribute::Versions::kPt;
  versions.push_back(SdpSimulcastAttribute::Version());
  versions.back().choices.push_back("8");
  versions.Serialize(os);
  ASSERT_EQ("pt=8", os.str());
  os.str("");

  versions.type = SdpSimulcastAttribute::Versions::kRid;
  versions.Serialize(os);
  ASSERT_EQ("rid=8", os.str());
  os.str("");

  versions.push_back(SdpSimulcastAttribute::Version());
  versions.Serialize(os);
  ASSERT_EQ("rid=8", os.str());
  os.str("");

  versions.back().choices.push_back("9");
  versions.Serialize(os);
  ASSERT_EQ("rid=8;9", os.str());
  os.str("");

  versions.push_back(SdpSimulcastAttribute::Version());
  versions.back().choices.push_back("0");
  versions.Serialize(os);
  ASSERT_EQ("rid=8;9;0", os.str());
  os.str("");
}

static SdpSimulcastAttribute::Versions
ParseSimulcastVersions(const std::string& input)
{
  std::istringstream is(input + " ");
  std::string error;
  SdpSimulcastAttribute::Versions list;
  EXPECT_TRUE(list.Parse(is, &error)) << error;
  EXPECT_EQ(' ', is.get());
  EXPECT_EQ(EOF, is.get());
  return list;
}

TEST(NewSdpTestNoFixture, CheckSimulcastVersionsValidParse)
{
  {
    SdpSimulcastAttribute::Versions versions(
        ParseSimulcastVersions("pt=8"));
    ASSERT_EQ(1U, versions.size());
    ASSERT_EQ(SdpSimulcastAttribute::Versions::kPt, versions.type);
    ASSERT_EQ(1U, versions[0].choices.size());
    ASSERT_EQ("8", versions[0].choices[0]);
  }

  {
    SdpSimulcastAttribute::Versions versions(
        ParseSimulcastVersions("rid=8"));
    ASSERT_EQ(1U, versions.size());
    ASSERT_EQ(SdpSimulcastAttribute::Versions::kRid, versions.type);
    ASSERT_EQ(1U, versions[0].choices.size());
    ASSERT_EQ("8", versions[0].choices[0]);
  }

  {
    SdpSimulcastAttribute::Versions versions(
        ParseSimulcastVersions("pt=8,9"));
    ASSERT_EQ(1U, versions.size());
    ASSERT_EQ(2U, versions[0].choices.size());
    ASSERT_EQ("8", versions[0].choices[0]);
    ASSERT_EQ("9", versions[0].choices[1]);
  }

  {
    SdpSimulcastAttribute::Versions versions(
        ParseSimulcastVersions("pt=8,9;10"));
    ASSERT_EQ(2U, versions.size());
    ASSERT_EQ(2U, versions[0].choices.size());
    ASSERT_EQ("8", versions[0].choices[0]);
    ASSERT_EQ("9", versions[0].choices[1]);
    ASSERT_EQ(1U, versions[1].choices.size());
    ASSERT_EQ("10", versions[1].choices[0]);
  }
}

TEST(NewSdpTestNoFixture, CheckSimulcastVersionsInvalidParse)
{
  ParseInvalid<SdpSimulcastAttribute::Versions>("", 0);
  ParseInvalid<SdpSimulcastAttribute::Versions>("x", 1);
  ParseInvalid<SdpSimulcastAttribute::Versions>(";", 1);
  ParseInvalid<SdpSimulcastAttribute::Versions>("8", 1);
  ParseInvalid<SdpSimulcastAttribute::Versions>("foo=", 4);
  ParseInvalid<SdpSimulcastAttribute::Versions>("foo=8", 4);
  ParseInvalid<SdpSimulcastAttribute::Versions>("pt=9999", 7);
  ParseInvalid<SdpSimulcastAttribute::Versions>("pt=-1", 5);
  ParseInvalid<SdpSimulcastAttribute::Versions>("pt=x", 4);
  ParseInvalid<SdpSimulcastAttribute::Versions>("pt=8;", 5);
  ParseInvalid<SdpSimulcastAttribute::Versions>("pt=8;x", 6);
  ParseInvalid<SdpSimulcastAttribute::Versions>("pt=8;;", 5);
}

TEST(NewSdpTestNoFixture, CheckSimulcastSerialize)
{
  std::ostringstream os;

  SdpSimulcastAttribute simulcast;
  simulcast.recvVersions.type = SdpSimulcastAttribute::Versions::kPt;
  simulcast.recvVersions.push_back(SdpSimulcastAttribute::Version());
  simulcast.recvVersions.back().choices.push_back("8");
  simulcast.Serialize(os);
  ASSERT_EQ("a=simulcast: recv pt=8" CRLF, os.str());
  os.str("");

  simulcast.sendVersions.push_back(SdpSimulcastAttribute::Version());
  simulcast.sendVersions.back().choices.push_back("9");
  simulcast.Serialize(os);
  ASSERT_EQ("a=simulcast: send rid=9 recv pt=8" CRLF, os.str());
  os.str("");
}

static SdpSimulcastAttribute
ParseSimulcast(const std::string& input)
{
  std::istringstream is(input);
  std::string error;
  SdpSimulcastAttribute simulcast;
  EXPECT_TRUE(simulcast.Parse(is, &error)) << error;
  EXPECT_TRUE(is.eof());
  return simulcast;
}

TEST(NewSdpTestNoFixture, CheckSimulcastValidParse)
{
  {
    SdpSimulcastAttribute simulcast(ParseSimulcast(" send pt=8"));
    ASSERT_EQ(1U, simulcast.sendVersions.size());
    ASSERT_EQ(SdpSimulcastAttribute::Versions::kPt,
              simulcast.sendVersions.type);
    ASSERT_EQ(1U, simulcast.sendVersions[0].choices.size());
    ASSERT_EQ("8", simulcast.sendVersions[0].choices[0]);
    ASSERT_EQ(0U, simulcast.recvVersions.size());
  }

  {
    SdpSimulcastAttribute simulcast(ParseSimulcast(" SEND pt=8"));
    ASSERT_EQ(1U, simulcast.sendVersions.size());
    ASSERT_EQ(SdpSimulcastAttribute::Versions::kPt,
              simulcast.sendVersions.type);
    ASSERT_EQ(1U, simulcast.sendVersions[0].choices.size());
    ASSERT_EQ("8", simulcast.sendVersions[0].choices[0]);
    ASSERT_EQ(0U, simulcast.recvVersions.size());
  }

  {
    SdpSimulcastAttribute simulcast(ParseSimulcast(" recv pt=8"));
    ASSERT_EQ(1U, simulcast.recvVersions.size());
    ASSERT_EQ(SdpSimulcastAttribute::Versions::kPt,
              simulcast.recvVersions.type);
    ASSERT_EQ(1U, simulcast.recvVersions[0].choices.size());
    ASSERT_EQ("8", simulcast.recvVersions[0].choices[0]);
    ASSERT_EQ(0U, simulcast.sendVersions.size());
  }

  {
    SdpSimulcastAttribute simulcast(
        ParseSimulcast(
          " send pt=8,9;101;97,98 recv pt=101,120;97"));
    ASSERT_EQ(3U, simulcast.sendVersions.size());
    ASSERT_EQ(SdpSimulcastAttribute::Versions::kPt,
              simulcast.sendVersions.type);
    ASSERT_EQ(2U, simulcast.sendVersions[0].choices.size());
    ASSERT_EQ("8", simulcast.sendVersions[0].choices[0]);
    ASSERT_EQ("9", simulcast.sendVersions[0].choices[1]);
    ASSERT_EQ(1U, simulcast.sendVersions[1].choices.size());
    ASSERT_EQ("101", simulcast.sendVersions[1].choices[0]);
    ASSERT_EQ(2U, simulcast.sendVersions[2].choices.size());
    ASSERT_EQ("97", simulcast.sendVersions[2].choices[0]);
    ASSERT_EQ("98", simulcast.sendVersions[2].choices[1]);

    ASSERT_EQ(2U, simulcast.recvVersions.size());
    ASSERT_EQ(SdpSimulcastAttribute::Versions::kPt,
              simulcast.recvVersions.type);
    ASSERT_EQ(2U, simulcast.recvVersions[0].choices.size());
    ASSERT_EQ("101", simulcast.recvVersions[0].choices[0]);
    ASSERT_EQ("120", simulcast.recvVersions[0].choices[1]);
    ASSERT_EQ(1U, simulcast.recvVersions[1].choices.size());
    ASSERT_EQ("97", simulcast.recvVersions[1].choices[0]);
  }
}

TEST(NewSdpTestNoFixture, CheckSimulcastInvalidParse)
{
  ParseInvalid<SdpSimulcastAttribute>("", 0);
  ParseInvalid<SdpSimulcastAttribute>(" ", 1);
  ParseInvalid<SdpSimulcastAttribute>("vcer ", 4);
  ParseInvalid<SdpSimulcastAttribute>(" send x", 7);
  ParseInvalid<SdpSimulcastAttribute>(" recv x", 7);
  ParseInvalid<SdpSimulcastAttribute>(" send pt=8 send ", 15);
  ParseInvalid<SdpSimulcastAttribute>(" recv pt=8 recv ", 15);
}

static SdpRidAttributeList::Rid
ParseRid(const std::string& input)
{
  std::istringstream is(input);
  std::string error;
  SdpRidAttributeList::Rid rid;
  EXPECT_TRUE(rid.Parse(is, &error)) << error;
  EXPECT_TRUE(is.eof());
  return rid;
}

TEST(NewSdpTestNoFixture, CheckRidValidParse)
{
  {
    SdpRidAttributeList::Rid rid(ParseRid("1 send"));
    ASSERT_EQ("1", rid.id);
    ASSERT_EQ(sdp::kSend, rid.direction);
    ASSERT_EQ(0U, rid.formats.size());
    ASSERT_EQ(0U, rid.constraints.maxWidth);
    ASSERT_EQ(0U, rid.constraints.maxHeight);
    ASSERT_EQ(0U, rid.constraints.maxFps);
    ASSERT_EQ(0U, rid.constraints.maxFs);
    ASSERT_EQ(0U, rid.constraints.maxBr);
    ASSERT_EQ(0U, rid.constraints.maxPps);
    ASSERT_EQ(0U, rid.dependIds.size());
  }

  {
    SdpRidAttributeList::Rid rid(ParseRid("1 send pt=96;max-width=800"));
    ASSERT_EQ("1", rid.id);
    ASSERT_EQ(sdp::kSend, rid.direction);
    ASSERT_EQ(1U, rid.formats.size());
    ASSERT_EQ(96U, rid.formats[0]);
    ASSERT_EQ(800U, rid.constraints.maxWidth);
    ASSERT_EQ(0U, rid.constraints.maxHeight);
    ASSERT_EQ(0U, rid.constraints.maxFps);
    ASSERT_EQ(0U, rid.constraints.maxFs);
    ASSERT_EQ(0U, rid.constraints.maxBr);
    ASSERT_EQ(0U, rid.constraints.maxPps);
    ASSERT_EQ(0U, rid.dependIds.size());
  }

  {
    SdpRidAttributeList::Rid rid(ParseRid("1 send pt=96,97,98;max-width=800"));
    ASSERT_EQ("1", rid.id);
    ASSERT_EQ(sdp::kSend, rid.direction);
    ASSERT_EQ(3U, rid.formats.size());
    ASSERT_EQ(96U, rid.formats[0]);
    ASSERT_EQ(97U, rid.formats[1]);
    ASSERT_EQ(98U, rid.formats[2]);
    ASSERT_EQ(800U, rid.constraints.maxWidth);
    ASSERT_EQ(0U, rid.constraints.maxHeight);
    ASSERT_EQ(0U, rid.constraints.maxFps);
    ASSERT_EQ(0U, rid.constraints.maxFs);
    ASSERT_EQ(0U, rid.constraints.maxBr);
    ASSERT_EQ(0U, rid.constraints.maxPps);
    ASSERT_EQ(0U, rid.dependIds.size());
  }

  {
    SdpRidAttributeList::Rid rid(
        ParseRid("0123456789az-_ recv max-width=800"));
    ASSERT_EQ("0123456789az-_", rid.id);
    ASSERT_EQ(sdp::kRecv, rid.direction);
    ASSERT_EQ(0U, rid.formats.size());
    ASSERT_EQ(800U, rid.constraints.maxWidth);
    ASSERT_EQ(0U, rid.constraints.maxHeight);
    ASSERT_EQ(0U, rid.constraints.maxFps);
    ASSERT_EQ(0U, rid.constraints.maxFs);
    ASSERT_EQ(0U, rid.constraints.maxBr);
    ASSERT_EQ(0U, rid.constraints.maxPps);
    ASSERT_EQ(0U, rid.dependIds.size());
  }

  {
    SdpRidAttributeList::Rid rid(
        ParseRid("foo send"));
    ASSERT_EQ(0U, rid.formats.size());
    ASSERT_EQ(0U, rid.constraints.maxWidth);
    ASSERT_EQ(0U, rid.constraints.maxHeight);
    ASSERT_EQ(0U, rid.constraints.maxFps);
    ASSERT_EQ(0U, rid.constraints.maxFs);
    ASSERT_EQ(0U, rid.constraints.maxBr);
    ASSERT_EQ(0U, rid.constraints.maxPps);
    ASSERT_EQ(0U, rid.dependIds.size());
  }

  {
    SdpRidAttributeList::Rid rid(
        ParseRid("foo send pt=96"));
    ASSERT_EQ(1U, rid.formats.size());
    ASSERT_EQ(96U, rid.formats[0]);
    ASSERT_EQ(0U, rid.constraints.maxWidth);
    ASSERT_EQ(0U, rid.constraints.maxHeight);
    ASSERT_EQ(0U, rid.constraints.maxFps);
    ASSERT_EQ(0U, rid.constraints.maxFs);
    ASSERT_EQ(0U, rid.constraints.maxBr);
    ASSERT_EQ(0U, rid.constraints.maxPps);
    ASSERT_EQ(0U, rid.dependIds.size());
  }

  // This is not technically permitted by the BNF, but the parse code is simpler
  // if we allow it. If we decide to stop allowing this, this will need to be
  // converted to an invalid parse test-case.
  {
    SdpRidAttributeList::Rid rid(
        ParseRid("foo send max-br=30000;pt=96"));
    ASSERT_EQ(1U, rid.formats.size());
    ASSERT_EQ(96U, rid.formats[0]);
    ASSERT_EQ(0U, rid.constraints.maxWidth);
    ASSERT_EQ(0U, rid.constraints.maxHeight);
    ASSERT_EQ(0U, rid.constraints.maxFps);
    ASSERT_EQ(0U, rid.constraints.maxFs);
    ASSERT_EQ(30000U, rid.constraints.maxBr);
    ASSERT_EQ(0U, rid.constraints.maxPps);
    ASSERT_EQ(0U, rid.dependIds.size());
  }

  {
    SdpRidAttributeList::Rid rid(
        ParseRid("foo send pt=96,97,98"));
    ASSERT_EQ(3U, rid.formats.size());
    ASSERT_EQ(96U, rid.formats[0]);
    ASSERT_EQ(97U, rid.formats[1]);
    ASSERT_EQ(98U, rid.formats[2]);
    ASSERT_EQ(0U, rid.constraints.maxWidth);
    ASSERT_EQ(0U, rid.constraints.maxHeight);
    ASSERT_EQ(0U, rid.constraints.maxFps);
    ASSERT_EQ(0U, rid.constraints.maxFs);
    ASSERT_EQ(0U, rid.constraints.maxBr);
    ASSERT_EQ(0U, rid.constraints.maxPps);
    ASSERT_EQ(0U, rid.dependIds.size());
  }

  {
    SdpRidAttributeList::Rid rid(
        ParseRid("foo send max-width=800"));
    ASSERT_EQ(0U, rid.formats.size());
    ASSERT_EQ(800U, rid.constraints.maxWidth);
    ASSERT_EQ(0U, rid.constraints.maxHeight);
    ASSERT_EQ(0U, rid.constraints.maxFps);
    ASSERT_EQ(0U, rid.constraints.maxFs);
    ASSERT_EQ(0U, rid.constraints.maxBr);
    ASSERT_EQ(0U, rid.constraints.maxPps);
    ASSERT_EQ(0U, rid.dependIds.size());
  }

  {
    SdpRidAttributeList::Rid rid(
        ParseRid("foo send max-height=640"));
    ASSERT_EQ(0U, rid.formats.size());
    ASSERT_EQ(0U, rid.constraints.maxWidth);
    ASSERT_EQ(640U, rid.constraints.maxHeight);
    ASSERT_EQ(0U, rid.constraints.maxFps);
    ASSERT_EQ(0U, rid.constraints.maxFs);
    ASSERT_EQ(0U, rid.constraints.maxBr);
    ASSERT_EQ(0U, rid.constraints.maxPps);
    ASSERT_EQ(0U, rid.dependIds.size());
  }

  {
    SdpRidAttributeList::Rid rid(
        ParseRid("foo send max-fps=30"));
    ASSERT_EQ(0U, rid.formats.size());
    ASSERT_EQ(0U, rid.constraints.maxWidth);
    ASSERT_EQ(0U, rid.constraints.maxHeight);
    ASSERT_EQ(30U, rid.constraints.maxFps);
    ASSERT_EQ(0U, rid.constraints.maxFs);
    ASSERT_EQ(0U, rid.constraints.maxBr);
    ASSERT_EQ(0U, rid.constraints.maxPps);
    ASSERT_EQ(0U, rid.dependIds.size());
  }

  {
    SdpRidAttributeList::Rid rid(
        ParseRid("foo send max-fs=3600"));
    ASSERT_EQ(0U, rid.formats.size());
    ASSERT_EQ(0U, rid.constraints.maxWidth);
    ASSERT_EQ(0U, rid.constraints.maxHeight);
    ASSERT_EQ(0U, rid.constraints.maxFps);
    ASSERT_EQ(3600U, rid.constraints.maxFs);
    ASSERT_EQ(0U, rid.constraints.maxBr);
    ASSERT_EQ(0U, rid.constraints.maxPps);
    ASSERT_EQ(0U, rid.dependIds.size());
  }

  {
    SdpRidAttributeList::Rid rid(
        ParseRid("foo send max-br=30000"));
    ASSERT_EQ(0U, rid.formats.size());
    ASSERT_EQ(0U, rid.constraints.maxWidth);
    ASSERT_EQ(0U, rid.constraints.maxHeight);
    ASSERT_EQ(0U, rid.constraints.maxFps);
    ASSERT_EQ(0U, rid.constraints.maxFs);
    ASSERT_EQ(30000U, rid.constraints.maxBr);
    ASSERT_EQ(0U, rid.constraints.maxPps);
    ASSERT_EQ(0U, rid.dependIds.size());
  }

  {
    SdpRidAttributeList::Rid rid(
        ParseRid("foo send max-pps=9216000"));
    ASSERT_EQ(0U, rid.formats.size());
    ASSERT_EQ(0U, rid.constraints.maxWidth);
    ASSERT_EQ(0U, rid.constraints.maxHeight);
    ASSERT_EQ(0U, rid.constraints.maxFps);
    ASSERT_EQ(0U, rid.constraints.maxFs);
    ASSERT_EQ(0U, rid.constraints.maxBr);
    ASSERT_EQ(9216000U, rid.constraints.maxPps);
    ASSERT_EQ(0U, rid.dependIds.size());
  }

  {
    SdpRidAttributeList::Rid rid(
        ParseRid("foo send depend=foo"));
    ASSERT_EQ(0U, rid.formats.size());
    ASSERT_EQ(0U, rid.constraints.maxWidth);
    ASSERT_EQ(0U, rid.constraints.maxHeight);
    ASSERT_EQ(0U, rid.constraints.maxFps);
    ASSERT_EQ(0U, rid.constraints.maxFs);
    ASSERT_EQ(0U, rid.constraints.maxBr);
    ASSERT_EQ(0U, rid.constraints.maxPps);
    ASSERT_EQ(1U, rid.dependIds.size());
    ASSERT_EQ("foo", rid.dependIds[0]);
  }

  {
    SdpRidAttributeList::Rid rid(
        ParseRid("foo send max-foo=20"));
    ASSERT_EQ(0U, rid.formats.size());
    ASSERT_EQ(0U, rid.constraints.maxWidth);
    ASSERT_EQ(0U, rid.constraints.maxHeight);
    ASSERT_EQ(0U, rid.constraints.maxFps);
    ASSERT_EQ(0U, rid.constraints.maxFs);
    ASSERT_EQ(0U, rid.constraints.maxBr);
    ASSERT_EQ(0U, rid.constraints.maxPps);
    ASSERT_EQ(0U, rid.dependIds.size());
  }

  {
    SdpRidAttributeList::Rid rid(
        ParseRid("foo send depend=foo,bar"));
    ASSERT_EQ(0U, rid.formats.size());
    ASSERT_EQ(0U, rid.constraints.maxWidth);
    ASSERT_EQ(0U, rid.constraints.maxHeight);
    ASSERT_EQ(0U, rid.constraints.maxFps);
    ASSERT_EQ(0U, rid.constraints.maxFs);
    ASSERT_EQ(0U, rid.constraints.maxBr);
    ASSERT_EQ(0U, rid.constraints.maxPps);
    ASSERT_EQ(2U, rid.dependIds.size());
    ASSERT_EQ("foo", rid.dependIds[0]);
    ASSERT_EQ("bar", rid.dependIds[1]);
  }

  {
    SdpRidAttributeList::Rid rid(
        ParseRid("foo send max-width=800;max-height=600"));
    ASSERT_EQ(0U, rid.formats.size());
    ASSERT_EQ(800U, rid.constraints.maxWidth);
    ASSERT_EQ(600U, rid.constraints.maxHeight);
    ASSERT_EQ(0U, rid.constraints.maxFps);
    ASSERT_EQ(0U, rid.constraints.maxFs);
    ASSERT_EQ(0U, rid.constraints.maxBr);
    ASSERT_EQ(0U, rid.constraints.maxPps);
    ASSERT_EQ(0U, rid.dependIds.size());
  }

  {
    SdpRidAttributeList::Rid rid(
        ParseRid("foo send pt=96,97;max-width=800;max-height=600"));
    ASSERT_EQ(2U, rid.formats.size());
    ASSERT_EQ(96U, rid.formats[0]);
    ASSERT_EQ(97U, rid.formats[1]);
    ASSERT_EQ(800U, rid.constraints.maxWidth);
    ASSERT_EQ(600U, rid.constraints.maxHeight);
    ASSERT_EQ(0U, rid.constraints.maxFps);
    ASSERT_EQ(0U, rid.constraints.maxFs);
    ASSERT_EQ(0U, rid.constraints.maxBr);
    ASSERT_EQ(0U, rid.constraints.maxPps);
    ASSERT_EQ(0U, rid.dependIds.size());
  }

  {
    SdpRidAttributeList::Rid rid(
        ParseRid("foo send depend=foo,bar;max-width=800;max-height=600"));
    ASSERT_EQ(0U, rid.formats.size());
    ASSERT_EQ(800U, rid.constraints.maxWidth);
    ASSERT_EQ(600U, rid.constraints.maxHeight);
    ASSERT_EQ(0U, rid.constraints.maxFps);
    ASSERT_EQ(0U, rid.constraints.maxFs);
    ASSERT_EQ(0U, rid.constraints.maxBr);
    ASSERT_EQ(0U, rid.constraints.maxPps);
    ASSERT_EQ(2U, rid.dependIds.size());
    ASSERT_EQ("foo", rid.dependIds[0]);
    ASSERT_EQ("bar", rid.dependIds[1]);
  }

  {
    SdpRidAttributeList::Rid rid(
        ParseRid("foo send max-foo=20;max-width=800;max-height=600"));
    ASSERT_EQ(0U, rid.formats.size());
    ASSERT_EQ(800U, rid.constraints.maxWidth);
    ASSERT_EQ(600U, rid.constraints.maxHeight);
    ASSERT_EQ(0U, rid.constraints.maxFps);
    ASSERT_EQ(0U, rid.constraints.maxFs);
    ASSERT_EQ(0U, rid.constraints.maxBr);
    ASSERT_EQ(0U, rid.constraints.maxPps);
    ASSERT_EQ(0U, rid.dependIds.size());
  }
}

TEST(NewSdpTestNoFixture, CheckRidInvalidParse)
{
  ParseInvalid<SdpRidAttributeList::Rid>("", 0);
  ParseInvalid<SdpRidAttributeList::Rid>(" ", 0);
  ParseInvalid<SdpRidAttributeList::Rid>("foo", 3);
  ParseInvalid<SdpRidAttributeList::Rid>("foo ", 4);
  ParseInvalid<SdpRidAttributeList::Rid>("foo  ", 5);
  ParseInvalid<SdpRidAttributeList::Rid>("foo bar", 7);
  ParseInvalid<SdpRidAttributeList::Rid>("foo recv ", 9);
  ParseInvalid<SdpRidAttributeList::Rid>("foo recv pt=", 12);
  ParseInvalid<SdpRidAttributeList::Rid>(" ", 0);
  ParseInvalid<SdpRidAttributeList::Rid>("foo send pt", 11);
  ParseInvalid<SdpRidAttributeList::Rid>("foo send pt=", 12);
  ParseInvalid<SdpRidAttributeList::Rid>("foo send pt=x", 12);
  ParseInvalid<SdpRidAttributeList::Rid>("foo send pt=-1", 12);
  ParseInvalid<SdpRidAttributeList::Rid>("foo send pt=96,", 15);
  ParseInvalid<SdpRidAttributeList::Rid>("foo send pt=196", 15);
  ParseInvalid<SdpRidAttributeList::Rid>("foo send max-width", 18);
  ParseInvalid<SdpRidAttributeList::Rid>("foo send max-width=", 19);
  ParseInvalid<SdpRidAttributeList::Rid>("foo send max-width=x", 19);
  ParseInvalid<SdpRidAttributeList::Rid>("foo send max-width=-1", 19);
  ParseInvalid<SdpRidAttributeList::Rid>("foo send max-width=800;", 23);
  ParseInvalid<SdpRidAttributeList::Rid>("foo send max-width=800; ", 24);
  ParseInvalid<SdpRidAttributeList::Rid>("foo send depend=",16);
  ParseInvalid<SdpRidAttributeList::Rid>("foo send depend=,", 16);
  ParseInvalid<SdpRidAttributeList::Rid>("foo send depend=1,", 18);
}

TEST(NewSdpTestNoFixture, CheckRidSerialize)
{
  {
    SdpRidAttributeList::Rid rid;
    rid.id = "foo";
    rid.direction = sdp::kSend;
    std::ostringstream os;
    rid.Serialize(os);
    ASSERT_EQ("foo send", os.str());
  }

  {
    SdpRidAttributeList::Rid rid;
    rid.id = "foo";
    rid.direction = sdp::kSend;
    std::ostringstream os;
    rid.Serialize(os);
    ASSERT_EQ("foo send", os.str());
  }

  {
    SdpRidAttributeList::Rid rid;
    rid.id = "foo";
    rid.direction = sdp::kSend;
    rid.formats.push_back(96);
    std::ostringstream os;
    rid.Serialize(os);
    ASSERT_EQ("foo send pt=96", os.str());
  }

  {
    SdpRidAttributeList::Rid rid;
    rid.id = "foo";
    rid.direction = sdp::kSend;
    rid.formats.push_back(96);
    rid.formats.push_back(97);
    std::ostringstream os;
    rid.Serialize(os);
    ASSERT_EQ("foo send pt=96,97", os.str());
  }

  {
    SdpRidAttributeList::Rid rid;
    rid.id = "foo";
    rid.direction = sdp::kSend;
    rid.constraints.maxWidth = 800;
    std::ostringstream os;
    rid.Serialize(os);
    ASSERT_EQ("foo send max-width=800", os.str());
  }

  {
    SdpRidAttributeList::Rid rid;
    rid.id = "foo";
    rid.direction = sdp::kSend;
    rid.constraints.maxHeight = 600;
    std::ostringstream os;
    rid.Serialize(os);
    ASSERT_EQ("foo send max-height=600", os.str());
  }

  {
    SdpRidAttributeList::Rid rid;
    rid.id = "foo";
    rid.direction = sdp::kSend;
    rid.constraints.maxFps = 30;
    std::ostringstream os;
    rid.Serialize(os);
    ASSERT_EQ("foo send max-fps=30", os.str());
  }

  {
    SdpRidAttributeList::Rid rid;
    rid.id = "foo";
    rid.direction = sdp::kSend;
    rid.constraints.maxFs = 3600;
    std::ostringstream os;
    rid.Serialize(os);
    ASSERT_EQ("foo send max-fs=3600", os.str());
  }

  {
    SdpRidAttributeList::Rid rid;
    rid.id = "foo";
    rid.direction = sdp::kSend;
    rid.constraints.maxBr = 30000;
    std::ostringstream os;
    rid.Serialize(os);
    ASSERT_EQ("foo send max-br=30000", os.str());
  }

  {
    SdpRidAttributeList::Rid rid;
    rid.id = "foo";
    rid.direction = sdp::kSend;
    rid.constraints.maxPps = 9216000;
    std::ostringstream os;
    rid.Serialize(os);
    ASSERT_EQ("foo send max-pps=9216000", os.str());
  }

  {
    SdpRidAttributeList::Rid rid;
    rid.id = "foo";
    rid.direction = sdp::kSend;
    rid.dependIds.push_back("foo");
    std::ostringstream os;
    rid.Serialize(os);
    ASSERT_EQ("foo send depend=foo", os.str());
  }

  {
    SdpRidAttributeList::Rid rid;
    rid.id = "foo";
    rid.direction = sdp::kSend;
    rid.dependIds.push_back("foo");
    rid.dependIds.push_back("bar");
    std::ostringstream os;
    rid.Serialize(os);
    ASSERT_EQ("foo send depend=foo,bar", os.str());
  }

  {
    SdpRidAttributeList::Rid rid;
    rid.id = "foo";
    rid.direction = sdp::kSend;
    rid.formats.push_back(96);
    rid.constraints.maxBr = 30000;
    std::ostringstream os;
    rid.Serialize(os);
    ASSERT_EQ("foo send pt=96;max-br=30000", os.str());
  }
}

} // End namespace test.

int main(int argc, char **argv) {
  ScopedXPCOM xpcom("sdp_unittests");

  test_utils = new MtransportTestUtils();
  NSS_NoDB_Init(nullptr);
  NSS_SetDomesticPolicy();

  ::testing::InitGoogleTest(&argc, argv);
  int result = RUN_ALL_TESTS();

  PeerConnectionCtx::Destroy();
  delete test_utils;

  return result;
}
