/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"

// f18fb09b-28b4-4435-bc5b-8027f18df743
#define NS_TESTING_CID \
{ 0xf18fb09b, 0x28b4, 0x4435, \
  { 0xbc, 0x5b, 0x80, 0x27, 0xf1, 0x8d, 0xf7, 0x43 } }

NS_DEFINE_NAMED_CID(NS_TESTING_CID);

static nsresult
DummyConstructorFunc(nsISupports* aOuter, const nsIID& aIID, void** aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

static const mozilla::Module::CIDEntry kTestCIDs[] = {
  { &kNS_TESTING_CID, false, nullptr, DummyConstructorFunc },
  { nullptr }
};

static const mozilla::Module kTestModule = {
  3, /* faking mozilla::Module::kVersion with a value that will never be used */
  kTestCIDs
};

NSMODULE_DEFN(dummy) = &kTestModule;
