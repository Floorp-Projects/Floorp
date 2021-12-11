/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/ResultExtensions.h"
#include "nsLocalFile.h"

#include <functional>

using namespace mozilla;

namespace {
class TestClass {
 public:
  static constexpr int kTestValue = 42;

  nsresult NonOverloadedNoInput(int* aOut) {
    *aOut = kTestValue;
    return NS_OK;
  }
  nsresult NonOverloadedNoInputFails(int* aOut) { return NS_ERROR_FAILURE; }

  nsresult NonOverloadedNoInputConst(int* aOut) const {
    *aOut = kTestValue;
    return NS_OK;
  }
  nsresult NonOverloadedNoInputFailsConst(int* aOut) const {
    return NS_ERROR_FAILURE;
  }

  nsresult NonOverloadedNoInputRef(int& aOut) {
    aOut = kTestValue;
    return NS_OK;
  }
  nsresult NonOverloadedNoInputFailsRef(int& aOut) { return NS_ERROR_FAILURE; }

  nsresult NonOverloadedWithInput(int aIn, int* aOut) {
    *aOut = aIn;
    return NS_OK;
  }
  nsresult NonOverloadedWithInputFails(int aIn, int* aOut) {
    return NS_ERROR_FAILURE;
  }

  nsresult NonOverloadedNoOutput(int aIn) { return NS_OK; }
  nsresult NonOverloadedNoOutputFails(int aIn) { return NS_ERROR_FAILURE; }

  nsresult PolymorphicNoInput(nsIFile** aOut) {
    *aOut = MakeAndAddRef<nsLocalFile>().take();
    return NS_OK;
  }
  nsresult PolymorphicNoInputFails(nsIFile** aOut) { return NS_ERROR_FAILURE; }
};

class RefCountedTestClass {
 public:
  NS_INLINE_DECL_REFCOUNTING(RefCountedTestClass);

  static constexpr int kTestValue = 42;

  nsresult NonOverloadedNoInput(int* aOut) {
    *aOut = kTestValue;
    return NS_OK;
  }
  nsresult NonOverloadedNoInputFails(int* aOut) { return NS_ERROR_FAILURE; }

 private:
  ~RefCountedTestClass() = default;
};

// Check that DerefedType deduces the types as expected
static_assert(std::is_same_v<mozilla::detail::DerefedType<RefCountedTestClass&>,
                             RefCountedTestClass>);
static_assert(std::is_same_v<mozilla::detail::DerefedType<RefCountedTestClass*>,
                             RefCountedTestClass>);
static_assert(
    std::is_same_v<mozilla::detail::DerefedType<RefPtr<RefCountedTestClass>>,
                   RefCountedTestClass>);

static_assert(std::is_same_v<mozilla::detail::DerefedType<nsIFile&>, nsIFile>);
static_assert(std::is_same_v<mozilla::detail::DerefedType<nsIFile*>, nsIFile>);
static_assert(
    std::is_same_v<mozilla::detail::DerefedType<nsCOMPtr<nsIFile>>, nsIFile>);
}  // namespace

TEST(ResultExtensions_ToResultInvoke, Lambda)
{
  TestClass foo;

  // success
  {
    auto valOrErr = ToResultInvoke<int>(
        [&foo](int* out) { return foo.NonOverloadedNoInput(out); });
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr = ToResultInvoke<int>(
        [&foo](int* out) { return foo.NonOverloadedNoInputFails(out); });
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvoke, MemFn)
{
  TestClass foo;

  // success
  {
    auto valOrErr =
        ToResultInvoke<int>(std::mem_fn(&TestClass::NonOverloadedNoInput), foo);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr = ToResultInvoke<int>(
        std::mem_fn(&TestClass::NonOverloadedNoInputFails), foo);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvoke, MemberFunction_NoInput)
{
  TestClass foo;

  // success
  {
    auto valOrErr = ToResultInvoke(foo, &TestClass::NonOverloadedNoInput);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr = ToResultInvoke(foo, &TestClass::NonOverloadedNoInputFails);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvoke, MemberFunction_NoInput_Const)
{
  const TestClass foo;

  // success
  {
    auto valOrErr = ToResultInvoke(foo, &TestClass::NonOverloadedNoInputConst);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr =
        ToResultInvoke(foo, &TestClass::NonOverloadedNoInputFailsConst);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvoke, MemberFunction_NoInput_Ref)
{
  TestClass foo;

  // success
  {
    auto valOrErr = ToResultInvoke(foo, &TestClass::NonOverloadedNoInputRef);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr =
        ToResultInvoke(foo, &TestClass::NonOverloadedNoInputFailsRef);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvoke, MemberFunction_WithInput)
{
  TestClass foo;

  // success
  {
    auto valOrErr = ToResultInvoke(foo, &TestClass::NonOverloadedWithInput,
                                   -TestClass::kTestValue);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(-TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr = ToResultInvoke(foo, &TestClass::NonOverloadedWithInputFails,
                                   -TestClass::kTestValue);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvoke, MemberFunction_NoOutput)
{
  TestClass foo;

  // success
  {
    auto valOrErr = ToResultInvoke(foo, &TestClass::NonOverloadedNoOutput,
                                   -TestClass::kTestValue);
    static_assert(std::is_same_v<decltype(valOrErr), Result<Ok, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
  }

  // failure
  {
    auto valOrErr = ToResultInvoke(foo, &TestClass::NonOverloadedNoOutputFails,
                                   -TestClass::kTestValue);
    static_assert(std::is_same_v<decltype(valOrErr), Result<Ok, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvoke, MemberFunction_NoInput_Macro)
{
  TestClass foo;

  // success
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE(foo, NonOverloadedNoInput);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE(foo, NonOverloadedNoInputFails);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvoke, MemberFunction_NoInput_Const_Macro)
{
  const TestClass foo;

  // success
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE(foo, NonOverloadedNoInputConst);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE(foo, NonOverloadedNoInputFailsConst);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvoke, MemberFunction_NoInput_Ref_Macro)
{
  TestClass foo;

  // success
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE(foo, NonOverloadedNoInputRef);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE(foo, NonOverloadedNoInputFailsRef);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvoke, MemberFunction_WithInput_Macro)
{
  TestClass foo;

  // success
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE(foo, NonOverloadedWithInput,
                                         -TestClass::kTestValue);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(-TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE(foo, NonOverloadedWithInputFails,
                                         -TestClass::kTestValue);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvoke, MemberFunction_NoOutput_Macro)
{
  TestClass foo;

  // success
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE(foo, NonOverloadedNoOutput,
                                         -TestClass::kTestValue);
    static_assert(std::is_same_v<decltype(valOrErr), Result<Ok, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
  }

  // failure
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE(foo, NonOverloadedNoOutputFails,
                                         -TestClass::kTestValue);
    static_assert(std::is_same_v<decltype(valOrErr), Result<Ok, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvoke, PolymorphicPointerResult_nsCOMPtr_Result)
{
  TestClass foo;

  // success
  {
    auto valOrErr = ToResultInvoke<nsCOMPtr<nsIFile>>(
        std::mem_fn(&TestClass::PolymorphicNoInput), foo);
    static_assert(std::is_same_v<decltype(valOrErr),
                                 Result<nsCOMPtr<nsIFile>, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_NE(nullptr, valOrErr.inspect());

    ASSERT_EQ(ToResultInvoke<nsString>(std::mem_fn(&nsIFile::GetPath),
                                       *MakeRefPtr<nsLocalFile>())
                  .inspect(),
              ToResultInvoke<nsString>(std::mem_fn(&nsIFile::GetPath),
                                       valOrErr.inspect())
                  .inspect());
  }

  // failure
  {
    auto valOrErr = ToResultInvoke<nsCOMPtr<nsIFile>>(
        std::mem_fn(&TestClass::PolymorphicNoInputFails), foo);
    static_assert(std::is_same_v<decltype(valOrErr),
                                 Result<nsCOMPtr<nsIFile>, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvoke, RefPtr_MemberFunction_NoInput)
{
  auto foo = MakeRefPtr<RefCountedTestClass>();

  // success
  {
    auto valOrErr =
        ToResultInvoke(foo, &RefCountedTestClass::NonOverloadedNoInput);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr =
        ToResultInvoke(foo, &RefCountedTestClass::NonOverloadedNoInputFails);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvoke, RefPtr_MemberFunction_NoInput_Macro)
{
  auto foo = MakeRefPtr<RefCountedTestClass>();

  // success
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE(foo, NonOverloadedNoInput);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE(foo, NonOverloadedNoInputFails);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvoke, RawPtr_MemberFunction_NoInput_Macro)
{
  auto foo = MakeRefPtr<RefCountedTestClass>();
  auto* fooPtr = foo.get();

  // success
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE(fooPtr, NonOverloadedNoInput);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE(fooPtr, NonOverloadedNoInputFails);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvoke, nsCOMPtr_NS_IMETHOD_bool_Result)
{
  nsCOMPtr<nsIFile> file = MakeAndAddRef<nsLocalFile>();
  ASSERT_TRUE(ToResultInvoke(file, &nsIFile::Equals, file).isOk());
}

TEST(ResultExtensions_ToResultInvoke, RawPtr_AbstractClass_MemberFunction_Macro)
{
  nsCOMPtr<nsIFile> file = MakeAndAddRef<nsLocalFile>();
  auto* filePtr = file.get();

  auto valOrErr = MOZ_TO_RESULT_INVOKE(filePtr, Equals, file);
  static_assert(std::is_same_v<decltype(valOrErr), Result<bool, nsresult>>);
  ASSERT_TRUE(valOrErr.isOk());
  ASSERT_EQ(true, valOrErr.unwrap());
}

TEST(ResultExtensions_ToResultInvoke,
     RawPtr_AbstractClass_MemberFunction_NoInput_Macro_Typed)
{
  nsCOMPtr<nsIFile> file = MakeAndAddRef<nsLocalFile>();
  auto* filePtr = file.get();

  auto valOrErr = MOZ_TO_RESULT_INVOKE_TYPED(nsCOMPtr<nsIFile>, filePtr, Clone);
  static_assert(
      std::is_same_v<decltype(valOrErr), Result<nsCOMPtr<nsIFile>, nsresult>>);
  ASSERT_TRUE(valOrErr.isOk());
  ASSERT_NE(nullptr, valOrErr.unwrap());
}
