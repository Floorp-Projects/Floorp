/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GeckoViewContentProtocolHandler_h__
#define GeckoViewContentProtocolHandler_h__

#include "nsIProtocolHandler.h"
#include "nsWeakReference.h"

class nsIURIMutator;

class GeckoViewContentProtocolHandler : public nsIProtocolHandler,
                                        public nsSupportsWeakReference {
  virtual ~GeckoViewContentProtocolHandler() = default;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPROTOCOLHANDLER

  GeckoViewContentProtocolHandler() = default;

  [[nodiscard]] nsresult Init();
};

#endif  // !GeckoViewContentProtocolHandler_h__
