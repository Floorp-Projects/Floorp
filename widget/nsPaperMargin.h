/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPaperMargin_h_
#define nsPaperMargin_h_

#include "nsISupportsImpl.h"
#include "nsIPaperMargin.h"
#include "mozilla/gfx/Rect.h"

class nsPaperMargin final : public nsIPaperMargin {
  using MarginDouble = mozilla::gfx::MarginDouble;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAPERMARGIN

  nsPaperMargin() = delete;

  explicit nsPaperMargin(const MarginDouble& aMargin) : mMargin(aMargin) {}

 private:
  ~nsPaperMargin() = default;
  const MarginDouble mMargin;
};

#endif
