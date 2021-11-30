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

  nsresult NonOverloadedNoInputComplex(std::pair<int, int>* aOut) {
    *aOut = std::pair{kTestValue, kTestValue};
    return NS_OK;
  }
  nsresult NonOverloadedNoInputFailsComplex(std::pair<int, int>* aOut) {
    return NS_ERROR_FAILURE;
  }

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

TEST(ResultExtensions_ToResultInvoke, Lambda_NoInput)
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

TEST(ResultExtensions_ToResultInvoke, MemFn_NoInput)
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

TEST(ResultExtensions_ToResultInvoke, MemFn_Polymorphic_NoInput)
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

TEST(ResultExtensions_ToResultInvokeMember, NoInput)
{
  TestClass foo;

  // success
  {
    auto valOrErr = ToResultInvokeMember(foo, &TestClass::NonOverloadedNoInput);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr =
        ToResultInvokeMember(foo, &TestClass::NonOverloadedNoInputFails);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvokeMember, NoInput_Const)
{
  const TestClass foo;

  // success
  {
    auto valOrErr =
        ToResultInvokeMember(foo, &TestClass::NonOverloadedNoInputConst);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr =
        ToResultInvokeMember(foo, &TestClass::NonOverloadedNoInputFailsConst);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvokeMember, NoInput_Ref)
{
  TestClass foo;

  // success
  {
    auto valOrErr =
        ToResultInvokeMember(foo, &TestClass::NonOverloadedNoInputRef);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr =
        ToResultInvokeMember(foo, &TestClass::NonOverloadedNoInputFailsRef);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvokeMember, NoInput_Complex)
{
  TestClass foo;

  // success
  {
    auto valOrErr =
        ToResultInvokeMember(foo, &TestClass::NonOverloadedNoInputComplex);
    static_assert(std::is_same_v<decltype(valOrErr),
                                 Result<std::pair<int, int>, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ((std::pair{TestClass::kTestValue, TestClass::kTestValue}),
              valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr =
        ToResultInvokeMember(foo, &TestClass::NonOverloadedNoInputFailsComplex);
    static_assert(std::is_same_v<decltype(valOrErr),
                                 Result<std::pair<int, int>, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvokeMember, WithInput)
{
  TestClass foo;

  // success
  {
    auto valOrErr = ToResultInvokeMember(
        foo, &TestClass::NonOverloadedWithInput, -TestClass::kTestValue);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(-TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr = ToResultInvokeMember(
        foo, &TestClass::NonOverloadedWithInputFails, -TestClass::kTestValue);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvokeMember, NoOutput)
{
  TestClass foo;

  // success
  {
    auto valOrErr = ToResultInvokeMember(foo, &TestClass::NonOverloadedNoOutput,
                                         -TestClass::kTestValue);
    static_assert(std::is_same_v<decltype(valOrErr), Result<Ok, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
  }

  // failure
  {
    auto valOrErr = ToResultInvokeMember(
        foo, &TestClass::NonOverloadedNoOutputFails, -TestClass::kTestValue);
    static_assert(std::is_same_v<decltype(valOrErr), Result<Ok, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvokeMember, NoInput_Macro)
{
  TestClass foo;

  // success
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE_MEMBER(foo, NonOverloadedNoInput);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE_MEMBER(foo, NonOverloadedNoInputFails);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvokeMember, NoInput_Const_Macro)
{
  const TestClass foo;

  // success
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE_MEMBER(foo, NonOverloadedNoInputConst);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr =
        MOZ_TO_RESULT_INVOKE_MEMBER(foo, NonOverloadedNoInputFailsConst);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvokeMember, NoInput_Ref_Macro)
{
  TestClass foo;

  // success
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE_MEMBER(foo, NonOverloadedNoInputRef);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr =
        MOZ_TO_RESULT_INVOKE_MEMBER(foo, NonOverloadedNoInputFailsRef);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvokeMember, NoInput_Complex_Macro)
{
  TestClass foo;

  // success
  {
    auto valOrErr =
        MOZ_TO_RESULT_INVOKE_MEMBER(foo, NonOverloadedNoInputComplex);
    static_assert(std::is_same_v<decltype(valOrErr),
                                 Result<std::pair<int, int>, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ((std::pair{TestClass::kTestValue, TestClass::kTestValue}),
              valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr =
        MOZ_TO_RESULT_INVOKE_MEMBER(foo, NonOverloadedNoInputFailsComplex);

    static_assert(std::is_same_v<decltype(valOrErr),
                                 Result<std::pair<int, int>, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvokeMember, WithInput_Macro)
{
  TestClass foo;

  // success
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE_MEMBER(foo, NonOverloadedWithInput,
                                                -TestClass::kTestValue);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(-TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE_MEMBER(
        foo, NonOverloadedWithInputFails, -TestClass::kTestValue);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvokeMember, NoOutput_Macro)
{
  TestClass foo;

  // success
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE_MEMBER(foo, NonOverloadedNoOutput,
                                                -TestClass::kTestValue);
    static_assert(std::is_same_v<decltype(valOrErr), Result<Ok, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
  }

  // failure
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE_MEMBER(foo, NonOverloadedNoOutputFails,
                                                -TestClass::kTestValue);
    static_assert(std::is_same_v<decltype(valOrErr), Result<Ok, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvokeMember, NoInput_Complex_Macro_Typed)
{
  TestClass foo;

  // success
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(
        (std::pair<int, int>), foo, NonOverloadedNoInputComplex);
    static_assert(std::is_same_v<decltype(valOrErr),
                                 Result<std::pair<int, int>, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ((std::pair{TestClass::kTestValue, TestClass::kTestValue}),
              valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(
        (std::pair<int, int>), foo, NonOverloadedNoInputFailsComplex);
    static_assert(std::is_same_v<decltype(valOrErr),
                                 Result<std::pair<int, int>, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvokeMember, RefPtr_NoInput)
{
  auto foo = MakeRefPtr<RefCountedTestClass>();

  // success
  {
    auto valOrErr =
        ToResultInvokeMember(foo, &RefCountedTestClass::NonOverloadedNoInput);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr = ToResultInvokeMember(
        foo, &RefCountedTestClass::NonOverloadedNoInputFails);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvokeMember, RefPtr_NoInput_Macro)
{
  auto foo = MakeRefPtr<RefCountedTestClass>();

  // success
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE_MEMBER(foo, NonOverloadedNoInput);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE_MEMBER(foo, NonOverloadedNoInputFails);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvokeMember, RawPtr_NoInput_Macro)
{
  auto foo = MakeRefPtr<RefCountedTestClass>();
  auto* fooPtr = foo.get();

  // success
  {
    auto valOrErr = MOZ_TO_RESULT_INVOKE_MEMBER(fooPtr, NonOverloadedNoInput);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isOk());
    ASSERT_EQ(TestClass::kTestValue, valOrErr.unwrap());
  }

  // failure
  {
    auto valOrErr =
        MOZ_TO_RESULT_INVOKE_MEMBER(fooPtr, NonOverloadedNoInputFails);
    static_assert(std::is_same_v<decltype(valOrErr), Result<int, nsresult>>);
    ASSERT_TRUE(valOrErr.isErr());
    ASSERT_EQ(NS_ERROR_FAILURE, valOrErr.unwrapErr());
  }
}

TEST(ResultExtensions_ToResultInvokeMember, nsCOMPtr_AbstractClass_WithInput)
{
  nsCOMPtr<nsIFile> file = MakeAndAddRef<nsLocalFile>();

  auto valOrErr = ToResultInvokeMember(file, &nsIFile::Equals, file);
  static_assert(std::is_same_v<decltype(valOrErr), Result<bool, nsresult>>);
  ASSERT_TRUE(valOrErr.isOk());
  ASSERT_EQ(true, valOrErr.unwrap());
}

TEST(ResultExtensions_ToResultInvokeMember,
     RawPtr_AbstractClass_WithInput_Macro)
{
  nsCOMPtr<nsIFile> file = MakeAndAddRef<nsLocalFile>();
  auto* filePtr = file.get();

  auto valOrErr = MOZ_TO_RESULT_INVOKE_MEMBER(filePtr, Equals, file);
  static_assert(std::is_same_v<decltype(valOrErr), Result<bool, nsresult>>);
  ASSERT_TRUE(valOrErr.isOk());
  ASSERT_EQ(true, valOrErr.unwrap());
}

TEST(ResultExtensions_ToResultInvokeMember,
     RawPtr_AbstractClass_NoInput_Macro_Typed)
{
  nsCOMPtr<nsIFile> file = MakeAndAddRef<nsLocalFile>();
  auto* filePtr = file.get();

  auto valOrErr =
      MOZ_TO_RESULT_INVOKE_MEMBER_TYPED(nsCOMPtr<nsIFile>, filePtr, Clone);
  static_assert(
      std::is_same_v<decltype(valOrErr), Result<nsCOMPtr<nsIFile>, nsresult>>);
  ASSERT_TRUE(valOrErr.isOk());
  ASSERT_NE(nullptr, valOrErr.unwrap());
}
