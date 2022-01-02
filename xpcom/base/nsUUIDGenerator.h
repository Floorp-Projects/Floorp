/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSUUIDGENERATOR_H_
#define _NSUUIDGENERATOR_H_

#include "nsIUUIDGenerator.h"

class nsUUIDGenerator final : public nsIUUIDGenerator {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  NS_DECL_NSIUUIDGENERATOR

 private:
  ~nsUUIDGenerator();
};

#define NS_UUID_GENERATOR_CONTRACTID "@mozilla.org/uuid-generator;1"
#define NS_UUID_GENERATOR_CID                        \
  {                                                  \
    0x706d36bb, 0xbf79, 0x4293, {                    \
      0x81, 0xf2, 0x8f, 0x68, 0x28, 0xc1, 0x8f, 0x9d \
    }                                                \
  }

#endif /* _NSUUIDGENERATOR_H_ */
