/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_GenericModule_h
#define mozilla_GenericModule_h

#include <type_traits>

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Attributes.h"
#include "mozilla/Module.h"

#define NS_GENERIC_FACTORY_CONSTRUCTOR(_InstanceClass)                         \
  static nsresult _InstanceClass##Constructor(REFNSIID aIID, void** aResult) { \
    RefPtr<_InstanceClass> inst;                                               \
                                                                               \
    *aResult = nullptr;                                                        \
                                                                               \
    inst = new _InstanceClass();                                               \
    return inst->QueryInterface(aIID, aResult);                                \
  }

#define NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(_InstanceClass, _InitMethod)       \
  static nsresult _InstanceClass##Constructor(REFNSIID aIID, void** aResult) { \
    nsresult rv;                                                               \
                                                                               \
    RefPtr<_InstanceClass> inst;                                               \
                                                                               \
    *aResult = nullptr;                                                        \
                                                                               \
    inst = new _InstanceClass();                                               \
    rv = inst->_InitMethod();                                                  \
    if (NS_SUCCEEDED(rv)) {                                                    \
      rv = inst->QueryInterface(aIID, aResult);                                \
    }                                                                          \
                                                                               \
    return rv;                                                                 \
  }

namespace mozilla {
namespace detail {

template <typename T>
struct RemoveAlreadyAddRefed {
  using Type = T;
};

template <typename T>
struct RemoveAlreadyAddRefed<already_AddRefed<T>> {
  using Type = T;
};

}  // namespace detail
}  // namespace mozilla

// 'Constructor' that uses an existing getter function that gets a singleton.
#define NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(_InstanceClass, _GetterProc)  \
  static nsresult _InstanceClass##Constructor(REFNSIID aIID, void** aResult) { \
    RefPtr<_InstanceClass> inst;                                               \
                                                                               \
    *aResult = nullptr;                                                        \
                                                                               \
    using T =                                                                  \
        mozilla::detail::RemoveAlreadyAddRefed<decltype(_GetterProc())>::Type; \
    static_assert(                                                             \
        std::is_same_v<already_AddRefed<T>, decltype(_GetterProc())>,          \
        "Singleton constructor must return already_AddRefed");                 \
    static_assert(                                                             \
        std::is_base_of<_InstanceClass, T>::value,                             \
        "Singleton constructor must return correct already_AddRefed");         \
    inst = _GetterProc();                                                      \
    if (nullptr == inst) {                                                     \
      return NS_ERROR_OUT_OF_MEMORY;                                           \
    }                                                                          \
    return inst->QueryInterface(aIID, aResult);                                \
  }

#endif  // mozilla_GenericModule_h
