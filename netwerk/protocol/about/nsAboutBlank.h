/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAboutBlank_h__
#define nsAboutBlank_h__

#include "nsIAboutModule.h"

class nsAboutBlank : public nsIAboutModule {
 public:
  NS_DECL_ISUPPORTS

  NS_DECL_NSIABOUTMODULE

  nsAboutBlank() = default;

  static nsresult Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

 private:
  virtual ~nsAboutBlank() = default;
};

#define NS_ABOUT_BLANK_MODULE_CID                    \
  { /* 3decd6c8-30ef-11d3-8cd0-0060b0fc14a3 */       \
    0x3decd6c8, 0x30ef, 0x11d3, {                    \
      0x8c, 0xd0, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3 \
    }                                                \
  }

#endif  // nsAboutBlank_h__
