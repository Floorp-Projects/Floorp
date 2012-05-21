/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The multiplex stream concatenates a list of input streams into a single
 * stream.
 */

#ifndef _nsMultiplexInputStream_h_
#define _nsMultiplexInputStream_h_

#include "nsIMultiplexInputStream.h"

#define NS_MULTIPLEXINPUTSTREAM_CLASSNAME  "nsMultiplexInputStream"
#define NS_MULTIPLEXINPUTSTREAM_CONTRACTID "@mozilla.org/io/multiplex-input-stream;1"
#define NS_MULTIPLEXINPUTSTREAM_CID                  \
{ /* 565e3a2c-1dd2-11b2-8da1-b4cef17e568d */         \
    0x565e3a2c,                                      \
    0x1dd2,                                          \
    0x11b2,                                          \
    {0x8d, 0xa1, 0xb4, 0xce, 0xf1, 0x7e, 0x56, 0x8d} \
}

extern nsresult nsMultiplexInputStreamConstructor(nsISupports *outer,
                                                  REFNSIID iid,
                                                  void **result);

#endif //  _nsMultiplexInputStream_h_
