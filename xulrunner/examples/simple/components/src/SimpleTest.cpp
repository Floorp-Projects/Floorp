/* vim:set ts=2 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include "nsISimpleTest.h"
#include "mozilla/ModuleUtils.h"

class SimpleTest : public nsISimpleTest
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLETEST
};

NS_IMPL_ISUPPORTS1(SimpleTest, nsISimpleTest)

NS_IMETHODIMP
SimpleTest::Add(int32_t a, int32_t b, int32_t *r)
{
  printf("add(%d,%d) from C++\n", a, b);

  *r = a + b;
  return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(SimpleTest)

// 5e14b432-37b6-4377-923b-c987418d8429
#define SIMPLETEST_CID \
  { 0x5e14b432, 0x37b6, 0x4377, \
    { 0x92, 0x3b, 0xc9, 0x87, 0x41, 0x8d, 0x84, 0x29 } }

NS_DEFINE_NAMED_CID(SIMPLETEST_CID);

static const mozilla::Module::CIDEntry kSimpleCIDs[] = {
  { &kSIMPLETEST_CID, false, nullptr, SimpleTestConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kSimpleContracts[] = {
  { "@test.mozilla.org/simple-test;1?impl=c++", &kSIMPLETEST_CID },
  { nullptr }
};

static const mozilla::Module kSimpleModule = {
  mozilla::Module::kVersion,
  kSimpleCIDs,
  kSimpleContracts
};

NSMODULE_DEFN(SimpleTestModule) = &kSimpleModule;
