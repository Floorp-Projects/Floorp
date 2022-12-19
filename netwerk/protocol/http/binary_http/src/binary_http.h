/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _binary_http_h_
#define _binary_http_h_

#include "nsISupportsUtils.h"  // for nsresult, etc.

// {b43b3f73-8160-4ab2-9f5d-4129a9708081}
#define NS_BINARY_HTTP_CID                           \
  {                                                  \
    0xb43b3f73, 0x8160, 0x4ab2, {                    \
      0x9f, 0x5d, 0x41, 0x29, 0xa9, 0x70, 0x80, 0x81 \
    }                                                \
  }

extern "C" {
nsresult binary_http_constructor(REFNSIID iid, void** result);
};

#endif  // _binary_http_h_
