/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GeckoViewContentChannel_h__
#define GeckoViewContentChannel_h__

#include "nsBaseChannel.h"

class GeckoViewContentChannel final : public nsBaseChannel {
 public:
  explicit GeckoViewContentChannel(nsIURI* aUri);

 private:
  ~GeckoViewContentChannel() = default;

  nsresult OpenContentStream(bool async, nsIInputStream** result,
                             nsIChannel** channel) override;
};

#endif  // !GeckoViewContentChannel_h__
