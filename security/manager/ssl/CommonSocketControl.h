/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CommonSocketControl_h
#define CommonSocketControl_h

#include "nsISSLSocketControl.h"
#include "TransportSecurityInfo.h"

class CommonSocketControl : public mozilla::psm::TransportSecurityInfo,
                            public nsISSLSocketControl {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSISSLSOCKETCONTROL

  explicit CommonSocketControl(uint32_t providerFlags);

  uint32_t GetProviderFlags() const { return mProviderFlags; }
  void SetSSLVersionUsed(int16_t version) { mSSLVersionUsed = version; }
  void SetResumed(bool aResumed) { mResumed = aResumed; }

 protected:
  ~CommonSocketControl() = default;
  nsCString mNegotiatedNPN;
  bool mNPNCompleted;
  bool mHandshakeCompleted;
  bool mJoined;
  bool mSentClientCert;
  bool mFailedVerification;
  mozilla::Atomic<bool, mozilla::Relaxed> mResumed;
  uint16_t mSSLVersionUsed;
  uint32_t mProviderFlags;
};

#endif  // CommonSocketControl_h
