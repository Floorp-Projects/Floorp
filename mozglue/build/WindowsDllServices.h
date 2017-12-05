/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WindowsDllServices_h
#define mozilla_WindowsDllServices_h

#include "mozilla/WindowsDllBlocklist.h"

#if defined(MOZILLA_INTERNAL_API)

#include "mozilla/SystemGroup.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsThreadUtils.h"

#endif // defined(MOZILLA_INTERNAL_API)

// For PCUNICODE_STRING
#include <winternl.h>

namespace mozilla {
namespace detail {

class DllServicesBase
{
public:
  /**
   * WARNING: This method is called from within an unsafe context that holds
   *          multiple locks inside the Windows loader. The only thing that
   *          this function should be used for is dispatching the event to our
   *          event loop so that it may be handled in a safe context.
   */
  virtual void DispatchDllLoadNotification(PCUNICODE_STRING aDllName) = 0;

  void Disable()
  {
    DllBlocklist_SetDllServices(nullptr);
  }

  DllServicesBase(const DllServicesBase&) = delete;
  DllServicesBase(DllServicesBase&&) = delete;
  DllServicesBase& operator=(const DllServicesBase&) = delete;
  DllServicesBase& operator=(DllServicesBase&&) = delete;

protected:
  DllServicesBase() = default;
  virtual ~DllServicesBase() = default;

  void Enable()
  {
    DllBlocklist_SetDllServices(this);
  }
};

} // namespace detail

#if defined(MOZILLA_INTERNAL_API)

class DllServices : public detail::DllServicesBase
{
public:
  virtual void DispatchDllLoadNotification(PCUNICODE_STRING aDllName) override final
  {
    nsDependentString strDllName(aDllName->Buffer,
                                 aDllName->Length / sizeof(wchar_t));

    nsCOMPtr<nsIRunnable> runnable(
      NewRunnableMethod<bool, nsString>("DllServices::NotifyDllLoad",
                                        this, &DllServices::NotifyDllLoad,
                                        NS_IsMainThread(), strDllName));

    SystemGroup::Dispatch(TaskCategory::Other, runnable.forget());
  }

  NS_INLINE_DECL_THREADSAFE_VIRTUAL_REFCOUNTING(DllServices)

protected:
  DllServices() = default;
  ~DllServices() = default;

  virtual void NotifyDllLoad(const bool aIsMainThread, const nsString& aDllName) = 0;
};

#endif // defined(MOZILLA_INTERNAL_API)

} // namespace mozilla

#endif // mozilla_WindowsDllServices_h
