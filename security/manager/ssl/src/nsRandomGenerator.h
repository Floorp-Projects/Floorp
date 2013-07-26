/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSRANDOMGENERATOR_H_
#define _NSRANDOMGENERATOR_H_

#include "nsIRandomGenerator.h"
#include "mozilla/Attributes.h"

#define NS_RANDOMGENERATOR_CID \
  {0xbe65e2b7, 0xfe46, 0x4e0f, {0x88, 0xe0, 0x4b, 0x38, 0x5d, 0xb4, 0xd6, 0x8a}}

#define NS_RANDOMGENERATOR_CONTRACTID \
  "@mozilla.org/security/random-generator;1"

class nsRandomGenerator MOZ_FINAL : public nsIRandomGenerator
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRANDOMGENERATOR
};

#endif // _NSRANDOMGENERATOR_H_
