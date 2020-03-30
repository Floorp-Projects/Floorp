/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/InitializedOnce.h"

#include <type_traits>

using namespace mozilla;

namespace {
template <typename T>
void AssertIsSome(const T& aVal) {
  ASSERT_TRUE(aVal);
  ASSERT_TRUE(aVal.isSome());
  ASSERT_FALSE(aVal.isNothing());
}

template <typename T>
void AssertIsNothing(const T& aVal) {
  ASSERT_FALSE(aVal);
  ASSERT_FALSE(aVal.isSome());
  ASSERT_TRUE(aVal.isNothing());
}

// XXX TODO Should be true once Bug ... ensures this for Maybe.
// static_assert(std::is_trivially_destructible_v<InitializedOnce<const int>>);
// static_assert(std::is_trivially_default_constructible_v<InitializedOnce<const
// int, LazyInit::Allow>>);

static_assert(!std::is_copy_constructible_v<InitializedOnce<const int>>);
static_assert(!std::is_copy_assignable_v<InitializedOnce<const int>>);

static_assert(!std::is_default_constructible_v<InitializedOnce<const int>>);
static_assert(std::is_default_constructible_v<LazyInitializedOnce<const int>>);
static_assert(std::is_default_constructible_v<
              LazyInitializedOnceEarlyDestructible<const int>>);

// XXX We cannot test for move-constructability/move-assignability at the
// moment, since the operations are always defined, but trigger static_assert's
// if they should not be used. This is not too bad, since we are never copyable.

constexpr InitializedOnce<const int>* kPtrInitializedOnceIntLazyInitForbid =
    nullptr;
constexpr LazyInitializedOnce<const int>* kPtrInitializedOnceIntLazyInitAllow =
    nullptr;
constexpr LazyInitializedOnceEarlyDestructible<const int>*
    kPtrInitializedOnceIntLazyInitAllowResettable = nullptr;

template <class T, typename = decltype(std::declval<T*>()->destroy())>
constexpr bool test_has_destroy_method(const T*) {
  return true;
}
constexpr bool test_has_destroy_method(...) { return false; }

static_assert(test_has_destroy_method(kPtrInitializedOnceIntLazyInitForbid));
static_assert(!test_has_destroy_method(kPtrInitializedOnceIntLazyInitAllow));
static_assert(
    test_has_destroy_method(kPtrInitializedOnceIntLazyInitAllowResettable));

template <class T,
          typename = decltype(std::declval<T*>()->init(std::declval<int>()))>
constexpr bool test_has_init_method(const T*) {
  return true;
}
constexpr bool test_has_init_method(...) { return false; }

static_assert(!test_has_init_method(kPtrInitializedOnceIntLazyInitForbid));
static_assert(test_has_init_method(kPtrInitializedOnceIntLazyInitAllow));
static_assert(
    test_has_init_method(kPtrInitializedOnceIntLazyInitAllowResettable));

struct MoveOnly {
  explicit MoveOnly(int aValue) : mValue{aValue} {}

  MoveOnly(MoveOnly&&) = default;
  MoveOnly& operator=(MoveOnly&&) = default;

  int mValue;
};

}  // namespace

constexpr int testValue = 32;

TEST(InitializedOnce, ImmediateInit)
{
  const InitializedOnce<const MoveOnly> val{testValue};

  AssertIsSome(val);
  ASSERT_EQ(testValue, (*val).mValue);
  ASSERT_EQ(testValue, val->mValue);
}

TEST(InitializedOnce, ImmediateInitReset)
{
  InitializedOnce<const MoveOnly> val{testValue};
  val.destroy();

  AssertIsNothing(val);
}

TEST(InitializedOnce, MoveConstruct)
{
  InitializedOnce<const MoveOnly> oldVal{testValue};
  InitializedOnce<const MoveOnly> val{std::move(oldVal)};

  AssertIsNothing(oldVal);
  AssertIsSome(val);
}

TEST(InitializedOnceAllowLazy, DefaultCtor)
{
  LazyInitializedOnce<const MoveOnly> val;

  AssertIsNothing(val);
}

TEST(InitializedOnceAllowLazy, Init)
{
  LazyInitializedOnce<const MoveOnly> val;
  val.init(testValue);

  AssertIsSome(val);
  ASSERT_EQ(testValue, (*val).mValue);
  ASSERT_EQ(testValue, val->mValue);
}

TEST(InitializedOnceAllowLazyResettable, DefaultCtor)
{
  LazyInitializedOnceEarlyDestructible<const MoveOnly> val;

  AssertIsNothing(val);
}

TEST(InitializedOnceAllowLazyResettable, Init)
{
  LazyInitializedOnceEarlyDestructible<const MoveOnly> val;
  val.init(testValue);

  AssertIsSome(val);
  ASSERT_EQ(testValue, (*val).mValue);
  ASSERT_EQ(testValue, val->mValue);
}

TEST(InitializedOnceAllowLazyResettable, InitReset)
{
  LazyInitializedOnceEarlyDestructible<const MoveOnly> val;
  val.init(testValue);
  val.destroy();

  AssertIsNothing(val);
}

TEST(InitializedOnceAllowLazyResettable, MoveConstruct)
{
  LazyInitializedOnceEarlyDestructible<const MoveOnly> oldVal{testValue};
  LazyInitializedOnceEarlyDestructible<const MoveOnly> val{std::move(oldVal)};

  AssertIsNothing(oldVal);
  AssertIsSome(val);
}

TEST(InitializedOnceAllowLazyResettable, MoveAssign)
{
  LazyInitializedOnceEarlyDestructible<const MoveOnly> oldVal{testValue};
  LazyInitializedOnceEarlyDestructible<const MoveOnly> val;

  val = std::move(oldVal);

  AssertIsNothing(oldVal);
  AssertIsSome(val);
}

// XXX How do we test for assertions to be hit?
