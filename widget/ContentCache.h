/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ContentCache_h
#define mozilla_ContentCache_h

#include <stdint.h>

#include "nsString.h"

namespace mozilla {

/**
 * ContentCache stores various information of the child content.  This hides
 * raw information but you can access more useful information with a lot of
 * methods.
 */

class ContentCache final
{
public:
  void Clear();

  void SetText(const nsAString& aText);
  const nsString& Text() const { return mText; }
  uint32_t TextLength() const { return mText.Length(); }

private:
  // Whole text in the target
  nsString mText;
};

} // namespace mozilla

#endif // mozilla_ContentCache_h
