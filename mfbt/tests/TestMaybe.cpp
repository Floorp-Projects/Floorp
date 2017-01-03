/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <utility>

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Compiler.h"
#include "mozilla/Maybe.h"
#include "mozilla/Move.h"
#include "mozilla/Types.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/UniquePtr.h"

using mozilla::IsSame;
using mozilla::Maybe;
using mozilla::Move;
using mozilla::Nothing;
using mozilla::Some;
using mozilla::Swap;
using mozilla::ToMaybe;
using mozilla::UniquePtr;

#define RUN_TEST(t) \
  do { \
    bool cond = (t()); \
    if (!cond) \
      return 1; \
    cond = AllDestructorsWereCalled(); \
    MOZ_ASSERT(cond, "Failed to destroy all objects during test: " #t); \
    if (!cond) \
      return 1; \
  } while (false)

enum Status
{
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

static bool AllDestructorsWereCalled()
{
  return sUndestroyedObjects == 0;
}

struct BasicValue
{
  BasicValue()
    : mStatus(eWasDefaultConstructed)
    , mTag(0)
  {
    ++sUndestroyedObjects;
  }

  explicit BasicValue(int aTag)
    : mStatus(eWasConstructed)
    , mTag(aTag)
  {
    ++sUndestroyedObjects;
  }

  BasicValue(const BasicValue& aOther)
    : mStatus(eWasCopyConstructed)
    , mTag(aOther.mTag)
  {
    ++sUndestroyedObjects;
  }

  BasicValue(BasicValue&& aOther)
    : mStatus(eWasMoveConstructed)
    , mTag(aOther.mTag)
  {
    ++sUndestroyedObjects;
    aOther.mStatus = eWasMovedFrom;
    aOther.mTag = 0;
  }

  ~BasicValue() { --sUndestroyedObjects; }

  BasicValue& operator=(const BasicValue& aOther)
  {
    mStatus = eWasCopyAssigned;
    mTag = aOther.mTag;
    return *this;
  }

  BasicValue& operator=(BasicValue&& aOther)
  {
    mStatus = eWasMoveAssigned;
    mTag = aOther.mTag;
    aOther.mStatus = eWasMovedFrom;
    aOther.mTag = 0;
    return *this;
  }

  bool operator==(const BasicValue& aOther) const
  {
    return mTag == aOther.mTag;
  }

  bool operator<(const BasicValue& aOther) const
  {
    return mTag < aOther.mTag;
  }

  Status GetStatus() const { return mStatus; }
  void SetTag(int aValue) { mTag = aValue; }
  int GetTag() const { return mTag; }

private:
  Status mStatus;
  int mTag;
};

struct UncopyableValue
{
  UncopyableValue()
    : mStatus(eWasDefaultConstructed)
  {
    ++sUndestroyedObjects;
  }

  UncopyableValue(UncopyableValue&& aOther)
    : mStatus(eWasMoveConstructed)
  {
    ++sUndestroyedObjects;
    aOther.mStatus = eWasMovedFrom;
  }

  ~UncopyableValue() { --sUndestroyedObjects; }

  UncopyableValue& operator=(UncopyableValue&& aOther)
  {
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

struct UnmovableValue
{
  UnmovableValue()
    : mStatus(eWasDefaultConstructed)
  {
    ++sUndestroyedObjects;
  }

  UnmovableValue(const UnmovableValue& aOther)
    : mStatus(eWasCopyConstructed)
  {
    ++sUndestroyedObjects;
  }

  ~UnmovableValue() { --sUndestroyedObjects; }

  UnmovableValue& operator=(const UnmovableValue& aOther)
  {
    mStatus = eWasCopyAssigned;
    return *this;
  }

  Status GetStatus() { return mStatus; }

private:
  UnmovableValue(UnmovableValue&& aOther) = delete;
  UnmovableValue& operator=(UnmovableValue&& aOther) = delete;

  Status mStatus;
};

struct UncopyableUnmovableValue
{
  UncopyableUnmovableValue()
    : mStatus(eWasDefaultConstructed)
  {
    ++sUndestroyedObjects;
  }

  explicit UncopyableUnmovableValue(int)
    : mStatus(eWasConstructed)
  {
    ++sUndestroyedObjects;
  }

  ~UncopyableUnmovableValue() { --sUndestroyedObjects; }

  Status GetStatus() { return mStatus; }

private:
  UncopyableUnmovableValue(const UncopyableUnmovableValue& aOther) = delete;
  UncopyableUnmovableValue& operator=(const UncopyableUnmovableValue& aOther) = delete;
  UncopyableUnmovableValue(UncopyableUnmovableValue&& aOther) = delete;
  UncopyableUnmovableValue& operator=(UncopyableUnmovableValue&& aOther) = delete;

  Status mStatus;
};

static bool
TestBasicFeatures()
{
  // Check that a Maybe<T> is initialized to Nothing.
  Maybe<BasicValue> mayValue;
  static_assert(IsSame<BasicValue, decltype(mayValue)::ValueType>::value,
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
  static_assert(IsSame<BasicValue, decltype(mayValue.value())>::value,
                "value() should return a BasicValue");
  MOZ_RELEASE_ASSERT(mayValue.ref() == BasicValue());
  static_assert(IsSame<BasicValue&, decltype(mayValue.ref())>::value,
                "ref() should return a BasicValue&");
  MOZ_RELEASE_ASSERT(mayValue.ptr() != nullptr);
  static_assert(IsSame<BasicValue*, decltype(mayValue.ptr())>::value,
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
  static_assert(IsSame<BasicValue, decltype(mayValueCRef.value())>::value,
                "value() should return a BasicValue");
  MOZ_RELEASE_ASSERT(mayValueCRef.ref() == BasicValue());
  static_assert(IsSame<const BasicValue&,
                       decltype(mayValueCRef.ref())>::value,
                "ref() should return a const BasicValue&");
  MOZ_RELEASE_ASSERT(mayValueCRef.ptr() != nullptr);
  static_assert(IsSame<const BasicValue*,
                       decltype(mayValueCRef.ptr())>::value,
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

static bool
TestCopyAndMove()
{
  // Check that we get moves when possible for types that can support both moves
  // and copies.
  Maybe<BasicValue> mayBasicValue = Some(BasicValue(1));
  MOZ_RELEASE_ASSERT(mayBasicValue->GetStatus() == eWasMoveConstructed);
  MOZ_RELEASE_ASSERT(mayBasicValue->GetTag() == 1);
  mayBasicValue = Some(BasicValue(2));
  MOZ_RELEASE_ASSERT(mayBasicValue->GetStatus() == eWasMoveAssigned);
  MOZ_RELEASE_ASSERT(mayBasicValue->GetTag() == 2);
  mayBasicValue.reset();
  mayBasicValue.emplace(BasicValue(3));
  MOZ_RELEASE_ASSERT(mayBasicValue->GetStatus() == eWasMoveConstructed);
  MOZ_RELEASE_ASSERT(mayBasicValue->GetTag() == 3);

  // Check that we get copies when moves aren't possible.
  Maybe<BasicValue> mayBasicValue2 = Some(*mayBasicValue);
  MOZ_RELEASE_ASSERT(mayBasicValue2->GetStatus() == eWasCopyConstructed);
  MOZ_RELEASE_ASSERT(mayBasicValue2->GetTag() == 3);
  mayBasicValue->SetTag(4);
  mayBasicValue2 = mayBasicValue;
  // This test should work again when we fix bug 1052940.
  //MOZ_RELEASE_ASSERT(mayBasicValue2->GetStatus() == eWasCopyAssigned);
  MOZ_RELEASE_ASSERT(mayBasicValue2->GetTag() == 4);
  mayBasicValue->SetTag(5);
  mayBasicValue2.reset();
  mayBasicValue2.emplace(*mayBasicValue);
  MOZ_RELEASE_ASSERT(mayBasicValue2->GetStatus() == eWasCopyConstructed);
  MOZ_RELEASE_ASSERT(mayBasicValue2->GetTag() == 5);

  // Check that Move() works. (Another sanity check for move support.)
  Maybe<BasicValue> mayBasicValue3 = Some(Move(*mayBasicValue));
  MOZ_RELEASE_ASSERT(mayBasicValue3->GetStatus() == eWasMoveConstructed);
  MOZ_RELEASE_ASSERT(mayBasicValue3->GetTag() == 5);
  MOZ_RELEASE_ASSERT(mayBasicValue->GetStatus() == eWasMovedFrom);
  mayBasicValue2->SetTag(6);
  mayBasicValue3 = Some(Move(*mayBasicValue2));
  MOZ_RELEASE_ASSERT(mayBasicValue3->GetStatus() == eWasMoveAssigned);
  MOZ_RELEASE_ASSERT(mayBasicValue3->GetTag() == 6);
  MOZ_RELEASE_ASSERT(mayBasicValue2->GetStatus() == eWasMovedFrom);
  Maybe<BasicValue> mayBasicValue4;
  mayBasicValue4.emplace(Move(*mayBasicValue3));
  MOZ_RELEASE_ASSERT(mayBasicValue4->GetStatus() == eWasMoveConstructed);
  MOZ_RELEASE_ASSERT(mayBasicValue4->GetTag() == 6);
  MOZ_RELEASE_ASSERT(mayBasicValue3->GetStatus() == eWasMovedFrom);

  // Check that we always get copies for types that don't support moves.
  // XXX(seth): These tests fail but probably shouldn't. For now we'll just
  // consider using Maybe with types that allow copies but have deleted or
  // private move constructors, or which do not support copy assignment, to
  // be supported only to the extent that we need for existing code to work.
  // These tests should work again when we fix bug 1052940.
  /*
  Maybe<UnmovableValue> mayUnmovableValue = Some(UnmovableValue());
  MOZ_RELEASE_ASSERT(mayUnmovableValue->GetStatus() == eWasCopyConstructed);
  mayUnmovableValue = Some(UnmovableValue());
  MOZ_RELEASE_ASSERT(mayUnmovableValue->GetStatus() == eWasCopyAssigned);
  mayUnmovableValue.reset();
  mayUnmovableValue.emplace(UnmovableValue());
  MOZ_RELEASE_ASSERT(mayUnmovableValue->GetStatus() == eWasCopyConstructed);
  */

  // Check that types that only support moves, but not copies, work.
  Maybe<UncopyableValue> mayUncopyableValue = Some(UncopyableValue());
  MOZ_RELEASE_ASSERT(mayUncopyableValue->GetStatus() == eWasMoveConstructed);
  mayUncopyableValue = Some(UncopyableValue());
  MOZ_RELEASE_ASSERT(mayUncopyableValue->GetStatus() == eWasMoveAssigned);
  mayUncopyableValue.reset();
  mayUncopyableValue.emplace(UncopyableValue());
  MOZ_RELEASE_ASSERT(mayUncopyableValue->GetStatus() == eWasMoveConstructed);

  // Check that types that support neither moves or copies work.
  Maybe<UncopyableUnmovableValue> mayUncopyableUnmovableValue;
  mayUncopyableUnmovableValue.emplace();
  MOZ_RELEASE_ASSERT(mayUncopyableUnmovableValue->GetStatus() == eWasDefaultConstructed);
  mayUncopyableUnmovableValue.reset();
  mayUncopyableUnmovableValue.emplace(0);
  MOZ_RELEASE_ASSERT(mayUncopyableUnmovableValue->GetStatus() == eWasConstructed);

  return true;
}

static BasicValue* sStaticBasicValue = nullptr;

static BasicValue
MakeBasicValue()
{
  return BasicValue(9);
}

static BasicValue&
MakeBasicValueRef()
{
  return *sStaticBasicValue;
}

static BasicValue*
MakeBasicValuePtr()
{
  return sStaticBasicValue;
}

static bool
TestFunctionalAccessors()
{
  BasicValue value(9);
  sStaticBasicValue = new BasicValue(9);

  // Check that the 'some' case of functional accessors works.
  Maybe<BasicValue> someValue = Some(BasicValue(3));
  MOZ_RELEASE_ASSERT(someValue.valueOr(value) == BasicValue(3));
  static_assert(IsSame<BasicValue,
                       decltype(someValue.valueOr(value))>::value,
                "valueOr should return a BasicValue");
  MOZ_RELEASE_ASSERT(someValue.valueOrFrom(&MakeBasicValue) == BasicValue(3));
  static_assert(IsSame<BasicValue,
                       decltype(someValue.valueOrFrom(&MakeBasicValue))>::value,
                "valueOrFrom should return a BasicValue");
  MOZ_RELEASE_ASSERT(someValue.ptrOr(&value) != &value);
  static_assert(IsSame<BasicValue*,
                       decltype(someValue.ptrOr(&value))>::value,
                "ptrOr should return a BasicValue*");
  MOZ_RELEASE_ASSERT(*someValue.ptrOrFrom(&MakeBasicValuePtr) == BasicValue(3));
  static_assert(IsSame<BasicValue*,
                       decltype(someValue.ptrOrFrom(&MakeBasicValuePtr))>::value,
                "ptrOrFrom should return a BasicValue*");
  MOZ_RELEASE_ASSERT(someValue.refOr(value) == BasicValue(3));
  static_assert(IsSame<BasicValue&,
                       decltype(someValue.refOr(value))>::value,
                "refOr should return a BasicValue&");
  MOZ_RELEASE_ASSERT(someValue.refOrFrom(&MakeBasicValueRef) == BasicValue(3));
  static_assert(IsSame<BasicValue&,
                       decltype(someValue.refOrFrom(&MakeBasicValueRef))>::value,
                "refOrFrom should return a BasicValue&");

  // Check that the 'some' case works through a const reference.
  const Maybe<BasicValue>& someValueCRef = someValue;
  MOZ_RELEASE_ASSERT(someValueCRef.valueOr(value) == BasicValue(3));
  static_assert(IsSame<BasicValue,
                       decltype(someValueCRef.valueOr(value))>::value,
                "valueOr should return a BasicValue");
  MOZ_RELEASE_ASSERT(someValueCRef.valueOrFrom(&MakeBasicValue) == BasicValue(3));
  static_assert(IsSame<BasicValue,
                       decltype(someValueCRef.valueOrFrom(&MakeBasicValue))>::value,
                "valueOrFrom should return a BasicValue");
  MOZ_RELEASE_ASSERT(someValueCRef.ptrOr(&value) != &value);
  static_assert(IsSame<const BasicValue*,
                       decltype(someValueCRef.ptrOr(&value))>::value,
                "ptrOr should return a const BasicValue*");
  MOZ_RELEASE_ASSERT(*someValueCRef.ptrOrFrom(&MakeBasicValuePtr) == BasicValue(3));
  static_assert(IsSame<const BasicValue*,
                       decltype(someValueCRef.ptrOrFrom(&MakeBasicValuePtr))>::value,
                "ptrOrFrom should return a const BasicValue*");
  MOZ_RELEASE_ASSERT(someValueCRef.refOr(value) == BasicValue(3));
  static_assert(IsSame<const BasicValue&,
                       decltype(someValueCRef.refOr(value))>::value,
                "refOr should return a const BasicValue&");
  MOZ_RELEASE_ASSERT(someValueCRef.refOrFrom(&MakeBasicValueRef) == BasicValue(3));
  static_assert(IsSame<const BasicValue&,
                       decltype(someValueCRef.refOrFrom(&MakeBasicValueRef))>::value,
                "refOrFrom should return a const BasicValue&");

  // Check that the 'none' case of functional accessors works.
  Maybe<BasicValue> noneValue;
  MOZ_RELEASE_ASSERT(noneValue.valueOr(value) == BasicValue(9));
  static_assert(IsSame<BasicValue,
                       decltype(noneValue.valueOr(value))>::value,
                "valueOr should return a BasicValue");
  MOZ_RELEASE_ASSERT(noneValue.valueOrFrom(&MakeBasicValue) == BasicValue(9));
  static_assert(IsSame<BasicValue,
                       decltype(noneValue.valueOrFrom(&MakeBasicValue))>::value,
                "valueOrFrom should return a BasicValue");
  MOZ_RELEASE_ASSERT(noneValue.ptrOr(&value) == &value);
  static_assert(IsSame<BasicValue*,
                       decltype(noneValue.ptrOr(&value))>::value,
                "ptrOr should return a BasicValue*");
  MOZ_RELEASE_ASSERT(*noneValue.ptrOrFrom(&MakeBasicValuePtr) == BasicValue(9));
  static_assert(IsSame<BasicValue*,
                       decltype(noneValue.ptrOrFrom(&MakeBasicValuePtr))>::value,
                "ptrOrFrom should return a BasicValue*");
  MOZ_RELEASE_ASSERT(noneValue.refOr(value) == BasicValue(9));
  static_assert(IsSame<BasicValue&,
                       decltype(noneValue.refOr(value))>::value,
                "refOr should return a BasicValue&");
  MOZ_RELEASE_ASSERT(noneValue.refOrFrom(&MakeBasicValueRef) == BasicValue(9));
  static_assert(IsSame<BasicValue&,
                       decltype(noneValue.refOrFrom(&MakeBasicValueRef))>::value,
                "refOrFrom should return a BasicValue&");

  // Check that the 'none' case works through a const reference.
  const Maybe<BasicValue>& noneValueCRef = noneValue;
  MOZ_RELEASE_ASSERT(noneValueCRef.valueOr(value) == BasicValue(9));
  static_assert(IsSame<BasicValue,
                       decltype(noneValueCRef.valueOr(value))>::value,
                "valueOr should return a BasicValue");
  MOZ_RELEASE_ASSERT(noneValueCRef.valueOrFrom(&MakeBasicValue) == BasicValue(9));
  static_assert(IsSame<BasicValue,
                       decltype(noneValueCRef.valueOrFrom(&MakeBasicValue))>::value,
                "valueOrFrom should return a BasicValue");
  MOZ_RELEASE_ASSERT(noneValueCRef.ptrOr(&value) == &value);
  static_assert(IsSame<const BasicValue*,
                       decltype(noneValueCRef.ptrOr(&value))>::value,
                "ptrOr should return a const BasicValue*");
  MOZ_RELEASE_ASSERT(*noneValueCRef.ptrOrFrom(&MakeBasicValuePtr) == BasicValue(9));
  static_assert(IsSame<const BasicValue*,
                       decltype(noneValueCRef.ptrOrFrom(&MakeBasicValuePtr))>::value,
                "ptrOrFrom should return a const BasicValue*");
  MOZ_RELEASE_ASSERT(noneValueCRef.refOr(value) == BasicValue(9));
  static_assert(IsSame<const BasicValue&,
                       decltype(noneValueCRef.refOr(value))>::value,
                "refOr should return a const BasicValue&");
  MOZ_RELEASE_ASSERT(noneValueCRef.refOrFrom(&MakeBasicValueRef) == BasicValue(9));
  static_assert(IsSame<const BasicValue&,
                       decltype(noneValueCRef.refOrFrom(&MakeBasicValueRef))>::value,
                "refOrFrom should return a const BasicValue&");

  // Clean up so the undestroyed objects count stays accurate.
  delete sStaticBasicValue;
  sStaticBasicValue = nullptr;

  return true;
}

static bool gFunctionWasApplied = false;

static void
IncrementTag(BasicValue& aValue)
{
  gFunctionWasApplied = true;
  aValue.SetTag(aValue.GetTag() + 1);
}

static void
AccessValue(const BasicValue&)
{
  gFunctionWasApplied = true;
}

struct IncrementTagFunctor
{
  IncrementTagFunctor() : mBy(1) { }

  void operator()(BasicValue& aValue)
  {
    aValue.SetTag(aValue.GetTag() + mBy.GetTag());
  }

  BasicValue mBy;
};

static bool
TestApply()
{
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
  mayValueCRef.apply([&](const BasicValue& aVal) { gFunctionWasApplied = true; });
  MOZ_RELEASE_ASSERT(gFunctionWasApplied == true);

  return true;
}

static int
TimesTwo(const BasicValue& aValue)
{
  return aValue.GetTag() * 2;
}

static int
TimesTwoAndResetOriginal(BasicValue& aValue)
{
  int tag = aValue.GetTag();
  aValue.SetTag(1);
  return tag * 2;
}

struct MultiplyTagFunctor
{
  MultiplyTagFunctor() : mBy(2) { }

  int operator()(BasicValue& aValue)
  {
    return aValue.GetTag() * mBy.GetTag();
  }

  BasicValue mBy;
};

static bool
TestMap()
{
  // Check that map handles the 'Nothing' case.
  Maybe<BasicValue> mayValue;
  MOZ_RELEASE_ASSERT(mayValue.map(&TimesTwo) == Nothing());
  static_assert(IsSame<Maybe<int>,
                       decltype(mayValue.map(&TimesTwo))>::value,
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
  static_assert(IsSame<Maybe<int>,
                       decltype(mayValueCRef.map(&TimesTwo))>::value,
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
    mayValue.map([&](const BasicValue& aVal) {
      return aVal.GetTag() * two;
    });
  MOZ_RELEASE_ASSERT(mappedValue == Some(4));
  mappedValue =
    mayValue.map([=](const BasicValue& aVal) {
      return aVal.GetTag() * two;
    });
  MOZ_RELEASE_ASSERT(mappedValue == Some(4));
  mappedValue =
    mayValueCRef.map([&](const BasicValue& aVal) {
      return aVal.GetTag() * two;
    });
  MOZ_RELEASE_ASSERT(mappedValue == Some(4));

  return true;
}

static bool
TestToMaybe()
{
  BasicValue value(1);
  BasicValue* nullPointer = nullptr;

  // Check that a non-null pointer translates into a Some value.
  Maybe<BasicValue> mayValue = ToMaybe(&value);
  static_assert(IsSame<Maybe<BasicValue>, decltype(ToMaybe(&value))>::value,
                "ToMaybe should return a Maybe<BasicValue>");
  MOZ_RELEASE_ASSERT(mayValue.isSome());
  MOZ_RELEASE_ASSERT(mayValue->GetTag() == 1);
  MOZ_RELEASE_ASSERT(mayValue->GetStatus() == eWasCopyConstructed);
  MOZ_RELEASE_ASSERT(value.GetStatus() != eWasMovedFrom);

  // Check that a null pointer translates into a Nothing value.
  mayValue = ToMaybe(nullPointer);
  static_assert(IsSame<Maybe<BasicValue>, decltype(ToMaybe(nullPointer))>::value,
                "ToMaybe should return a Maybe<BasicValue>");
  MOZ_RELEASE_ASSERT(mayValue.isNothing());

  return true;
}

static bool
TestComparisonOperators()
{
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
  virtual void VirtualMethod() { /* do nothing */ }
};

class MyDerivedClass : public MySuperClass {
  void VirtualMethod() override { /* do nothing */ }
};

static bool
TestVirtualFunction() {
  Maybe<MySuperClass> super;
  super.emplace();
  super.reset();

  Maybe<MyDerivedClass> derived;
  derived.emplace();
  derived.reset();

  // If this compiles successfully, we've passed.
  return true;
}

static Maybe<int*>
ReturnSomeNullptr()
{
  return Some(nullptr);
}

struct D
{
  explicit D(Maybe<int*>) {}
};

static bool
TestSomeNullptrConversion()
{
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

static Maybe<Base*>
ReturnDerivedPointer()
{
  Derived* d = nullptr;
  return Some(d);
}

struct ExplicitConstructorBasePointer
{
  explicit ExplicitConstructorBasePointer(Maybe<Base*>) {}
};

static bool
TestSomePointerConversion()
{
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

struct SourceType1
{
  int mTag;

  operator int() const
  {
    return mTag;
  }
};
struct DestType
{
  int mTag;
  Status mStatus;

  DestType()
    : mTag(0)
    , mStatus(eWasDefaultConstructed)
  {}

  MOZ_IMPLICIT DestType(int aTag)
    : mTag(aTag)
    , mStatus(eWasConstructed)
  {}

  MOZ_IMPLICIT DestType(SourceType1&& aSrc)
    : mTag(aSrc.mTag)
    , mStatus(eWasMoveConstructed)
  {}

  MOZ_IMPLICIT DestType(const SourceType1& aSrc)
    : mTag(aSrc.mTag)
    , mStatus(eWasCopyConstructed)
  {}

  DestType& operator=(int aTag)
  {
    mTag = aTag;
    mStatus = eWasAssigned;
    return *this;
  }

  DestType& operator=(SourceType1&& aSrc)
  {
    mTag = aSrc.mTag;
    mStatus = eWasMoveAssigned;
    return *this;
  }

  DestType& operator=(const SourceType1& aSrc)
  {
    mTag = aSrc.mTag;
    mStatus = eWasCopyAssigned;
    return *this;
  }
};
struct SourceType2
{
  int mTag;

  operator DestType() const&
  {
    DestType result;
    result.mTag = mTag;
    result.mStatus = eWasCopiedFrom;
    return result;
  }

  operator DestType() &&
  {
    DestType result;
    result.mTag = mTag;
    result.mStatus = eWasMovedFrom;
    return result;
  }
};

static bool
TestTypeConversion()
{
  {
    Maybe<SourceType1> src = Some(SourceType1 {1});
    Maybe<DestType> dest = src;
    MOZ_RELEASE_ASSERT(src.isSome() && src->mTag == 1);
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 1);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasCopyConstructed);

    src = Some(SourceType1 {2});
    dest = src;
    MOZ_RELEASE_ASSERT(src.isSome() && src->mTag == 2);
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 2);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasCopyAssigned);
  }

  {
    Maybe<SourceType1> src = Some(SourceType1 {1});
    Maybe<DestType> dest = Move(src);
    MOZ_RELEASE_ASSERT(src.isNothing());
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 1);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasMoveConstructed);

    src = Some(SourceType1 {2});
    dest = Move(src);
    MOZ_RELEASE_ASSERT(src.isNothing());
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 2);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasMoveAssigned);
  }

  {
    Maybe<SourceType2> src = Some(SourceType2 {1});
    Maybe<DestType> dest = src;
    MOZ_RELEASE_ASSERT(src.isSome() && src->mTag == 1);
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 1);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasCopiedFrom);

    src = Some(SourceType2 {2});
    dest = src;
    MOZ_RELEASE_ASSERT(src.isSome() && src->mTag == 2);
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 2);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasCopiedFrom);
  }

  {
    Maybe<SourceType2> src = Some(SourceType2 {1});
    Maybe<DestType> dest = Move(src);
    MOZ_RELEASE_ASSERT(src.isNothing());
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 1);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasMovedFrom);

    src = Some(SourceType2 {2});
    dest = Move(src);
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
    Maybe<DestType> dest = Move(src);
    MOZ_RELEASE_ASSERT(src.isNothing());
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 1);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasConstructed);

    src = Some(2);
    dest = Move(src);
    MOZ_RELEASE_ASSERT(src.isNothing());
    MOZ_RELEASE_ASSERT(dest.isSome() && dest->mTag == 2);
    MOZ_RELEASE_ASSERT(dest->mStatus == eWasAssigned);
  }

  {
    Maybe<SourceType1> src = Some(SourceType1 {1});
    Maybe<int> dest = src;
    MOZ_RELEASE_ASSERT(src.isSome() && src->mTag == 1);
    MOZ_RELEASE_ASSERT(dest.isSome() && *dest == 1);

    src = Some(SourceType1 {2});
    dest = src;
    MOZ_RELEASE_ASSERT(src.isSome() && src->mTag == 2);
    MOZ_RELEASE_ASSERT(dest.isSome() && *dest == 2);
  }

  {
    Maybe<SourceType1> src = Some(SourceType1 {1});
    Maybe<int> dest = Move(src);
    MOZ_RELEASE_ASSERT(src.isNothing());
    MOZ_RELEASE_ASSERT(dest.isSome() && *dest == 1);

    src = Some(SourceType1 {2});
    dest = Move(src);
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
    Maybe<char16_t> dest = Move(src);
    MOZ_RELEASE_ASSERT(src.isNothing());
    MOZ_RELEASE_ASSERT(dest.isSome() && *dest == 1);

    src = Some(2);
    dest = Move(src);
    MOZ_RELEASE_ASSERT(src.isNothing());
    MOZ_RELEASE_ASSERT(dest.isSome() && *dest == 2);
  }

  return true;
}

int
main()
{
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

  return 0;
}
