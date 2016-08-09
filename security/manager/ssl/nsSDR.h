/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSSDR_H_
#define _NSSDR_H_

#include "nsISecretDecoderRing.h"
#include "nsNSSShutDown.h"

/**
 * NS_SDR_CONTRACTID - contract id for SDR services.
 *   Implements nsISecretDecoderRing.
 *   Should eventually implement an interface to set window
 *   context and other information. (nsISecretDecoderRingConfig)
 *
 * NOTE: This definition should move to base code.  It
 *   is conditionally defined here until it is moved.
 *   Delete this after defining in the new location.
 */
#ifndef NS_SDR_CONTRACTID
#define NS_SDR_CONTRACTID "@mozilla.org/security/sdr;1"
#endif

// ===============================================
// nsSecretDecoderRing - implementation of nsISecretDecoderRing
// ===============================================

#define NS_SDR_CID \
  { 0x0c4f1ddc, 0x1dd2, 0x11b2, { 0x9d, 0x95, 0xf2, 0xfd, 0xf1, 0x13, 0x04, 0x4b } }

class nsSecretDecoderRing : public nsISecretDecoderRing
                          , public nsISecretDecoderRingConfig
                          , public nsNSSShutDownObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISECRETDECODERRING
  NS_DECL_NSISECRETDECODERRINGCONFIG

  nsSecretDecoderRing();

  // Nothing to release.
  virtual void virtualDestroyNSSReference() override {}

protected:
  virtual ~nsSecretDecoderRing();
};

#endif /* _NSSDR_H_ */
