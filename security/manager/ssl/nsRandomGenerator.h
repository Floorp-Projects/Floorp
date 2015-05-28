/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRandomGenerator_h
#define nsRandomGenerator_h

#include "mozilla/Attributes.h"
#include "nsIRandomGenerator.h"
#include "nsNSSShutDown.h"

#define NS_RANDOMGENERATOR_CID \
  {0xbe65e2b7, 0xfe46, 0x4e0f, {0x88, 0xe0, 0x4b, 0x38, 0x5d, 0xb4, 0xd6, 0x8a}}

#define NS_RANDOMGENERATOR_CONTRACTID \
  "@mozilla.org/security/random-generator;1"

class nsRandomGenerator final : public nsIRandomGenerator
                              , public nsNSSShutDownObject
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRANDOMGENERATOR

private:
  ~nsRandomGenerator();
  virtual void virtualDestroyNSSReference() override {}
};

#endif // nsRandomGenerator_h
