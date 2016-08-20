/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SecretDecoderRing_h
#define SecretDecoderRing_h

#include "nsISecretDecoderRing.h"
#include "nsNSSShutDown.h"
#include "nsString.h"

#define NS_SECRETDECODERRING_CONTRACTID "@mozilla.org/security/sdr;1"

#define NS_SECRETDECODERRING_CID \
  { 0x0c4f1ddc, 0x1dd2, 0x11b2, { 0x9d, 0x95, 0xf2, 0xfd, 0xf1, 0x13, 0x04, 0x4b } }

class SecretDecoderRing : public nsISecretDecoderRing
                        , public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISECRETDECODERRING

  SecretDecoderRing();

  // Nothing to release.
  virtual void virtualDestroyNSSReference() override {}

protected:
  virtual ~SecretDecoderRing();

private:
  nsresult Encrypt(const nsACString& data, /*out*/ nsACString& result);
  nsresult Decrypt(const nsACString& data, /*out*/ nsACString& result);
};

#endif // SecretDecoderRing_h
