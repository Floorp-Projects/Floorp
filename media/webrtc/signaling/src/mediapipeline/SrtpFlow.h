/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef srtpflow_h__
#define srtpflow_h__

#include "ssl.h"
#include "sslproto.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"

typedef struct srtp_policy_t srtp_policy_t;
typedef struct srtp_ctx_t *srtp_t;
typedef struct srtp_event_data_t srtp_event_data_t;

namespace mozilla {

#define SRTP_MASTER_KEY_LENGTH 16
#define SRTP_MASTER_SALT_LENGTH 14
#define SRTP_TOTAL_KEY_LENGTH (SRTP_MASTER_KEY_LENGTH + SRTP_MASTER_SALT_LENGTH)

// SRTCP requires an auth tag *plus* a 4-byte index-plus-'E'-bit value (see
// RFC 3711)
#define SRTP_MAX_EXPANSION (SRTP_MAX_TRAILER_LEN+4)


class SrtpFlow {
  ~SrtpFlow();
 public:


  static RefPtr<SrtpFlow> Create(int cipher_suite,
                                          bool inbound,
                                          const void *key,
                                          size_t key_len);

  nsresult ProtectRtp(void *in, int in_len,
                      int max_len, int *out_len);
  nsresult UnprotectRtp(void *in, int in_len,
                        int max_len, int *out_len);
  nsresult ProtectRtcp(void *in, int in_len,
                       int max_len, int *out_len);
  nsresult UnprotectRtcp(void *in, int in_len,
                         int max_len, int *out_len);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SrtpFlow)

  static void srtp_event_handler(srtp_event_data_t *data);


 private:
  SrtpFlow() : session_(nullptr) {}

  nsresult CheckInputs(bool protect, void *in, int in_len,
                       int max_len, int *out_len);

  static nsresult Init();
  static bool initialized;  // Was libsrtp initialized? Only happens once.

  srtp_t session_;
};

}  // End of namespace
#endif

