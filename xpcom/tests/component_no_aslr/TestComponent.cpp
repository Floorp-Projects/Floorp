/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"

#define NS_TESTING_CID \
{ 0x335fb596, 0xe52d, 0x418f, \
  { 0xb0, 0x1c, 0x1b, 0xf1, 0x6c, 0xe5, 0xe7, 0xe4 } }

NS_DEFINE_NAMED_CID(NS_TESTING_CID);

static nsresult
DummyConstructorFunc(nsISupports* aOuter, const nsIID& aIID, void** aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

static const mozilla::Module::CIDEntry kTestCIDs[] = {
  { &kNS_TESTING_CID, false, NULL, DummyConstructorFunc },
  { NULL }
};

static const mozilla::Module kTestModule = {
  mozilla::Module::kVersion,
  kTestCIDs
};

NSMODULE_DEFN(dummy) = &kTestModule;

  
