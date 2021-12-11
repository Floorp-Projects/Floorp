/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsDialogParamBlock_h
#define __nsDialogParamBlock_h

#include "nsIDialogParamBlock.h"
#include "nsIMutableArray.h"
#include "nsCOMPtr.h"
#include "nsStringFwd.h"

// {4E4AAE11-8901-46cc-8217-DAD7C5415873}
#define NS_DIALOGPARAMBLOCK_CID                      \
  {                                                  \
    0x4e4aae11, 0x8901, 0x46cc, {                    \
      0x82, 0x17, 0xda, 0xd7, 0xc5, 0x41, 0x58, 0x73 \
    }                                                \
  }

class nsDialogParamBlock : public nsIDialogParamBlock {
 public:
  nsDialogParamBlock();

  NS_DECL_NSIDIALOGPARAMBLOCK
  NS_DECL_ISUPPORTS

 protected:
  virtual ~nsDialogParamBlock();

 private:
  enum { kNumInts = 8, kNumStrings = 16 };

  nsresult InBounds(int32_t aIndex, int32_t aMax) {
    return aIndex >= 0 && aIndex < aMax ? NS_OK : NS_ERROR_ILLEGAL_VALUE;
  }

  int32_t mInt[kNumInts];
  int32_t mNumStrings;
  nsString* mString;
  nsCOMPtr<nsIMutableArray> mObjects;
};

#endif
