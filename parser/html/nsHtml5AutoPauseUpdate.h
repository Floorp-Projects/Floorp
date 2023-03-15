/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5AutoPauseUpdate_h
#define nsHtml5AutoPauseUpdate_h

#include "nsHtml5DocumentBuilder.h"

class MOZ_RAII nsHtml5AutoPauseUpdate final {
 private:
  RefPtr<nsHtml5DocumentBuilder> mBuilder;

 public:
  explicit nsHtml5AutoPauseUpdate(nsHtml5DocumentBuilder* aBuilder)
      : mBuilder(aBuilder) {
    mBuilder->EndDocUpdate();
  }
  ~nsHtml5AutoPauseUpdate() {
    // Something may have terminated the parser during the update pause.
    if (!mBuilder->IsComplete()) {
      mBuilder->BeginDocUpdate();
    }
  }
};

#endif  // nsHtml5AutoPauseUpdate_h
