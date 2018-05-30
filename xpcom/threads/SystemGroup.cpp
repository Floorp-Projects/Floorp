/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SystemGroup.h"

#include "mozilla/AbstractThread.h"
#include "mozilla/Move.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsINamed.h"

using namespace mozilla;

class SystemGroupImpl final : public SchedulerGroup
{
public:
  SystemGroupImpl();
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SystemGroupImpl, override)

  static void InitStatic();
  static void ShutdownStatic();
  static SystemGroupImpl* Get();

  static bool Initialized() { return !!sSingleton; }

private:
  ~SystemGroupImpl() = default;
  static StaticRefPtr<SystemGroupImpl> sSingleton;
};

StaticRefPtr<SystemGroupImpl> SystemGroupImpl::sSingleton;

SystemGroupImpl::SystemGroupImpl()
{
  CreateEventTargets(/* aNeedValidation = */ true);
}

/* static */ void
SystemGroupImpl::InitStatic()
{
  MOZ_ASSERT(!sSingleton);
  MOZ_ASSERT(NS_IsMainThread());
  sSingleton = new SystemGroupImpl();
}

/* static */ void
SystemGroupImpl::ShutdownStatic()
{
  sSingleton->Shutdown(true);
  sSingleton = nullptr;
}

/* static */ SystemGroupImpl*
SystemGroupImpl::Get()
{
  MOZ_ASSERT(sSingleton);
  return sSingleton.get();
}

void
SystemGroup::InitStatic()
{
  SystemGroupImpl::InitStatic();
}

void
SystemGroup::Shutdown()
{
  SystemGroupImpl::ShutdownStatic();
}

bool
SystemGroup::Initialized()
{
  return SystemGroupImpl::Initialized();
}

/* static */ nsresult
SystemGroup::Dispatch(TaskCategory aCategory,
                      already_AddRefed<nsIRunnable>&& aRunnable)
{
  if (!SystemGroupImpl::Initialized()) {
    return NS_DispatchToMainThread(std::move(aRunnable));
  }
  return SystemGroupImpl::Get()->Dispatch(aCategory, std::move(aRunnable));
}

/* static */ nsISerialEventTarget*
SystemGroup::EventTargetFor(TaskCategory aCategory)
{
  if (!SystemGroupImpl::Initialized()) {
    return GetMainThreadSerialEventTarget();
  }
  return SystemGroupImpl::Get()->EventTargetFor(aCategory);
}

/* static */ AbstractThread*
SystemGroup::AbstractMainThreadFor(TaskCategory aCategory)
{
  MOZ_ASSERT(SystemGroupImpl::Initialized());
  return SystemGroupImpl::Get()->AbstractMainThreadFor(aCategory);
}
