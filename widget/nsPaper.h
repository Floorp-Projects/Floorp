/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPaper_h__
#define nsPaper_h__

#include "nsIPaper.h"
#include "nsISupportsImpl.h"
#include "nsString.h"

class nsPaper final : public nsIPaper {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAPER
  nsPaper() = delete;
  nsPaper(const nsAString& aName, double aWidth, double aHeight)
      : mName(aName), mWidth(aWidth), mHeight(aHeight) {}

 private:
  ~nsPaper() = default;

  nsString mName;
  double mWidth;
  double mHeight;
};

#endif /* nsPaper_h__ */
