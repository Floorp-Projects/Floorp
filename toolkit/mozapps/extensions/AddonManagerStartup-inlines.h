/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AddonManagerStartup_inlines_h
#define AddonManagerStartup_inlines_h

#include "jsapi.h"
#include "nsJSUtils.h"

#include "mozilla/Maybe.h"
#include "mozilla/Move.h"

namespace mozilla {

class ArrayIterElem;
class PropertyIterElem;


/*****************************************************************************
 * Object iterator base classes
 *****************************************************************************/

template<class T, class PropertyType>
class MOZ_STACK_CLASS BaseIter {
public:
  typedef T SelfType;

  PropertyType begin() const
  {
    PropertyType elem(Self());
    return Move(elem);
  }

  PropertyType end() const
  {
    PropertyType elem(Self());
    return elem.End();
  }

  void* Context() const { return mContext; }

protected:
  BaseIter(JSContext* cx, JS::HandleObject object, void* context = nullptr)
    : mCx(cx)
    , mObject(object)
    , mContext(context)
  {}

  const SelfType& Self() const
  {
    return *static_cast<const SelfType*>(this);
  }
  SelfType& Self()
  {
    return *static_cast<SelfType*>(this);
  }

  JSContext* mCx;

  JS::HandleObject mObject;

  void* mContext;
};

template<class T, class IterType>
class MOZ_STACK_CLASS BaseIterElem {
public:
  typedef T SelfType;

  explicit BaseIterElem(const IterType& iter, uint32_t index = 0)
    : mIter(iter)
    , mIndex(index)
  {}

  uint32_t Length() const
  {
    return mIter.Length();
  }

  JS::Value Value()
  {
    JS::RootedValue value(mIter.mCx, JS::UndefinedValue());

    auto& self = Self();
    if (!self.GetValue(&value)) {
      JS_ClearPendingException(mIter.mCx);
    }

    return value;
  }

  SelfType& operator*() { return Self(); }

  SelfType& operator++()
  {
    MOZ_ASSERT(mIndex < Length());
    mIndex++;
    return Self();
  }

  bool operator!=(const SelfType& other) const
  {
    return &mIter != &other.mIter || mIndex != other.mIndex;
  }


  SelfType End() const
  {
    SelfType end(mIter);
    end.mIndex = Length();
    return Move(end);
  }

  void* Context() const { return mIter.Context(); }

protected:
  const SelfType& Self() const
  {
    return *static_cast<const SelfType*>(this);
  }
  SelfType& Self() {
    return *static_cast<SelfType*>(this);
  }

  const IterType& mIter;

  uint32_t mIndex;
};


/*****************************************************************************
 * Property iteration
 *****************************************************************************/

class MOZ_STACK_CLASS PropertyIter
  : public BaseIter<PropertyIter, PropertyIterElem>
{
  friend class PropertyIterElem;
  friend class BaseIterElem<PropertyIterElem, PropertyIter>;

public:
  PropertyIter(JSContext* cx, JS::HandleObject object, void* context = nullptr)
    : BaseIter(cx, object, context)
    , mIds(cx, JS::IdVector(cx))
  {
    if (!JS_Enumerate(cx, object, &mIds)) {
      JS_ClearPendingException(cx);
    }
  }

  PropertyIter(const PropertyIter& other)
    : PropertyIter(other.mCx, other.mObject, other.mContext)
  {}

  PropertyIter& operator=(const PropertyIter& other)
  {
    MOZ_ASSERT(other.mObject == mObject);
    mCx = other.mCx;
    mContext = other.mContext;

    mIds.clear();
    if (!JS_Enumerate(mCx, mObject, &mIds)) {
      JS_ClearPendingException(mCx);
    }
    return *this;
  }

  int32_t Length() const
  {
    return mIds.length();
  }

protected:
  JS::Rooted<JS::IdVector> mIds;
};

class MOZ_STACK_CLASS PropertyIterElem
  : public BaseIterElem<PropertyIterElem, PropertyIter>
{
  friend class BaseIterElem<PropertyIterElem, PropertyIter>;

public:
  using BaseIterElem::BaseIterElem;

  PropertyIterElem(const PropertyIterElem& other)
    : BaseIterElem(other.mIter, other.mIndex)
  {}

  jsid Id()
  {
    MOZ_ASSERT(mIndex < mIter.mIds.length());

    return mIter.mIds[mIndex];
  }

  const nsAString& Name()
  {
    if(mName.isNothing()) {
      mName.emplace();
      mName.ref().init(mIter.mCx, Id());
    }
    return mName.ref();
  }

  JSContext* Cx() { return mIter.mCx; }

protected:
  bool GetValue(JS::MutableHandleValue value)
  {
    MOZ_ASSERT(mIndex < Length());
    JS::Rooted<jsid> id(mIter.mCx, Id());

    return JS_GetPropertyById(mIter.mCx, mIter.mObject, id, value);
  }

private:
  Maybe<nsAutoJSString> mName;
};


/*****************************************************************************
 * Array iteration
 *****************************************************************************/

class MOZ_STACK_CLASS ArrayIter
  : public BaseIter<ArrayIter, ArrayIterElem>
{
  friend class ArrayIterElem;
  friend class BaseIterElem<ArrayIterElem, ArrayIter>;

public:
  ArrayIter(JSContext* cx, JS::HandleObject object)
    : BaseIter(cx, object)
    , mLength(0)
  {
    bool isArray;
    if (!JS_IsArrayObject(cx, object, &isArray) || !isArray) {
      JS_ClearPendingException(cx);
      return;
    }

    if (!JS_GetArrayLength(cx, object, &mLength)) {
      JS_ClearPendingException(cx);
    }
  }

  uint32_t Length() const
  {
    return mLength;
  }

private:
  uint32_t mLength;
};

class MOZ_STACK_CLASS ArrayIterElem
  : public BaseIterElem<ArrayIterElem, ArrayIter>
{
  friend class BaseIterElem<ArrayIterElem, ArrayIter>;

public:
  using BaseIterElem::BaseIterElem;

  ArrayIterElem(const ArrayIterElem& other)
    : BaseIterElem(other.mIter, other.mIndex)
  {}

protected:
  bool
  GetValue(JS::MutableHandleValue value)
  {
    MOZ_ASSERT(mIndex < Length());
    return JS_GetElement(mIter.mCx, mIter.mObject, mIndex, value);
  }
};

}

#endif // AddonManagerStartup_inlines_h
