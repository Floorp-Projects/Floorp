/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CFTypeRefPtr_h
#define CFTypeRefPtr_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/DbgMacro.h"
#include "mozilla/HashFunctions.h"

// A smart pointer for CoreFoundation classes which does reference counting.
//
// Manual reference counting:
//
// UInt32 someNumber = 10;
// CFNumberRef numberObject =
//     CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &someNumber);
// // do something with numberObject
// CFRelease(numberObject);
//
// Automatic reference counting using CFTypeRefPtr:
//
// UInt32 someNumber = 10;
// auto numberObject =
//      CFTypeRefPtr<CFNumberRef>::WrapUnderCreateRule(
//        CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &someNumber));
// // do something with numberObject
// // no CFRelease

template <class PtrT>
class CFTypeRefPtr {
 private:
  void assign_with_CFRetain(PtrT aRawPtr) {
    CFRetain(aRawPtr);
    assign_assuming_CFRetain(aRawPtr);
  }

  void assign_assuming_CFRetain(PtrT aNewPtr) {
    PtrT oldPtr = mRawPtr;
    mRawPtr = aNewPtr;
    if (oldPtr) {
      CFRelease(oldPtr);
    }
  }

 private:
  PtrT mRawPtr;

 public:
  ~CFTypeRefPtr() {
    if (mRawPtr) {
      CFRelease(mRawPtr);
    }
  }

  // Constructors

  CFTypeRefPtr() : mRawPtr(nullptr) {}

  CFTypeRefPtr(const CFTypeRefPtr<PtrT>& aSmartPtr)
      : mRawPtr(aSmartPtr.mRawPtr) {
    if (mRawPtr) {
      CFRetain(mRawPtr);
    }
  }

  CFTypeRefPtr(CFTypeRefPtr<PtrT>&& aRefPtr) : mRawPtr(aRefPtr.mRawPtr) {
    aRefPtr.mRawPtr = nullptr;
  }

  MOZ_IMPLICIT CFTypeRefPtr(decltype(nullptr)) : mRawPtr(nullptr) {}

  // There is no constructor from a raw pointer value.
  // Use one of the static WrapUnder*Rule methods below instead.

  static CFTypeRefPtr<PtrT> WrapUnderCreateRule(PtrT aRawPtr) {
    CFTypeRefPtr<PtrT> ptr;
    ptr.AssignUnderCreateRule(aRawPtr);
    return ptr;
  }

  static CFTypeRefPtr<PtrT> WrapUnderGetRule(PtrT aRawPtr) {
    CFTypeRefPtr<PtrT> ptr;
    ptr.AssignUnderGetRule(aRawPtr);
    return ptr;
  }

  // Assignment operators

  CFTypeRefPtr<PtrT>& operator=(decltype(nullptr)) {
    assign_assuming_CFRetain(nullptr);
    return *this;
  }

  CFTypeRefPtr<PtrT>& operator=(const CFTypeRefPtr<PtrT>& aRhs) {
    assign_with_CFRetain(aRhs.mRawPtr);
    return *this;
  }

  CFTypeRefPtr<PtrT>& operator=(CFTypeRefPtr<PtrT>&& aRefPtr) {
    assign_assuming_CFRetain(aRefPtr.mRawPtr);
    aRefPtr.mRawPtr = nullptr;
    return *this;
  }

  // There is no operator= for a raw pointer value.
  // Use one of the AssignUnder*Rule methods below instead.

  CFTypeRefPtr<PtrT>& AssignUnderCreateRule(PtrT aRawPtr) {
    // Freshly-created objects come with a retain count of 1.
    assign_assuming_CFRetain(aRawPtr);
    return *this;
  }

  CFTypeRefPtr<PtrT>& AssignUnderGetRule(PtrT aRawPtr) {
    assign_with_CFRetain(aRawPtr);
    return *this;
  }

  // Other pointer operators

  // This is the only way to get the raw pointer out of the smart pointer.
  // There is no implicit conversion to a raw pointer.
  PtrT get() const { return mRawPtr; }

  // Don't allow implicit conversion of temporary CFTypeRefPtr to raw pointer,
  // because the refcount might be one and the pointer will immediately become
  // invalid.
  operator PtrT() const&& = delete;
  // Also don't allow implicit conversion of non-temporary CFTypeRefPtr.
  operator PtrT() const& = delete;

  // These let you null-check a pointer without calling get().
  explicit operator bool() const { return !!mRawPtr; }
};

template <class PtrT>
inline bool operator==(const CFTypeRefPtr<PtrT>& aLhs,
                       const CFTypeRefPtr<PtrT>& aRhs) {
  return aLhs.get() == aRhs.get();
}

template <class PtrT>
inline bool operator!=(const CFTypeRefPtr<PtrT>& aLhs,
                       const CFTypeRefPtr<PtrT>& aRhs) {
  return !(aLhs == aRhs);
}

// Comparing an |CFTypeRefPtr| to |nullptr|

template <class PtrT>
inline bool operator==(const CFTypeRefPtr<PtrT>& aLhs, decltype(nullptr)) {
  return aLhs.get() == nullptr;
}

template <class PtrT>
inline bool operator==(decltype(nullptr), const CFTypeRefPtr<PtrT>& aRhs) {
  return nullptr == aRhs.get();
}

template <class PtrT>
inline bool operator!=(const CFTypeRefPtr<PtrT>& aLhs, decltype(nullptr)) {
  return aLhs.get() != nullptr;
}

template <class PtrT>
inline bool operator!=(decltype(nullptr), const CFTypeRefPtr<PtrT>& aRhs) {
  return nullptr != aRhs.get();
}

// MOZ_DBG support

template <class PtrT>
std::ostream& operator<<(std::ostream& aOut, const CFTypeRefPtr<PtrT>& aObj) {
  return mozilla::DebugValue(aOut, aObj.get());
}

// std::hash support (e.g. for unordered_map)
namespace std {
template <class PtrT>
struct hash<CFTypeRefPtr<PtrT>> {
  typedef CFTypeRefPtr<PtrT> argument_type;
  typedef std::size_t result_type;
  result_type operator()(argument_type const& aPtr) const {
    return mozilla::HashGeneric(reinterpret_cast<uintptr_t>(aPtr.get()));
  }
};
}  // namespace std

#endif /* CFTypeRefPtr_h */
