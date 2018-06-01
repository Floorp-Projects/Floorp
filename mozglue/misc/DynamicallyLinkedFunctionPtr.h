/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_DynamicallyLinkedFunctionPtr_h
#define mozilla_DynamicallyLinkedFunctionPtr_h

#include "mozilla/Move.h"
#include <windows.h>

namespace mozilla {

template <typename T>
class DynamicallyLinkedFunctionPtr;

template <typename R, typename... Args>
class DynamicallyLinkedFunctionPtr<R (__stdcall*)(Args...)>
{
  typedef R (__stdcall* FunctionPtrT)(Args...);

public:
  DynamicallyLinkedFunctionPtr(const wchar_t* aLibName, const char* aFuncName)
    : mModule(NULL)
    , mFunction(nullptr)
  {
    mModule = ::LoadLibraryW(aLibName);
    if (mModule) {
      mFunction = reinterpret_cast<FunctionPtrT>(
                    ::GetProcAddress(mModule, aFuncName));

      if (!mFunction) {
        // Since the function doesn't exist, there is no point in holding a
        // reference to mModule anymore.
        ::FreeLibrary(mModule);
        mModule = NULL;
      }
    }
  }

  DynamicallyLinkedFunctionPtr(const DynamicallyLinkedFunctionPtr&) = delete;
  DynamicallyLinkedFunctionPtr& operator=(const DynamicallyLinkedFunctionPtr&) = delete;

  DynamicallyLinkedFunctionPtr(DynamicallyLinkedFunctionPtr&&) = delete;
  DynamicallyLinkedFunctionPtr& operator=(DynamicallyLinkedFunctionPtr&&) = delete;

  ~DynamicallyLinkedFunctionPtr()
  {
    if (mModule) {
      ::FreeLibrary(mModule);
    }
  }

  R operator()(Args... args) const
  {
    return mFunction(std::forward<Args>(args)...);
  }

  explicit operator bool() const
  {
    return !!mFunction;
  }

private:
  HMODULE       mModule;
  FunctionPtrT  mFunction;
};

} // namespace mozilla

#endif // mozilla_DynamicallyLinkedFunctionPtr_h

