/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDebugImpl_h
#define nsDebugImpl_h

#include "nsIDebug2.h"

class nsDebugImpl : public nsIDebug2
{
public:
  nsDebugImpl() = default;
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDEBUG2

  static nsresult Create(nsISupports* aOuter, const nsIID& aIID,
                         void** aInstancePtr);

  /*
   * Inform nsDebugImpl that we're in multiprocess mode.
   *
   * If aDesc is not nullptr, the string it points to must be
   * statically-allocated (i.e., it must be a string literal).
   */
  static void SetMultiprocessMode(const char* aDesc);
};


#define NS_DEBUG_CONTRACTID "@mozilla.org/xpcom/debug;1"
#define NS_DEBUG_CID                                 \
{ /* cb6cdb94-e417-4601-b4a5-f991bf41453d */         \
  0xcb6cdb94,                                        \
    0xe417,                                          \
    0x4601,                                          \
    {0xb4, 0xa5, 0xf9, 0x91, 0xbf, 0x41, 0x45, 0x3d} \
}

#endif // nsDebugImpl_h
