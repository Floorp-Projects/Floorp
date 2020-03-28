/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsISupportsUtils_h__
#define nsISupportsUtils_h__

#include <type_traits>

#include "nscore.h"
#include "nsISupportsBase.h"
#include "nsError.h"
#include "nsDebug.h"
#include "nsISupportsImpl.h"
#include "mozilla/RefPtr.h"

/**
 * Macro for adding a reference to an interface.
 * @param _ptr The interface pointer.
 */
#define NS_ADDREF(_ptr) (_ptr)->AddRef()

/**
 * Macro for adding a reference to this. This macro should be used
 * because NS_ADDREF (when tracing) may require an ambiguous cast
 * from the pointers primary type to nsISupports. This macro sidesteps
 * that entire problem.
 */
#define NS_ADDREF_THIS() AddRef()

// Making this a |inline| |template| allows |aExpr| to be evaluated only once,
// yet still denies you the ability to |AddRef()| an |nsCOMPtr|.
template <class T>
inline void ns_if_addref(T aExpr) {
  if (aExpr) {
    aExpr->AddRef();
  }
}

/**
 * Macro for adding a reference to an interface that checks for nullptr.
 * @param _expr The interface pointer.
 */
#define NS_IF_ADDREF(_expr) ns_if_addref(_expr)

/*
 * Given these declarations, it explicitly OK and efficient to end a `getter'
 * with:
 *
 *    NS_IF_ADDREF(*result = mThing);
 *
 * even if |mThing| is an |nsCOMPtr|.  If |mThing| is an |nsCOMPtr|, however, it
 * is still _illegal_ to say |NS_IF_ADDREF(mThing)|.
 */

/**
 * Macro for releasing a reference to an interface.
 * @param _ptr The interface pointer.
 */
#define NS_RELEASE(_ptr) \
  do {                   \
    (_ptr)->Release();   \
    (_ptr) = 0;          \
  } while (0)

/**
 * Macro for releasing a reference to this interface.
 */
#define NS_RELEASE_THIS() Release()

/**
 * Macro for releasing a reference to an interface, except that this
 * macro preserves the return value from the underlying Release call.
 * The interface pointer argument will only be NULLed if the reference count
 * goes to zero.
 *
 * @param _ptr The interface pointer.
 * @param _rc  The reference count.
 */
#define NS_RELEASE2(_ptr, _rc)  \
  do {                          \
    _rc = (_ptr)->Release();    \
    if (0 == (_rc)) (_ptr) = 0; \
  } while (0)

/**
 * Macro for releasing a reference to an interface that checks for nullptr;
 * @param _ptr The interface pointer.
 */
#define NS_IF_RELEASE(_ptr) \
  do {                      \
    if (_ptr) {             \
      (_ptr)->Release();    \
      (_ptr) = 0;           \
    }                       \
  } while (0)

/*
 * Often you have to cast an implementation pointer, e.g., |this|, to an
 * |nsISupports*|, but because you have multiple inheritance, a simple cast
 * is ambiguous.  One could simply say, e.g., (given a base |nsIBase|),
 * |static_cast<nsIBase*>(this)|; but that disguises the fact that what
 * you are really doing is disambiguating the |nsISupports|.  You could make
 * that more obvious with a double cast, e.g., |static_cast<nsISupports*>
                                                           (*
 static_cast<nsIBase*>(this))|, but that is bulky and harder to read...
 *
 * The following macro is clean, short, and obvious.  In the example above,
 * you would use it like this: |NS_ISUPPORTS_CAST(nsIBase*, this)|.
 */

#define NS_ISUPPORTS_CAST(__unambiguousBase, __expr) \
  static_cast<nsISupports*>(static_cast<__unambiguousBase>(__expr))

// a type-safe shortcut for calling the |QueryInterface()| member function
template <class T, class DestinationType>
inline nsresult CallQueryInterface(T* aSource, DestinationType** aDestination) {
  // We permit nsISupports-to-nsISupports here so that one can still obtain
  // the canonical nsISupports pointer with CallQueryInterface.
  static_assert(
      !(std::is_same_v<DestinationType, T> ||
        std::is_base_of<DestinationType, T>::value) ||
          std::is_same_v<DestinationType, nsISupports>,
      "don't use CallQueryInterface for compile-time-determinable casts");

  MOZ_ASSERT(aSource, "null parameter");
  MOZ_ASSERT(aDestination, "null parameter");

  return aSource->QueryInterface(NS_GET_TEMPLATE_IID(DestinationType),
                                 reinterpret_cast<void**>(aDestination));
}

template <class SourceType, class DestinationType>
inline nsresult CallQueryInterface(RefPtr<SourceType>& aSourcePtr,
                                   DestinationType** aDestPtr) {
  return CallQueryInterface(aSourcePtr.get(), aDestPtr);
}

#endif /* __nsISupportsUtils_h */
