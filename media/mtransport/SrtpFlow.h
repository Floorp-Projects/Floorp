/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#ifndef srtpflow_h__
#define srtpflow_h__

#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"
#include "srtp.h"

namespace mozilla {

#define SRTP_ICM_MASTER_KEY_LENGTH 16
#define SRTP_ICM_MASTER_SALT_LENGTH 14
#define SRTP_ICM_MAX_MASTER_LENGTH \
  (SRTP_ICM_MASTER_KEY_LENGTH + SRTP_ICM_MASTER_SALT_LENGTH)

#define SRTP_GCM_MASTER_KEY_MIN_LENGTH 16
#define SRTP_GCM_MASTER_KEY_MAX_LENGTH 32
#define SRTP_GCM_MASTER_SALT_LENGTH 12

#define SRTP_GCM_MIN_MASTER_LENGTH \
  (SRTP_GCM_MASTER_KEY_MIN_LENGTH + SRTP_GCM_MASTER_SALT_LENGTH)
#define SRTP_GCM_MAX_MASTER_LENGTH \
  (SRTP_GCM_MASTER_KEY_MAX_LENGTH + SRTP_GCM_MASTER_SALT_LENGTH)

#define SRTP_MIN_KEY_LENGTH SRTP_GCM_MIN_MASTER_LENGTH
#define SRTP_MAX_KEY_LENGTH SRTP_GCM_MAX_MASTER_LENGTH

// SRTCP requires an auth tag *plus* a 4-byte index-plus-'E'-bit value (see
// RFC 3711)
#define SRTP_MAX_EXPANSION (SRTP_MAX_TRAILER_LEN + 4)

class SrtpFlow {
  ~SrtpFlow();

 public:
  static unsigned int KeySize(int cipher_suite);
  static unsigned int SaltSize(int cipher_suite);

  static RefPtr<SrtpFlow> Create(int cipher_suite, bool inbound,
                                 const void* key, size_t key_len);

  nsresult ProtectRtp(void* in, int in_len, int max_len, int* out_len);
  nsresult UnprotectRtp(void* in, int in_len, int max_len, int* out_len);
  nsresult ProtectRtcp(void* in, int in_len, int max_len, int* out_len);
  nsresult UnprotectRtcp(void* in, int in_len, int max_len, int* out_len);

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SrtpFlow)

  static void srtp_event_handler(srtp_event_data_t* data);

 private:
  SrtpFlow() : session_(nullptr) {}

  nsresult CheckInputs(bool protect, void* in, int in_len, int max_len,
                       int* out_len);

  static nsresult Init();
  static bool initialized;  // Was libsrtp initialized? Only happens once.

  srtp_t session_;
};

}  // namespace mozilla
#endif
