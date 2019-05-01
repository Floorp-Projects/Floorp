/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The MIME stream separates headers and a datastream. It also allows
 * automatic creation of the content-length header.
 */

#ifndef _nsMIMEInputStream_h_
#define _nsMIMEInputStream_h_

#include "nsIMIMEInputStream.h"

#define NS_MIMEINPUTSTREAM_CONTRACTID "@mozilla.org/network/mime-input-stream;1"
#define NS_MIMEINPUTSTREAM_CID                       \
  { /* 58a1c31c-1dd2-11b2-a3f6-d36949d48268 */       \
    0x58a1c31c, 0x1dd2, 0x11b2, {                    \
      0xa3, 0xf6, 0xd3, 0x69, 0x49, 0xd4, 0x82, 0x68 \
    }                                                \
  }

extern nsresult nsMIMEInputStreamConstructor(nsISupports* outer, REFNSIID iid,
                                             void** result);

#endif  // _nsMIMEInputStream_h_
