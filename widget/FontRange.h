/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_FontRange_h_
#define mozilla_FontRange_h_

namespace mozilla {

struct FontRange
{
  FontRange()
    : mStartOffset(0)
    , mFontSize(0)
  {
  }

  int32_t mStartOffset;
  nsString mFontName;
  gfxFloat mFontSize; // in device pixels
};

}

#endif // mozilla_FontRange_h_
