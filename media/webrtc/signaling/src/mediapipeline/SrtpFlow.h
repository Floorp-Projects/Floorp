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

// For some reason libsrtp increases packet size by > 12 for RTCP even though
// the doc claims otherwise.
#define SRTP_MAX_EXPANSION 20


class SrtpFlow {
 public:
  ~SrtpFlow();


  static mozilla::RefPtr<SrtpFlow> Create(int cipher_suite,
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
  SrtpFlow() : policy_(NULL), session_(NULL) {}

  nsresult CheckInputs(bool protect, void *in, int in_len,
                       int max_len, int *out_len);

  static nsresult Init();
  static bool initialized;  // Was SRTP initialized?

  srtp_policy_t *policy_;
  srtp_t session_;
};

}  // End of namespace
#endif

