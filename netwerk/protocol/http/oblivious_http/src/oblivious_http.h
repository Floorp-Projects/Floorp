/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _oblivious_http_h_
#define _oblivious_http_h_

#include "nsISupportsUtils.h"  // for nsresult, etc.

// {d581149e-3319-4563-b95e-46c64af5c4e8}
#define NS_OBLIVIOUS_HTTP_CID                        \
  {                                                  \
    0xd581149e, 0x3319, 0x4563, {                    \
      0xb9, 0x5e, 0x46, 0xc6, 0x4a, 0xf5, 0xc4, 0xe8 \
    }                                                \
  }

extern "C" {
nsresult oblivious_http_constructor(REFNSIID iid, void** result);
};

#endif  // _oblivious_http_h_
