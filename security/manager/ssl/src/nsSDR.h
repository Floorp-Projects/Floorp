/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Terry Hayes <thayes@netscape.com>
 */

#ifndef _NSSDR_H_
#define _NSSDR_H_

#include "nsISecretDecoderRing.h"

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

#define NS_SDR_CLASSNAME "PIPNSS Secret Decoder Ring"
#define NS_SDR_CID \
  { 0x0c4f1ddc, 0x1dd2, 0x11b2, { 0x9d, 0x95, 0xf2, 0xfd, 0xf1, 0x13, 0x04, 0x4b } }

class nsSecretDecoderRing
: public nsISecretDecoderRing,
  public nsISecretDecoderRingConfig
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISECRETDECODERRING
  NS_DECL_NSISECRETDECODERRINGCONFIG

  nsSecretDecoderRing();
  virtual ~nsSecretDecoderRing();

private:

  /**
   * encode - encodes binary into BASE64 string.
   * decode - decode BASE64 string into binary.
   */
  nsresult encode(const unsigned char *data, PRInt32 dataLen, char **_retval);
  nsresult decode(const char *data, unsigned char **result, PRInt32 * _retval);

};

#endif /* _NSSDR_H_ */
