/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ContentSignatureVerifier_h
#define ContentSignatureVerifier_h

#include "nsIContentSignatureVerifier.h"
#include "nsString.h"

// 45a5fe2f-c350-4b86-962d-02d5aaaa955a
#define NS_CONTENTSIGNATUREVERIFIER_CID              \
  {                                                  \
    0x45a5fe2f, 0xc350, 0x4b86, {                    \
      0x96, 0x2d, 0x02, 0xd5, 0xaa, 0xaa, 0x95, 0x5a \
    }                                                \
  }
#define NS_CONTENTSIGNATUREVERIFIER_CONTRACTID \
  "@mozilla.org/security/contentsignatureverifier;1"

class ContentSignatureVerifier final : public nsIContentSignatureVerifier {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTSIGNATUREVERIFIER

 private:
  ~ContentSignatureVerifier() = default;
};

#endif  // ContentSignatureVerifier_h
