/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <type_traits>
#include <utility>

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Compiler.h"
#include "mozilla/Maybe.h"
#include "mozilla/TemplateLib.h"
#include "mozilla/Types.h"
#include "mozilla/UniquePtr.h"

using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;
using mozilla::SomeRef;
using mozilla::ToMaybe;

#define RUN_TEST(t)                                                     \
  do {                                                                  \
    bool cond = (t());                                                  \
    if (!cond) return 1;                                                \
    cond = AllDestructorsWereCalled();                                  \
    MOZ_ASSERT(cond, "Failed to destroy all objects during test: " #t); \
    if (!cond) return 1;                                                \
  } while (false)

enum Status {
  eWasDefaultConstructed,
  eWasConstructed,
  eWasCopyConstructed,
  eWasMoveConstructed,
  eWasAssigned,
  eWasCopyAssigned,
  eWasMoveAssigned,
  eWasCopiedFrom,
  eWasMovedFrom,
};

static size_t sUndestroyedObjects = 0;

static bool AllDestructorsWereCalled() { return sUndestroyedObjects == 0; }

struct BasicValue {
  BasicValue() : mStatus(eWasDefaultConstructed), mTag(0) {
    ++sUndestroyedObjects;
  }

  explicit BasicValue(int aTag) : mStatus(eWasConstructed), mTag(aTag) {
    ++sUndestroyedObjects;
  }

  BasicValue(const BasicValue& aOther)
      : mStatus(eWasCopyConstructed), mTag(aOther.mTag) {
    ++sUndestroyedObjects;
  }

  BasicValue(BasicValue&& aOther)
      : mStatus(eWasMoveConstructed), mTag(aOther.mTag) {
    ++sUndestroyedObjects;
    aOther.mStatus = eWasMovedFrom;
    aOther.mTag = 0;
  }

  ~BasicValue() { --sUndestroyedObjects; }

  BasicValue& operator=(const BasicValue& aOther) {
    mStatus = eWasCopyAssigned;
    mTag = aOther.mTag;
    return *this;
  }

  BasicValue& operator=(BasicValue&& aOther) {
    mStatus = eWasMoveAssigned;
    mTag = aOther.mTag;
    aOther.mStatus = eWasMovedFrom;
    aOther.mTag = 0;
    return *this;
  }

  bool operator==(const BasicValue& aOther) const {
    return mTag == aOther.mTag;
  }

  bool operator<(const BasicValue& aOther) const { return mTag < aOther.mTag; }

  Status GetStatus() const { return mStatus; }
  void SetTag(int aValue) { mTag = aValue; }
  int GetTag() const { return mTag; }

 private:
  Status mStatus;
  int mTag;
};

struct UncopyableValue {
  UncopyableValue() : mStatus(eWasDefaultConstructed) { ++sUndestroyedObjects; }

  UncopyableValue(UncopyableValue&& aOther) : mStatus(eWasMoveConstructed) {
    ++sUndestroyedObjects;
    aOther.mStatus = eWasMovedFrom;
  }

  ~UncopyableValue() { --sUndestroyedObjects; }

  UncopyableValue& operator=(UncopyableValue&& aOther) {
    mStatus = eWasMoveAssigned;
    aOther.mStatus = eWasMovedFrom;
    return *this;
  }

  Status GetStatus() { return mStatus; }

 private:
  UncopyableValue(const UncopyableValue& aOther) = delete;
  UncopyableValue& operator=(const UncopyableValue& aOther) = delete;

  Status mStatus;
};

struct UnmovableValue {
  UnmovableValue() : mStatus(eWasDefaultConstructed) { ++sUndestroyedObjects; }

  UnmovableValue(const UnmovableValue& aOther) : mStatus(eWasCopyConstructed) {
    ++sUndestroyedObjects;
  }

  ~UnmovableValue() { --sUndestroyedObjects; }

  UnmovableValue& operator=(const UnmovableValue& aOther) {
    mStatus = eWasCopyAssigned;
    return *this;
  }

  Status GetStatus() { return mStatus; }

  UnmovableValue(UnmovableValue&& aOther) = delete;
  UnmovableValue& operator=(UnmovableValue&& aOther) = delete;

 private:
  Status mStatus;
};

struct UncopyableUnmovableValue {
  UncopyableUnmovableValue() : mStatus(eWasDefaultConstructed) {
    ++sUndestroyedObjects;
  }

  explicit UncopyableUnmovableValue(int) : mStatus(eWasConstructed) {
    ++sUndestroyedObjects;
  }

  ~UncopyableUnmovableValue() { --sUndestroyedObjects; }

  Status GetStatus() { return mStatus; }

 private:
  UncopyableUnmovableValue(const UncopyableUnmovableValue& aOther) = delete;
  UncopyableUnmovableValue& operator=(const UncopyableUnmovableValue& aOther) =
      delete;
  UncopyableUnmovableValue(UncopyableUnmovableValue&& aOther) = delete;
  UncopyableUnmovableValue& operator=(UncopyableUnmovableValue&& aOther) =
      delete;

  Status mStatus;
};

static_assert(std::is_literal_type_v<Maybe<int>>);
static_assert(std::is_trivially_copy_constructible_v<Maybe<int>>);
static_assert(std::is_trivially_copy_assignable_v<Maybe<int>>);

static_assert(42 == Some(42).value());
static_assert(42 == Some(42).valueOr(43));
static_assert(42 == Maybe<int>{}.valueOr(42));
static_assert(42 == Some(42).valueOrFrom([] { return 43; }));
static_assert(42 == Maybe<int>{}.valueOrFrom([] { return 42; }));
static_assert(Some(43) == [] {
  auto val = Some(42);
  val.apply([](int& val) { val += 1; });
  return val;
}());
static_assert(Some(43) == Some(42).map([](int val) { return val + 1; }));

struct TriviallyDestructible {
  TriviallyDestructible() {  // not trivially constructible
  }
};

static_assert(std::is_trivially_destructible_v<Maybe<TriviallyDestructible>>);

static bool TestBasicFeatures() {
  // Check that a Maybe<T> is initialized to Nothing.
  Maybe<BasicValue> mayValue;
  static_assert(std::is_same_v<BasicValue, decltype(mayValue)::ValueType>,
                "Should have BasicValue ValueType");
  MOZ_RELEASE_ASSERT(!mayValue);
  MOZ_RELEASE_ASSERT(!mayValue.isSome());
  MOZ_RELEASE_ASSERT(mayValue.isNothing());

  // Check that emplace() default constructs and the accessors work.
  mayValue.emplace();
  MOZ_RELEASE_ASSERT(mayValue);
  MOZ_RELEASE_ASSERT(mayValue.isSome());
  MOZ_RELEASE_ASSERT(!mayValue.isNothing());
  MOZ_RELEASE_ASSERT(*mayValue == BasicValue());
  MOZ_RELEASE_ASSERT(mayValue.value() == BasicValue());
  static_assert(std::is_same_v<BasicValue, decltype(mayValue.value())>,
                "value() should return a BasicValue");
  MOZ_RELEASE_ASSERT(mayValue.ref() == BasicValue());
  static_assert(std::is_same_v<BasicValue&, decltype(mayValue.ref())>,
                "ref() should return a BasicValue&");
  MOZ_RELEASE_ASSERT(mayValue.ptr() != nullptr);
  static_assert(std::is_same_v<BasicValue*, decltype(mayValue.ptr())>,
                "ptr() should return a BasicValue*");
  MOZ_RELEASE_ASSERT(mayValue->GetStatus() == eWasDefaultConstructed);

  // Check that reset() works.
  mayValue.reset();
  MOZ_RELEASE_ASSERT(!mayValue);
  MOZ_RELEASE_ASSERT(!mayValue.isSome());
  MOZ_RELEASE_ASSERT(mayValue.isNothing());

  // Check that emplace(T1) calls the correct constructor.
  mayValue.emplace(1);
  MOZ_RELEASE_ASSERT(mayValue);
  MOZ_RELEASE_ASSERT(mayValue->GetStatus() == eWasConstructed);
  MOZ_RELEASE_ASSERT(mayValue->GetTag() == 1);
  mayValue.reset();
  MOZ_RELEASE_ASSERT(!mayValue);

  // Check that Some() and Nothing() work.
  mayValue = Some(BasicValue(2));
  MOZ_RELEASE_ASSERT(mayValue);
  MOZ_RELEASE_ASSERT(mayValue->GetStatus() == eWasMoveConstructed);
  MOZ_RELEASE_ASSERT(mayValue->GetTag() == 2);
  mayValue = Nothing();
  MOZ_RELEASE_ASSERT(!mayValue);

  // Check that the accessors work through a const ref.
  mayValue.emplace();
  const Maybe<BasicValue>& mayValueCRef = mayValue;
  MOZ_RELEASE_ASSERT(mayValueCRef);
  MOZ_RELEASE_ASSERT(mayValueCRef.isSome());
  MOZ_RELEASE_ASSERT(!mayValueCRef.isNothing());
  MOZ_RELEASE_ASSERT(*mayValueCRef == BasicValue());
  MOZ_RELEASE_ASSERT(mayValueCRef.value() == BasicValue());
  static_assert(std::is_same_v<BasicValue, decltype(mayValueCRef.value())>,
                "value() should return a BasicValue");
  MOZ_RELEASE_ASSERT(mayValueCRef.ref() == BasicValue());
  static_assert(std::is_same_v<const BasicValue&, decltype(mayValueCRef.ref())>,
                "ref() should return a const BasicValue&");
  MOZ_RELEASE_ASSERT(mayValueCRef.ptr() != nullptr);
  static_assert(std::is_same_v<const BasicValue*, decltype(mayValueCRef.ptr())>,
                "ptr() should return a const BasicValue*");
  MOZ_RELEASE_ASSERT(mayValueCRef->GetStatus() == eWasDefaultConstructed);
  mayValue.reset();

  // Check that we can create and reference Maybe<const Type>.
  Maybe<const BasicValue> mayCValue1 = Some(BasicValue(5));
  MOZ_RELEASE_ASSERT(mayCValue1);
  MOZ_RELEASE_ASSERT(mayCValue1.isSome());
  MOZ_RELEASE_ASSERT(*mayCValue1 == BasicValue(5));
  const Maybe<const BasicValue>& mayCValue1Ref = mayCValue1;
  MOZ_RELEASE_ASSERT(mayCValue1Ref == mayCValue1);
  MOZ_RELEASE_ASSERT(*mayCValue1Ref == BasicValue(5));
  Maybe<const BasicValue> mayCValue2;
  mayCValue2.emplace(6);
  MOZ_RELEASE_ASSERT(mayCValue2);
  MOZ_RELEASE_ASSERT(mayCValue2.isSome());
  MOZ_RELEASE_ASSERT(*mayCValue2 == BasicValue(6));

  return true;
}

template <typename T>
static void TestCopyMaybe() {
  {
    MOZ_RELEASE_ASSERT(0 == sUndestroyedObjects);

    Maybe<T> src = Some(T());
    Maybe<T> dstCopyConstructed = src;

    MOZ_RELEASE_ASSERT(2 == sUndestroyedObjects);
    MOZ_RELEASE_ASSERT(dstCopyConstructed->GetStatus() == eWasCopyConstructed);
  }

  {
    MOZ_RELEASE_ASSERT(0 == sUndestroyedObjects);

    Maybe<T> src = Some(T());
    Maybe<T> dstCopyAssigned;
    dstCopyAssigned = src;

    MOZ_RELEASE_ASSERT(2 == sUndestroyedObjects);
    MOZ_RELEASE_ASSERT(dstCopyAssigned->GetStatus() == eWasCopyConstructed);
  }
}

template <typename T>
static void TestMoveMaybe() {
  {
    MOZ_RELEASE_ASSERT(0 == sUndestroyedObjects);

    Maybe<T> src = Some(T());
    Maybe<T> dstMoveConstructed = std::move(src);

    MOZ_RELEASE_ASSERT(1 == sUndestroyedObjects);
    MOZ_RELEASE_ASSERT(dstMoveConstructed->GetStatus() == eWasMoveConstructed);
  }

  {
    MOZ_RELEASE_ASSERT(0 == sUndestroyedObjects);

    Maybe<T> src = Some(T());
    Maybe<T> dstMoveAssigned;
    dstMoveAssigned = std::move(src);

    MOZ_RELEASE_ASSERT(1 == sUndestroyedObjects);
    MOZ_RELEASE_ASSERT(dstMoveAssigned->GetStatus() == eWasMoveConstructed);
  }
}

static bool TestCopyAndMove() {
  MOZ_RELEASE_ASSERT(0 == sUndestroyedObjects);

  {
    // Check that we get moves when possible for types that can support both
    // moves and copies.
    {
      Maybe<BasicValue> mayBasicValue = Some(BasicValue(1));
      MOZ_RELEASE_ASSERT(1 == sUndestroyedObjects);
      MOZ_RELEASE_ASSERT(mayBasicValue->GetStatus() == eWasMoveConstructed);
      MOZ_RELEASE_ASSERT(mayBasicValue->GetTag() == 1);
      mayBasicValue = Some(BasicValue(2));
      MOZ_RELEASE_ASSERT(1 == sUndestroyedObjects);
      MOZ_RELEASE_ASSERT(mayBasicValue->GetStatus() == eWasMoveAssigned);
      MOZ_RELEASE_ASSERT(mayBasicValue->GetTag() == 2);
      mayBasicValue.reset();
      MOZ_RELEASE_ASSERT(0 == sUndestroyedObjects);
      mayBasicValue.emplace(BasicValue(3));
      MOZ_RELEASE_ASSERT(1 == sUndestroyedObjects);
      MOZ_RELEASE_ASSERT(mayBasicValue->GetStatus() == eWasMoveConstructed);
      MOZ_RELEASE_ASSERT(mayBasicValue->GetTag() == 3);

      // Check that we get copies when moves aren't possible.
      Maybe<BasicValue> mayBasicValue2 = Some(*mayBasicValue);
      MOZ_RELEASE_ASSERT(mayBasicValue2->GetStatus() == eWasCopyConstructed);
      MOZ_RELEASE_ASSERT(mayBasicValue2->GetTag() == 3);
      mayBasicValue->SetTag(4);
      mayBasicValue2 = mayBasicValue;
      // This test should work again when we fix bug 1052940.
      // MOZ_RELEASE_ASSERT(mayBasicValue2->GetStatus() == eWasCopyAssigned);
      MOZ_RELEASE_ASSERT(mayBasicValue2->GetTag() == 4);
      mayBasicValue->SetTag(5);
      mayBasicValue2.reset();
      mayBasicValue2.emplace(*mayBasicValue);
      MOZ_RELEASE_ASSERT(mayBasicValue2->GetStatus() == eWasCopyConstructed);
      MOZ_RELEASE_ASSERT(mayBasicValue2->GetTag() == 5);

      // Check that std::move() works. (Another sanity check for move support.)
      Maybe<BasicValue> mayBasicValue3 = Some(std::move(*mayBasicValue));
      MOZ_RELEASE_ASSERT(mayBasicValue3->GetStatus() == eWasMoveConstructed);
      MOZ_RELEASE_ASSERT(mayBasicValue3->GetTag() == 5);
      MOZ_RELEASE_ASSERT(mayBasicValue->GetStatus() == eWasMovedFrom);
      mayBasicValue2->SetTag(6);
      mayBasicValue3 = Some(std::move(*mayBasicValue2));
      MOZ_RELEASE_ASSERT(mayBasicValue3->GetStatus() == eWasMoveAssigned);
      MOZ_RELEASE_ASSERT(mayBasicValue3->GetTag() == 6);
      MOZ_RELEASE_ASSERT(mayBasicValue2->GetStatus() == eWasMovedFrom);
      Maybe<BasicValue> mayBasicValue4;
      mayBasicValue4.emplace(std::move(*mayBasicValue3));
      MOZ_RELEASE_ASSERT(mayBasicValue4->GetStatus() == eWasMoveConstructed);
      MOZ_RELEASE_ASSERT(mayBasicValue4->GetTag() == 6);
      MOZ_RELEASE_ASSERT(mayBasicValue3->GetStatus() == eWasMovedFrom);
    }

    TestCopyMaybe<BasicValue>();
    TestMoveMaybe<BasicValue>();
  }

  MOZ_RELEASE_ASSERT(0 == sUndestroyedObjects);

  {
    // Check that we always get copies for types that don't support moves.
    {
      Maybe<UnmovableValue> mayUnmovableValue = Some(UnmovableValue());
      MOZ_RELEASE_ASSERT(mayUnmovableValue->GetStatus() == eWasCopyConstructed);
      mayUnmovableValue = Some(UnmovableValue());
      MOZ_RELEASE_ASSERT(mayUnmovableValue->GetStatus() == eWasCopyAssigned);
      mayUnmovableValue.reset();
      mayUnmovableValue.emplace(UnmovableValue());
      MOZ_RELEASE_ASSERT(mayUnmovableValue->GetStatus() == eWasCopyConstructed);
    }

    TestCopyMaybe<UnmovableValue>();

    static_assert(std::is_copy_constructible_v<Maybe<UnmovableValue>>);
    static_assert(std::is_copy_assignable_v<Maybe<UnmovableValue>>);
    // XXX Why do these static_asserts not hold?
    // static_assert(!std::is_move_constructible_v<Maybe<UnmovableValue>>);
    // static_assert(!std::is_move_assignable_v<Maybe<UnmovableValue>>);
  }

  MOZ_RELEASE_ASSERT(0 == sUndestroyedObjects);

  {
    // Check that types that only support moves, but not copies, work.
    {
      Maybe<UncopyableValue> mayUncopyableValue = Some(UncopyableValue());
      MOZ_RELEASE_ASSERT(mayUncopyableValue->GetStatus() ==
                         eWasMoveConstructed);
      mayUncopyableValue = Some(UncopyableValue());
      MOZ_RELEASE_ASSERT(mayUncopyableValue->GetStatus() == eWasMoveAssigned);
      mayUncopyableValue.reset();
      mayUncopyableValue.emplace(UncopyableValue());
      MOZ_RELEASE_ASSERT(mayUncopyableValue->GetStatus() ==
                         eWasMoveConstructed);
      mayUncopyableValue = Nothing();
    }

    TestMoveMaybe<BasicValue>();

    static_assert(!std::is_copy_constructible_v<Maybe<UncopyableValue>>);
    static_assert(!std::is_copy_assignable_v<Maybe<UncopyableValue>>);
    static_assert(std::is_move_constructible_v<Maybe<UncopyableValue>>);
    static_assert(std::is_move_assignable_v<Maybe<UncopyableValue>>);
  }

  MOZ_RELEASE_ASSERT(0 == sUndestroyedObjects);

  {  // Check that types that support neither moves or copies work.
    Maybe<UncopyableUnmovableValue> mayUncopyableUnmovableValue;
    mayUncopyableUnmovableValue.emplace();
    MOZ_RELEASE_ASSERT(mayUncopyableUnmovableValue->GetStatus() ==
                       eWasDefaultConstructed);
    mayUncopyableUnmovableValue.reset();
    mayUncopyableUnmovableValue.emplace(0);
    MOZ_RELEASE_ASSERT(mayUncopyableUnmovableValue->GetStatus() ==
                       eWasConstructed);
    mayUncopyableUnmovableValue = Nothing();

    static_assert(
        !std::is_copy_constructible_v<Maybe<UncopyableUnmovableValue>>);
    static_assert(!std::is_copy_assignable_v<Maybe<UncopyableUnmovableValue>>);
    static_assert(
        !std::is_move_constructible_v<Maybe<UncopyableUnmovableValue>>);
    static_assert(!std::is_move_assignable_v<Maybe<UncopyableUnmovableValue>>);
  }

  {
    // Test copy and move with a trivially copyable and trivially destructible
    // type.
    {
      constexpr Maybe<int> src = Some(42);
      constexpr Maybe<int> dstCopyConstructed = src;

      static_assert(src.isSome());
      static_assert(dstCopyConstructed.isSome());
      static_assert(42 == *src);
      static_assert(42 == *dstCopyConstructed);
      static_assert(42 == dstCopyConstructed.value());
    }

    {
      const Maybe<int> src = Some(42);
      Maybe<int> dstCopyAssigned;
      dstCopyAssigned = src;

      MOZ_RELEASE_ASSERT(src.isSome());
      MOZ_RELEASE_ASSERT(dstCopyAssigned.isSome());
      MOZ_RELEASE_ASSERT(42 == *src);
      MOZ_RELEASE_ASSERT(42 == *dstCopyAssigned);
    }

    {
      Maybe<int> src = Some(42);
      const Maybe<int> dstMoveConstructed = std::move(src);

      MOZ_RELEASE_ASSERT(!src.isSome());
      MOZ_RELEASE_ASSERT(dstMoveConstructed.isSome());
      MOZ_RELEASE_ASSERT(42 == *dstMoveConstructed);
    }

    {
      Maybe<int> src = Some(42);
      Maybe<int> dstMoveAssigned;
      dstMoveAssigned = std::move(src);

      MOZ_RELEASE_ASSERT(!src.isSome());
      MOZ_RELEASE_ASSERT(dstMoveAssigned.isSome());
      MOZ_RELEASE_ASSERT(42 == *dstMoveAssigned);
    }
  }

  return true;
}

static BasicValue* sStaticBasicValue = nullptr;

static BasicValue MakeBasicValue() { return BasicValue(9); }

static BasicValue& MakeBasicValueRef() { return *sStaticBasicValue; }

static BasicValue* MakeBasicValuePtr() { return sStaticBasicValue; }

static bool TestFunctionalAccessors() {
  BasicValue value(9);
  sStaticBasicValue = new BasicValue(9);

  // Check that the 'some' case of functional accessors works.
  Maybe<BasicValue> someValue = Some(BasicValue(3));
  MOZ_RELEASE_ASSERT(someValue.valueOr(value) == BasicValue(3));
  static_assert(std::is_same_v<BasicValue, decltype(someValue.valueOr(value))>,
                "valueOr should return a BasicValue");
  MOZ_RELEASE_ASSERT(someValue.valueOrFrom(&MakeBasicValue) == BasicValue(3));
  static_assert(
      std::is_same_v<BasicValue,
                     decltype(someValue.valueOrFrom(&MakeBasicValue))>,
      "valueOrFrom should return a BasicValue");
  MOZ_RELEASE_ASSERT(someValue.ptrOr(&value) != &value);
  static_assert(std::is_same_v<BasicValue*, decltype(someValue.ptrOr(&value))>,
                "ptrOr should return a BasicValue*");
  MOZ_RELEASE_ASSERT(*someValue.ptrOrFrom(&MakeBasicValuePtr) == BasicValue(3));
  static_assert(
      std::is_same_v<BasicValue*,
                     decltype(someValue.ptrOrFrom(&MakeBasicValuePtr))>,
      "ptrOrFrom should return a BasicValue*");
  MOZ_RELEASE_ASSERT(someValue.refOr(value) == BasicValue(3));
  static_assert(std::is_same_v<BasicValue&, decltype(someValue.refOr(value))>,
                "refOr should return a BasicValue&");
  MOZ_RELEASE_ASSERT(someValue.refOrFrom(&MakeBasicValueRef) == BasicValue(3));
  static_assert(
      std::is_same_v<BasicValue&,
                     decltype(someValue.refOrFrom(&MakeBasicValueRef))>,
      "refOrFrom should return a BasicValue&");

  // Check that the 'some' case works through a const reference.
  const Maybe<BasicValue>& someValueCRef = someValue;
  MOZ_RELEASE_ASSERT(someValueCRef.valueOr(value) == BasicValue(3));
  static_assert(
      std::is_same_v<BasicValue, decltype(someValueCRef.valueOr(value))>,
      "valueOr should return a BasicValue");
  MOZ_RELEASE_ASSERT(someValueCRef.valueOrFrom(&MakeBasicValue) ==
                     BasicValue(3));
  static_assert(
      std::is_same_v<BasicValue,
                     decltype(someValueCRef.valueOrFrom(&MakeBasicValue))>,
      "valueOrFrom should return a BasicValue");
  MOZ_RELEASE_ASSERT(someValueCRef.ptrOr(&value) != &value);
  static_assert(
      std::is_same_v<const BasicValue*, decltype(someValueCRef.ptrOr(&value))>,
      "ptrOr should return a const BasicValue*");
  MOZ_RELEASE_ASSERT(*someValueCRef.ptrOrFrom(&MakeBasicValuePtr) ==
                     BasicValue(3));
  static_assert(
      std::is_same_v<const BasicValue*,
                     decltype(someValueCRef.ptrOrFrom(&MakeBasicValuePtr))>,
      "ptrOrFrom should return a const BasicValue*");
  MOZ_RELEASE_ASSERT(someValueCRef.refOr(value) == BasicValue(3));
  static_assert(
      std::is_same_v<const BasicValue&, decltype(someValueCRef.refOr(value))>,
      "refOr should return a const BasicValue&");
  MOZ_RELEASE_ASSERT(someValueCRef.refOrFrom(&MakeBasicValueRef) ==
                     BasicValue(3));
  static_assert(
      std::is_same_v<const BasicValue&,
                     decltype(someValueCRef.refOrFrom(&MakeBasicValueRef))>,
      "refOrFrom should return a const BasicValue&");

  // Check that the 'none' case of functional accessors works.
  Maybe<BasicValue> noneValue;
  MOZ_RELEASE_ASSERT(noneValue.valueOr(value) == BasicValue(9));
  static_assert(std::is_same_v<BasicValue, decltype(noneValue.valueOr(value))>,
                "valueOr should return a BasicValue");
  MOZ_RELEASE_ASSERT(noneValue.valueOrFrom(&MakeBasicValue) == BasicValue(9));
  static_assert(
      std::is_same_v<BasicValue,
                     decltype(noneValue.valueOrFrom(&MakeBasicValue))>,
      "valueOrFrom should return a BasicValue");
  MOZ_RELEASE_ASSERT(noneValue.ptrOr(&value) == &value);
  static_assert(std::is_same_v<BasicValue*, decltype(noneValue.ptrOr(&value))>,
                "ptrOr should return a BasicValue*");
  MOZ_RELEASE_ASSERT(*noneValue.ptrOrFrom(&MakeBasicValuePtr) == BasicValue(9));
  static_assert(
      std::is_same_v<BasicValue*,
                     decltype(noneValue.ptrOrFrom(&MakeBasicValuePtr))>,
      "ptrOrFrom should return a BasicValue*");
  MOZ_RELEASE_ASSERT(noneValue.refOr(value) == BasicValue(9));
  static_assert(std::is_same_v<BasicValue&, decltype(noneValue.refOr(value))>,
                "refOr should return a BasicValue&");
  MOZ_RELEASE_ASSERT(noneValue.refOrFrom(&MakeBasicValueRef) == BasicValue(9));
  static_assert(
      std::is_same_v<BasicValue&,
                     decltype(noneValue.refOrFrom(&MakeBasicValueRef))>,
      "refOrFrom should return a BasicValue&");

  // Check that the 'none' case works through a const reference.
  const Maybe<BasicValue>& noneValueCRef = noneValue;
  MOZ_RELEASE_ASSERT(noneValueCRef.valueOr(value) == BasicValue(9));
  static_assert(
      std::is_same_v<BasicValue, decltype(noneValueCRef.valueOr(value))>,
      "valueOr should return a BasicValue");
  MOZ_RELEASE_ASSERT(noneValueCRef.valueOrFrom(&MakeBasicValue) ==
                     BasicValue(9));
  static_assert(
      std::is_same_v<BasicValue,
                     decltype(noneValueCRef.valueOrFrom(&MakeBasicValue))>,
      "valueOrFrom should return a BasicValue");
  MOZ_RELEASE_ASSERT(noneValueCRef.ptrOr(&value) == &value);
  static_assert(
      std::is_same_v<const BasicValue*, decltype(noneValueCRef.ptrOr(&value))>,
      "ptrOr should return a const BasicValue*");
  MOZ_RELEASE_ASSERT(*noneValueCRef.ptrOrFrom(&MakeBasicValuePtr) ==
                     BasicValue(9));
  static_assert(
      std::is_same_v<const BasicValue*,
                     decltype(noneValueCRef.ptrOrFrom(&MakeBasicValuePtr))>,
      "ptrOrFrom should return a const BasicValue*");
  MOZ_RELEASE_ASSERT(noneValueCRef.refOr(value) == BasicValue(9));
  static_assert(
      std::is_same_v<const BasicValue&, decltype(noneValueCRef.refOr(value))>,
      "refOr should return a const BasicValue&");
  MOZ_RELEASE_ASSERT(noneValueCRef.refOrFrom(&MakeBasicValueRef) ==
                     BasicValue(9));
  static_assert(
      std::is_same_v<const BasicValue&,
                     decltype(noneValueCRef.refOrFrom(&MakeBasicValueRef))>,
      "refOrFrom should return a const BasicValue&");

  // Clean up so the undestroyed objects count stays accurate.
  delete sStaticBasicValue;
  sStaticBasicValue = nullptr;

  return true;
}

static bool gFunctionWasApplied = false;

static void IncrementTag(BasicValue& aValue) {
  gFunctionWasApplied = true;
  aValue.SetTag(aValue.GetTag() + 1);
}

static void AccessValue(const BasicValue&) { gFunctionWasApplied = true; }

struct IncrementTagFunctor {
  IncrementTagFunctor() : mBy(1) {}

  void operator()(BasicValue& aValue) {
    aValue.SetTag(aValue.GetTag() + mBy.GetTag());
  }

  BasicValue mBy;
};

static bool TestApply() {
  // Check that apply handles the 'Nothing' case.
  gFunctionWasApplied = false;
  Maybe<BasicValue> mayValue;
  mayValue.apply(&IncrementTag);
  mayValue.apply(&AccessValue);
  MOZ_RELEASE_ASSERT(!gFunctionWasApplied);

  // Check that apply handles the 'Some' case.
  mayValue = Some(BasicValue(1));
  mayValue.apply(&IncrementTag);
  MOZ_RELEASE_ASSERT(gFunctionWasApplied);
  MOZ_RELEASE_ASSERT(mayValue->GetTag() == 2);
  gFunctionWasApplied = false;
  mayValue.apply(&AccessValue);
  MOZ_RELEASE_ASSERT(gFunctionWasApplied);

  // Check that apply works with a const reference.
  const Maybe<BasicValue>& mayValueCRef = mayValue;
  gFunctionWasApplied = false;
  mayValueCRef.apply(&AccessValue);
  MOZ_RELEASE_ASSERT(gFunctionWasApplied);

  // Check that apply works with functors.
  IncrementTagFunctor tagIncrementer;
  MOZ_RELEASE_ASSERT(tagIncrementer.mBy.GetStatus() == eWasConstructed);
  mayValue = Some(BasicValue(1));
  mayValue.apply(tagIncrementer);
  MOZ_RELEASE_ASSERT(mayValue->GetTag() == 2);
  MOZ_RELEASE_ASSERT(tagIncrementer.mBy.GetStatus() == eWasConstructed);

  // Check that apply works with lambda expressions.
  int32_t two = 2;
  gFunctionWasApplied = false;
  mayValue = Some(BasicValue(2));
  mayValue.apply([&](BasicValue& aVal) { aVal.SetTag(aVal.GetTag() * two); });
  MOZ_RELEASE_ASSERT(mayValue->GetTag() == 4);
  mayValue.apply([=](BasicValue& aVal) { aVal.SetTag(aVal.GetTag() * two); });
  MOZ_RELEASE_ASSERT(mayValue->GetTag() == 8);
  mayValueCRef.apply(
      [&](const BasicValue& aVal) { gFunctionWasApplied = true; });
  MOZ_RELEASE_ASSERT(gFunctionWasApplied == true);

  return true;
}

static int TimesTwo(const BasicValue& aValue) { return aValue.GetTag() * 2; }

static int TimesTwoAndResetOriginal(BasicValue& aValue) {
  int tag = aValue.GetTag();
  aValue.SetTag(1);
  return tag * 2;
}

struct MultiplyTagFunctor {
  MultiplyTagFunctor() : mBy(2) {}

  int operator()(BasicValue& aValue) { return aValue.GetTag() * mBy.GetTag(); }

  BasicValue mBy;
};

static bool TestMap() {
  // Check that map handles the 'Nothing' case.
  Maybe<BasicValue> mayValue;
  MOZ_RELEASE_ASSERT(mayValue.map(&TimesTwo) == Nothing());
  static_assert(std::is_same_v<Maybe<int>, decltype(mayValue.map(&TimesTwo))>,
                "map(TimesTwo) should return a Maybe<int>");
  MOZ_RELEASE_ASSERT(mayValue.map(&TimesTwoAndResetOriginal) == Nothing());

  // Check that map handles the 'Some' case.
  mayValue = Some(BasicValue(2));
  MOZ_RELEASE_ASSERT(mayValue.map(&TimesTwo) == Some(4));
  MOZ_RELEASE_ASSERT(mayValue.map(&TimesTwoAndResetOriginal) == Some(4));
  MOZ_RELEASE_ASSERT(mayValue->GetTag() == 1);
  mayValue = Some(BasicValue(2));

  // Check that map works with a const reference.
  mayValue->SetTag(2);
  const Maybe<BasicValue>& mayValueCRef = mayValue;
  MOZ_RELEASE_ASSERT(mayValueCRef.map(&TimesTwo) == Some(4));
  static_assert(
      std::is_same_v<Maybe<int>, decltype(mayValueCRef.map(&TimesTwo))>,
      "map(TimesTwo) should return a Maybe<int>");

  // Check that map works with functors.
  MultiplyTagFunctor tagMultiplier;
  MOZ_RELEASE_ASSERT(tagMultiplier.mBy.GetStatus() == eWasConstructed);
  MOZ_RELEASE_ASSERT(mayValue.map(tagMultiplier) == Some(4));
  MOZ_RELEASE_ASSERT(tagMultiplier.mBy.GetStatus() == eWasConstructed);

  // Check that map works with lambda expressions.
  int two = 2;
  mayValue = Some(BasicValue(2));
  Maybe<int> mappedValue =
      mayValue.map([&](const BasicValue& aVal) { return aVal.GetTag() * two; });
  MOZ_RELEASE_ASSERT(mappedValue == Some(4));
  mappedValue =
      mayValue.map([=](const BasicValue& aVal) { return aVal.GetTag() * two; });
  MOZ_RELEASE_ASSERT(mappedValue == Some(4));
  mappedValue = mayValueCRef.map(
      [&](const BasicValue& aVal) { return aVal.GetTag() * two; });
  MOZ_RELEASE_ASSERT(mappedValue == Some(4));

  // Check that function object qualifiers are preserved when invoked.
  struct F {
    std::integral_constant<int, 1> operator()(int) & { return {}; }
    std::integral_constant<int, 2> operator()(int) const& { return {}; }
    std::integral_constant<int, 3> operator()(int) && { return {}; }
    std::integral_constant<int, 4> operator()(int) const&& { return {}; }
  };
  Maybe<int> mi = Some(0);
  const Maybe<int> cmi = Some(0);
  F f;
  static_assert(std::is_same<decltype(mi.map(f)),
                             Maybe<std::integral_constant<int, 1>>>::value,
                "Maybe.map(&)");
  MOZ_RELEASE_ASSERT(mi.map(f).value()() == 1);
  static_assert(std::is_same<decltype(cmi.map(f)),
                             Maybe<std::integral_constant<int, 1>>>::value,
                "const Maybe.map(&)");
  MOZ_RELEASE_ASSERT(cmi.map(f).value()() == 1);
  const F cf;
  static_assert(std::is_same<decltype(mi.map(cf)),
                             Maybe<std::integral_constant<int, 2>>>::value,
                "Maybe.map(const &)");
  MOZ_RELEASE_ASSERT(mi.map(cf).value() == 2);
  static_assert(std::is_same<decltype(cmi.map(cf)),
                             Maybe<std::integral_constant<int, 2>>>::value,
                "const Maybe.map(const &)");
  MOZ_RELEASE_ASSERT(cmi.map(cf).value() == 2);
  static_assert(std::is_same<decltype(mi.map(F{})),
                             Maybe<std::integral_constant<int, 3>>>::value,
                "Maybe.map(&&)");
  MOZ_RELEASE_ASSERT(mi.map(F{}).value() == 3);
  static_assert(std::is_same<decltype(cmi.map(F{})),
                             Maybe<std::integral_constant<int, 3>>>::value,
                "const Maybe.map(&&)");
  MOZ_RELEASE_ASSERT(cmi.map(F{}).value() == 3);
  using CF = const F;
  static_assert(std::is_same<decltype(mi.map(CF{})),
                             Maybe<std::integral_constant<int, 4>>>::value,
                "Maybe.map(const &&)");
  MOZ_RELEASE_ASSERT(mi.map(CF{}).value() == 4);
  static_assert(std::is_same<decltype(cmi.map(CF{})),
                             Maybe<std::integral_constant<int, 4>>>::value,
                "const Maybe.map(const &&)");
  MOZ_RELEASE_ASSERT(cmi.map(CF{}).value() == 4);

  return true;
}

static bool TestToMaybe() {
  BasicValue value(1);
  BasicValue* nullPointer = nullptr;

  // Check that a non-null pointer translates into a Some value.
  Maybe<BasicValue> mayValue = ToMaybe(&value);
  static_assert(std::is_same_v<Maybe<BasicValue>, decltype(ToMaybe(&value))>,
                "ToMaybe should return a Maybe<BasicValue>");
  MOZ_RELEASE_ASSERT(mayValue.isSome());
  MOZ_RELEASE_ASSERT(mayValue->GetTag() == 1);
  MOZ_RELEASE_ASSERT(mayValue->GetStatus() == eWasCopyConstructed);
  MOZ_RELEASE_ASSERT(value.GetStatus() != eWasMovedFrom);

  // Check that a null pointer translates into a Nothing value.
  mayValue = ToMaybe(nullPointer);
  static_assert(
      std::is_same_v<Maybe<BasicValue>, decltype(ToMaybe(nullPointer))>,
      "ToMaybe should return a Maybe<BasicValue>");
  MOZ_RELEASE_ASSERT(mayValue.isNothing());

  return true;
}

static bool TestComparisonOperators() {
  Maybe<BasicValue> nothingValue = Nothing();
  Maybe<BasicValue> anotherNothingValue = Nothing();
  Maybe<BasicValue> oneValue = Some(BasicValue(1));
  Maybe<BasicValue> anotherOneValue = Some(BasicValue(1));
  Maybe<BasicValue> twoValue = Some(BasicValue(2));

  // Check equality.
  MOZ_RELEASE_ASSERT(nothingValue == anotherNothingValue);
  MOZ_RELEASE_ASSERT(oneValue == anotherOneValue);

  // Check inequality.
  MOZ_RELEASE_ASSERT(nothingValue != oneValue);
  MOZ_RELEASE_ASSERT(oneValue != nothingValue);
  MOZ_RELEASE_ASSERT(oneValue != twoValue);

  // Check '<'.
  MOZ_RELEASE_ASSERT(nothingValue < oneValue);
  MOZ_RELEASE_ASSERT(oneValue < twoValue);

  // Check '<='.
  MOZ_RELEASE_ASSERT(nothingValue <= anotherNothingValue);
  MOZ_RELEASE_ASSERT(nothingValue <= oneValue);
  MOZ_RELEASE_ASSERT(oneValue <= oneValue);
  MOZ_RELEASE_ASSERT(oneValue <= twoValue);

  // Check '>'.
  MOZ_RELEASE_ASSERT(oneValue > nothingValue);
  MOZ_RELEASE_ASSERT(twoValue > oneValue);

  // Check '>='.
  MOZ_RELEASE_ASSERT(nothingValue >= anotherNothingValue);
  MOZ_RELEASE_ASSERT(oneValue >= nothingValue);
  MOZ_RELEASE_ASSERT(oneValue >= oneValue);
  MOZ_RELEASE_ASSERT(twoValue >= oneValue);

  return true;
}

// Check that Maybe<> can wrap a superclass that happens to also be a concrete
// class (i.e. that the compiler doesn't warn when we invoke the superclass's
// destructor explicitly in |reset()|.
class MySuperClass {
  virtual void VirtualMethod() { /* do nothing */
  }
};

class MyDerivedClass : public MySuperClass {
  void VirtualMethod() override { /* do nothing */
  }
};

static bool TestVirtualFunction() {
  Maybe<MySuperClass> super;
  super.emplace();
  super.reset();

  Maybe<MyDerivedClass> derived;
  derived.emplace();
  derived.reset();

  // If this compiles successfully, we've passed.
  return true;
}

static Maybe<int*> ReturnSomeNullptr() { return Some(nullptr); }

struct D {
  explicit D(const Maybe<int*>&) {}
};

static bool TestSomeNullptrConversion() {
  Maybe<int*> m1 = Some(nullptr);
  MOZ_RELEASE_ASSERT(m1.isSome());
  MOZ_RELEASE_ASSERT(m1);
  MOZ_RELEASE_ASSERT(!*m1);

  auto m2 = ReturnSomeNullptr();
  MOZ_RELEASE_ASSERT(m2.isSome());
  MOZ_RELEASE_ASSERT(m2);
  MOZ_RELEASE_ASSERT(!*m2);

  Maybe<decltype(nullptr)> m3 = Some(nullptr);
  MOZ_RELEASE_ASSERT(m3.isSome());
  MOZ_RELEASE_ASSERT(m3);
  MOZ_RELEASE_ASSERT(*m3 == nullptr);

  D d(Some(nullptr));

  return true;
}

struct Base {};
struct Derived : Base {};

static Maybe<Base*> ReturnDerivedPointer() {
  Derived* d = nullptr;
  return Some(d);
}

struct ExplicitConstructorBasePointer {
  explicit ExplicitConstructorBasePointer(const Maybe<Base*>&) {}
};

static bool TestSomePointerConversion() {
  Base base;
  Derived derived;

  Maybe<Base*> m1 = Some(&derived);
  MOZ_RELEASE_ASSERT(m1.isSome());
  MOZ_RELEASE_ASSERT(m1);
  MOZ_RELEASE_ASSERT(*m1 == &derived);

  auto m2 = ReturnDerivedPointer();
  MOZ_RELEASE_ASSERT(m2.isSome());
  MOZ_RELEASE_ASSERT(m2);
  MOZ_RELEASE_ASSERT(*m2 == nullptr);

  Maybe<Base*> m3 = Some(&base);
  MOZ_RELEASE_ASSERT(m3.isSome());
  MOZ_RELEASE_ASSERT(m3);
  MOZ_RELEASE_ASSERT(*m3 == &base);

  auto s1 = Some(&derived);
  Maybe<Base*> c1(s1);
  MOZ_RELEASE_ASSERT(c1.isSome());
  MOZ_RELEASE_ASSERT(c1);
  MOZ_RELEASE_ASSERT(*c1 == &derived);

  ExplicitConstructorBasePointer ecbp(Some(&derived));

  return true;
}

struct SourceType1 {
  int mTag;

  operator int() const { return mTag; }
};
struct DestType {
  int mTag;
  Status mStatus;

  DestType() : mTag(0), mStatus(eWasDefaultConstructed) {}

  MOZ_IMPLICIT DestType(int aTag) : mTag(aTag), mStatus(eWasConstructed) {}

  MOZ_IMPLICIT DestType(SourceType1&& aSrc)
      : mTag(aSrc.mTag), mStatus(eWasMoveConstructed) {}

  MOZ_IMPLICIT DestType(const SourceType1& aSrc)
      : mTag(aSrc.mTag), mStatus(eWasCopyConstructed) {}

  DestType& operator=(int aTag) {
    mTag = aTag;
    mStatus = eWasAssigned;
    return *this;
  }

  DestType& operator=(SourceType1&& aSrc) {
    mTag = aSrc.mTag;
    mStatus = eWasMoveAssigned;
    return *this;
  }

  DestType& operator=(const SourceType1& aSrc) {
    mTag = aSrc.mTag;
    mStatus = eWasCopyAssigned;
    return *this;
  }
};
struct SourceType2 {
  int mTag;

  operator DestType() const& {
    DestType result;
    result.mTag = mTag;
    result.mStatus = eWasCopiedFrom;
    return result;
  }

  operator DestType() && {
    DestType result;
    result.mTag = mTag;
    result.mStatus = eWasMovedFrom;
    return result;
  }
};

static bool TestTypeConversion() {
  {
    Maybe<SourceType1> src = Some(SourceType1{1});
    Maybe<DestType> dest = src;
    MOZ_RELEASE_ASSERT(src.isSome() && src->mTag == 1);
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 1);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasCopyConstructed);

    src = Some(SourceType1{2});
    dest = src;
    MOZ_RELEASE_ASSERT(src.isSome() && src->mTag == 2);
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 2);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasCopyAssigned);
  }

  {
    Maybe<SourceType1> src = Some(SourceType1{1});
    Maybe<DestType> dest = std::move(src);
    MOZ_RELEASE_ASSERT(src.isNothing());
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 1);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasMoveConstructed);

    src = Some(SourceType1{2});
    dest = std::move(src);
    MOZ_RELEASE_ASSERT(src.isNothing());
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 2);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasMoveAssigned);
  }

  {
    Maybe<SourceType2> src = Some(SourceType2{1});
    Maybe<DestType> dest = src;
    MOZ_RELEASE_ASSERT(src.isSome() && src->mTag == 1);
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 1);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasCopiedFrom);

    src = Some(SourceType2{2});
    dest = src;
    MOZ_RELEASE_ASSERT(src.isSome() && src->mTag == 2);
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 2);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasCopiedFrom);
  }

  {
    Maybe<SourceType2> src = Some(SourceType2{1});
    Maybe<DestType> dest = std::move(src);
    MOZ_RELEASE_ASSERT(src.isNothing());
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 1);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasMovedFrom);

    src = Some(SourceType2{2});
    dest = std::move(src);
    MOZ_RELEASE_ASSERT(src.isNothing());
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 2);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasMovedFrom);
  }

  {
    Maybe<int> src = Some(1);
    Maybe<DestType> dest = src;
    MOZ_RELEASE_ASSERT(src.isSome() && *src == 1);
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 1);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasConstructed);

    src = Some(2);
    dest = src;
    MOZ_RELEASE_ASSERT(src.isSome() && *src == 2);
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 2);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasAssigned);
  }

  {
    Maybe<int> src = Some(1);
    Maybe<DestType> dest = std::move(src);
    MOZ_RELEASE_ASSERT(src.isNothing());
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 1);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasConstructed);

    src = Some(2);
    dest = std::move(src);
    MOZ_RELEASE_ASSERT(src.isNothing());
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 2);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasAssigned);
  }

  {
    Maybe<SourceType1> src = Some(SourceType1{1});
    Maybe<int> dest = src;
    MOZ_RELEASE_ASSERT(src.isSome() && src->mTag == 1);
    MOZ_RELEASE_ASSERT(dest.isSome() && *dest == 1);

    src = Some(SourceType1{2});
    dest = src;
    MOZ_RELEASE_ASSERT(src.isSome() && src->mTag == 2);
    MOZ_RELEASE_ASSERT(dest.isSome() && *dest == 2);
  }

  {
    Maybe<SourceType1> src = Some(SourceType1{1});
    Maybe<int> dest = std::move(src);
    MOZ_RELEASE_ASSERT(src.isNothing());
    MOZ_RELEASE_ASSERT(dest.isSome() && *dest == 1);

    src = Some(SourceType1{2});
    dest = std::move(src);
    MOZ_RELEASE_ASSERT(src.isNothing());
    MOZ_RELEASE_ASSERT(dest.isSome() && *dest == 2);
  }

  {
    Maybe<size_t> src = Some(1);
    Maybe<char16_t> dest = src;
    MOZ_RELEASE_ASSERT(src.isSome() && *src == 1);
    MOZ_RELEASE_ASSERT(dest.isSome() && *dest == 1);

    src = Some(2);
    dest = src;
    MOZ_RELEASE_ASSERT(src.isSome() && *src == 2);
    MOZ_RELEASE_ASSERT(dest.isSome() && *dest == 2);
  }

  {
    Maybe<size_t> src = Some(1);
    Maybe<char16_t> dest = std::move(src);
    MOZ_RELEASE_ASSERT(src.isNothing());
    MOZ_RELEASE_ASSERT(dest.isSome() && *dest == 1);

    src = Some(2);
    dest = std::move(src);
    MOZ_RELEASE_ASSERT(src.isNothing());
    MOZ_RELEASE_ASSERT(dest.isSome() && *dest == 2);
  }

  return true;
}

static bool TestReference() {
  static_assert(std::is_literal_type_v<Maybe<int&>>);
  static_assert(std::is_trivially_copy_constructible_v<Maybe<int&>>);
  static_assert(std::is_trivially_copy_assignable_v<Maybe<int&>>);

  {
    Maybe<int&> defaultConstructed;

    MOZ_RELEASE_ASSERT(defaultConstructed.isNothing());
    MOZ_RELEASE_ASSERT(!defaultConstructed.isSome());
    MOZ_RELEASE_ASSERT(!defaultConstructed);
  }

  {
    Maybe<int&> nothing = Nothing();

    MOZ_RELEASE_ASSERT(nothing.isNothing());
    MOZ_RELEASE_ASSERT(!nothing.isSome());
    MOZ_RELEASE_ASSERT(!nothing);
  }

  {
    int foo = 42;
    Maybe<int&> some = SomeRef(foo);

    MOZ_RELEASE_ASSERT(!some.isNothing());
    MOZ_RELEASE_ASSERT(some.isSome());
    MOZ_RELEASE_ASSERT(some);
    MOZ_RELEASE_ASSERT(&some.ref() == &foo);

    some.ref()++;
    MOZ_RELEASE_ASSERT(43 == foo);

    (*some)++;
    MOZ_RELEASE_ASSERT(44 == foo);
  }

  {
    int foo = 42;
    Maybe<int&> some;
    some.emplace(foo);

    MOZ_RELEASE_ASSERT(!some.isNothing());
    MOZ_RELEASE_ASSERT(some.isSome());
    MOZ_RELEASE_ASSERT(some);
    MOZ_RELEASE_ASSERT(&some.ref() == &foo);

    some.ref()++;
    MOZ_RELEASE_ASSERT(43 == foo);
  }

  {
    Maybe<int&> defaultConstructed;
    defaultConstructed.reset();

    MOZ_RELEASE_ASSERT(defaultConstructed.isNothing());
    MOZ_RELEASE_ASSERT(!defaultConstructed.isSome());
    MOZ_RELEASE_ASSERT(!defaultConstructed);
  }

  {
    int foo = 42;
    Maybe<int&> some = SomeRef(foo);
    some.reset();

    MOZ_RELEASE_ASSERT(some.isNothing());
    MOZ_RELEASE_ASSERT(!some.isSome());
    MOZ_RELEASE_ASSERT(!some);
  }

  {
    int foo = 42;
    Maybe<int&> some = SomeRef(foo);

    auto& applied = some.apply([](int& ref) { ref++; });

    MOZ_RELEASE_ASSERT(&some == &applied);
    MOZ_RELEASE_ASSERT(43 == foo);
  }

  {
    Maybe<int&> nothing;

    auto& applied = nothing.apply([](int& ref) { ref++; });

    MOZ_RELEASE_ASSERT(&nothing == &applied);
  }

  {
    int foo = 42;
    Maybe<int&> some = SomeRef(foo);

    auto mapped = some.map([](int& ref) { return &ref; });
    static_assert(std::is_same_v<decltype(mapped), Maybe<int*>>);

    MOZ_RELEASE_ASSERT(&foo == *mapped);
  }

  {
    Maybe<int&> nothing;

    auto mapped = nothing.map([](int& ref) { return &ref; });

    MOZ_RELEASE_ASSERT(mapped.isNothing());
    MOZ_RELEASE_ASSERT(!mapped.isSome());
    MOZ_RELEASE_ASSERT(!mapped);
  }

  return true;
}

// These are quasi-implementation details, but we assert them here to prevent
// backsliding to earlier times when Maybe<T> for smaller T took up more space
// than T's alignment required.

static_assert(sizeof(Maybe<char>) == 2 * sizeof(char),
              "Maybe<char> shouldn't bloat at all ");
static_assert(sizeof(Maybe<bool>) <= 2 * sizeof(bool),
              "Maybe<bool> shouldn't bloat");
static_assert(sizeof(Maybe<int>) <= 2 * sizeof(int),
              "Maybe<int> shouldn't bloat");
static_assert(sizeof(Maybe<long>) <= 2 * sizeof(long),
              "Maybe<long> shouldn't bloat");
static_assert(sizeof(Maybe<double>) <= 2 * sizeof(double),
              "Maybe<double> shouldn't bloat");
static_assert(sizeof(Maybe<int&>) == sizeof(int*));

int main() {
  RUN_TEST(TestBasicFeatures);
  RUN_TEST(TestCopyAndMove);
  RUN_TEST(TestFunctionalAccessors);
  RUN_TEST(TestApply);
  RUN_TEST(TestMap);
  RUN_TEST(TestToMaybe);
  RUN_TEST(TestComparisonOperators);
  RUN_TEST(TestVirtualFunction);
  RUN_TEST(TestSomeNullptrConversion);
  RUN_TEST(TestSomePointerConversion);
  RUN_TEST(TestTypeConversion);
  RUN_TEST(TestReference);

  return 0;
}
