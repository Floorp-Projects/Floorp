/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The multiplex stream concatenates a list of input streams into a single
 * stream.
 */

#ifndef _nsMultiplexInputStream_h_
#define _nsMultiplexInputStream_h_

#define NS_MULTIPLEXINPUTSTREAM_CONTRACTID \
  "@mozilla.org/io/multiplex-input-stream;1"
#define NS_MULTIPLEXINPUTSTREAM_CID                  \
  { /* 565e3a2c-1dd2-11b2-8da1-b4cef17e568d */       \
    0x565e3a2c, 0x1dd2, 0x11b2, {                    \
      0x8d, 0xa1, 0xb4, 0xce, 0xf1, 0x7e, 0x56, 0x8d \
    }                                                \
  }

extern nsresult nsMultiplexInputStreamConstructor(nsISupports* aOuter,
                                                  REFNSIID aIID,
                                                  void** aResult);

#endif  //  _nsMultiplexInputStream_h_
