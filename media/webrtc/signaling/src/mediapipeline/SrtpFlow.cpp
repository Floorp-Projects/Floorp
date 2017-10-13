/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include "logging.h"
#include "SrtpFlow.h"

#include "srtp.h"
#include "ssl.h"
#include "sslproto.h"

#include "mozilla/RefPtr.h"

static const char* sfLogTag = "SrtpFlow";
#ifdef LOGTAG
#undef LOGTAG
#endif
#define LOGTAG sfLogTag

using namespace mozilla;

namespace mozilla {

bool SrtpFlow::initialized;  // Static

SrtpFlow::~SrtpFlow() {
  if (session_) {
    srtp_dealloc(session_);
  }
}

RefPtr<SrtpFlow> SrtpFlow::Create(int cipher_suite,
                                           bool inbound,
                                           const void *key,
                                           size_t key_len) {
  nsresult res = Init();
  if (!NS_SUCCEEDED(res))
    return nullptr;

  RefPtr<SrtpFlow> flow = new SrtpFlow();

  if (!key) {
    CSFLogError(LOGTAG, "Null SRTP key specified");
    return nullptr;
  }

  if (key_len != SRTP_TOTAL_KEY_LENGTH) {
    CSFLogError(LOGTAG, "Invalid SRTP key length");
    return nullptr;
  }

  srtp_policy_t policy;
  memset(&policy, 0, sizeof(srtp_policy_t));

  // Note that we set the same cipher suite for RTP and RTCP
  // since any flow can only have one cipher suite with DTLS-SRTP
  switch (cipher_suite) {
    case SRTP_AES128_CM_HMAC_SHA1_80:
      CSFLogDebug(LOGTAG,
                  "Setting SRTP cipher suite SRTP_AES128_CM_HMAC_SHA1_80");
      crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
      crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
      break;
    case SRTP_AES128_CM_HMAC_SHA1_32:
      CSFLogDebug(LOGTAG,
                  "Setting SRTP cipher suite SRTP_AES128_CM_HMAC_SHA1_32");
      crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
      crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp); // 80-bit per RFC 5764
      break;                                                   // S 4.1.2.
    default:
      CSFLogError(LOGTAG, "Request to set unknown SRTP cipher suite");
      return nullptr;
  }
  // This key is copied into the srtp_t object, so we don't
  // need to keep it.
  policy.key = const_cast<unsigned char *>(
      static_cast<const unsigned char *>(key));
  policy.ssrc.type = inbound ? ssrc_any_inbound : ssrc_any_outbound;
  policy.ssrc.value = 0;
  policy.ekt = nullptr;
  policy.window_size = 1024;   // Use the Chrome value.  Needs to be revisited.  Default is 128
  policy.allow_repeat_tx = 1;  // Use Chrome value; needed for NACK mode to work
  policy.next = nullptr;

  // Now make the session
  err_status_t r = srtp_create(&flow->session_, &policy);
  if (r != err_status_ok) {
    CSFLogError(LOGTAG, "Error creating srtp session");
    return nullptr;
  }

  return flow;
}


nsresult SrtpFlow::CheckInputs(bool protect, void *in, int in_len,
                               int max_len, int *out_len) {
  MOZ_ASSERT(in);
  if (!in) {
    CSFLogError(LOGTAG, "NULL input value");
    return NS_ERROR_NULL_POINTER;
  }

  if (in_len < 0) {
    CSFLogError(LOGTAG, "Input length is negative");
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (max_len < 0) {
    CSFLogError(LOGTAG, "Max output length is negative");
    return NS_ERROR_ILLEGAL_VALUE;
  }

  if (protect) {
    if ((max_len < SRTP_MAX_EXPANSION) ||
        ((max_len - SRTP_MAX_EXPANSION) < in_len)) {
      CSFLogError(LOGTAG, "Output too short");
      return NS_ERROR_ILLEGAL_VALUE;
    }
  }
  else {
    if (in_len > max_len) {
      CSFLogError(LOGTAG, "Output too short");
      return NS_ERROR_ILLEGAL_VALUE;
    }
  }

  return NS_OK;
}

nsresult SrtpFlow::ProtectRtp(void *in, int in_len,
                              int max_len, int *out_len) {
  nsresult res = CheckInputs(true, in, in_len, max_len, out_len);
  if (NS_FAILED(res))
    return res;

  int len = in_len;
  err_status_t r = srtp_protect(session_, in, &len);

  if (r != err_status_ok) {
    CSFLogError(LOGTAG, "Error protecting SRTP packet");
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(len <= max_len);
  *out_len = len;


  CSFLogDebug(LOGTAG, "Successfully protected an SRTP packet of len %d",
              *out_len);

  return NS_OK;
}

nsresult SrtpFlow::UnprotectRtp(void *in, int in_len,
                                int max_len, int *out_len) {
  nsresult res = CheckInputs(false, in, in_len, max_len, out_len);
  if (NS_FAILED(res))
    return res;

  int len = in_len;
  err_status_t r = srtp_unprotect(session_, in, &len);

  if (r != err_status_ok) {
    CSFLogError(LOGTAG, "Error unprotecting SRTP packet error=%d", (int)r);
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(len <= max_len);
  *out_len = len;

  CSFLogDebug(LOGTAG, "Successfully unprotected an SRTP packet of len %d",
              *out_len);

  return NS_OK;
}

nsresult SrtpFlow::ProtectRtcp(void *in, int in_len,
                               int max_len, int *out_len) {
  nsresult res = CheckInputs(true, in, in_len, max_len, out_len);
  if (NS_FAILED(res))
    return res;

  int len = in_len;
  err_status_t r = srtp_protect_rtcp(session_, in, &len);

  if (r != err_status_ok) {
    CSFLogError(LOGTAG, "Error protecting SRTCP packet");
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(len <= max_len);
  *out_len = len;

  CSFLogDebug(LOGTAG, "Successfully protected an SRTCP packet of len %d",
              *out_len);

  return NS_OK;
}

nsresult SrtpFlow::UnprotectRtcp(void *in, int in_len,
                                 int max_len, int *out_len) {
  nsresult res = CheckInputs(false, in, in_len, max_len, out_len);
  if (NS_FAILED(res))
    return res;

  int len = in_len;
  err_status_t r = srtp_unprotect_rtcp(session_, in, &len);

  if (r != err_status_ok) {
    CSFLogError(LOGTAG, "Error unprotecting SRTCP packet error=%d", (int)r);
    return NS_ERROR_FAILURE;
  }

  MOZ_ASSERT(len <= max_len);
  *out_len = len;

  CSFLogDebug(LOGTAG, "Successfully unprotected an SRTCP packet of len %d",
              *out_len);

  return NS_OK;
}

// Statics
void SrtpFlow::srtp_event_handler(srtp_event_data_t *data) {
  // TODO(ekr@rtfm.com): Implement this
  MOZ_CRASH();
}

nsresult SrtpFlow::Init() {
  if (!initialized) {
    err_status_t r = srtp_init();
    if (r != err_status_ok) {
      CSFLogError(LOGTAG, "Could not initialize SRTP");
      MOZ_ASSERT(PR_FALSE);
      return NS_ERROR_FAILURE;
    }

    r = srtp_install_event_handler(&SrtpFlow::srtp_event_handler);
    if (r != err_status_ok) {
      CSFLogError(LOGTAG, "Could not install SRTP event handler");
      MOZ_ASSERT(PR_FALSE);
      return NS_ERROR_FAILURE;
    }

    initialized = true;
  }

  return NS_OK;
}

}  // end of namespace

