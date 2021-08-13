/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5StreamParserReleaser_h
#define nsHtml5StreamParserReleaser_h

#include "nsHtml5StreamParser.h"
#include "nsThreadUtils.h"

class nsHtml5StreamParserReleaser : public mozilla::Runnable {
 private:
  nsHtml5StreamParser* mPtr;

 public:
  explicit nsHtml5StreamParserReleaser(nsHtml5StreamParser* aPtr)
      : mozilla::Runnable("nsHtml5StreamParserReleaser"), mPtr(aPtr) {}
  NS_IMETHOD Run() override {
    mPtr->Release();
    return NS_OK;
  }
};

#endif // nsHtml5StreamParserReleaser_h
