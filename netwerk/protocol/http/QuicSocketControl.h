/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QuicSocketControl_h
#define QuicSocketControl_h

#include "CommonSocketControl.h"
#include "nsIWeakReferenceUtils.h"

namespace mozilla {
namespace net {

class Http3Session;

// IID for the QuicSocketControl interface
#define NS_QUICSOCKETCONTROL_IID                     \
  {                                                  \
    0xdbc67fd0, 0x1ac6, 0x457b, {                    \
      0x91, 0x4e, 0x4c, 0x86, 0x60, 0xff, 0x00, 0x69 \
    }                                                \
  }

class QuicSocketControl final : public CommonSocketControl {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_QUICSOCKETCONTROL_IID)

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD GetSSLVersionOffered(int16_t* aSSLVersionOffered) override;

  explicit QuicSocketControl(uint32_t providerFlags,
                             Http3Session* aHttp3Session);

  void SetNegotiatedNPN(const nsACString& aValue);
  void SetInfo(uint16_t aCipherSuite, uint16_t aProtocolVersion,
               uint16_t aKeaGroup, uint16_t aSignatureScheme,
               bool aEchAccepted);

  void CallAuthenticated();

  void HandshakeCompleted();
  void SetCertVerificationResult(PRErrorCode errorCode) override;

  NS_IMETHOD GetPeerId(nsACString& aResult) override;

  NS_IMETHOD GetEchConfig(nsACString& aEchConfig) override;
  NS_IMETHOD SetEchConfig(const nsACString& aEchConfig) override;
  NS_IMETHOD GetRetryEchConfig(nsACString& aEchConfig) override;
  void SetRetryEchConfig(const nsACString& aEchConfig);

 private:
  ~QuicSocketControl();

  // For Authentication done callback and echConfig.
  nsWeakPtr mHttp3Session;
  nsCOMPtr<nsIEventTarget> mSocketThread;

  nsCString mEchConfig;
  nsCString mRetryEchConfig;
};

NS_DEFINE_STATIC_IID_ACCESSOR(QuicSocketControl, NS_QUICSOCKETCONTROL_IID)

}  // namespace net
}  // namespace mozilla

#endif  // QuicSocketControl_h
