/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#ifndef FOGFixture_h_
#define FOGFixture_h_

#include "gtest/gtest.h"
#include "mozilla/glean/fog_ffi_generated.h"
#include "nsString.h"

using namespace mozilla::glean::impl;

class FOGFixture : public ::testing::Test {
 protected:
  FOGFixture() = default;
  virtual void SetUp() { TestResetFOG(); }

 public:
  static void TestResetFOG() {
    nsCString empty;
    ASSERT_EQ(NS_OK, fog_test_reset(&empty, &empty));
  }
};

template <typename ParamType>
class FOGFixtureWithParam : public ::testing::TestWithParam<ParamType> {
 protected:
  FOGFixtureWithParam() = default;

  virtual void SetUp() { TestResetFOG(); }

  // Can be called by derived fixtures
  void TestResetFOG() { FOGFixture::TestResetFOG(); }
};

#endif  // FOGFixture_h_
