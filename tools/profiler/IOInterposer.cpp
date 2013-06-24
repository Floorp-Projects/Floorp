/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IOInterposer.h"
#include "NSPRInterposer.h"
#include "SQLiteInterposer.h"

using namespace mozilla;

static StaticAutoPtr<IOInterposer> sSingleton;

/* static */ IOInterposer*
IOInterposer::GetInstance()
{
  if (!sSingleton) {
    // We can't actually use this assertion because we initialize this code
    // before XPCOM is initialized, so NS_IsMainThread() always returns false.
    // MOZ_ASSERT(NS_IsMainThread());
    sSingleton = new IOInterposer();
    sSingleton->Init();
  }

  return sSingleton.get();
}

/* static */ void
IOInterposer::ClearInstance()
{
  // We can't actually use this assertion because we execute this code
  // after XPCOM is shut down, so NS_IsMainThread() always returns false.
  // MOZ_ASSERT(NS_IsMainThread());
  sSingleton = nullptr;
}

IOInterposer::IOInterposer()
  :mMutex("IOInterposer::mMutex")
{
  // We can't actually use this assertion because we initialize this code
  // before XPCOM is initialized, so NS_IsMainThread() always returns false.
  // MOZ_ASSERT(NS_IsMainThread());
}

IOInterposer::~IOInterposer()
{
  // We can't actually use this assertion because we execute this code
  // after XPCOM is shut down, so NS_IsMainThread() always returns false.
  // MOZ_ASSERT(NS_IsMainThread());
  Enable(false);
  NSPRInterposer::ClearInstance();
  SQLiteInterposer::ClearInstance();
}

bool
IOInterposer::Init()
{
  // We can't actually use this assertion because we initialize this code
  // before XPCOM is initialized, so NS_IsMainThread() always returns false.
  // MOZ_ASSERT(NS_IsMainThread());
  mozilla::MutexAutoLock lock(mMutex);
  IOInterposerModule* nsprModule = NSPRInterposer::GetInstance(this,
      IOInterposeObserver::OpAll);
  if (!nsprModule) {
    return false;
  }

  IOInterposerModule* sqlModule = SQLiteInterposer::GetInstance(this,
      IOInterposeObserver::OpAll);
  if (!sqlModule) {
    return false;
  }

  mModules.AppendElement(nsprModule);
  mModules.AppendElement(sqlModule);
  return true;
}

void
IOInterposer::Enable(bool aEnable)
{
  mozilla::MutexAutoLock lock(mMutex);
  for (uint32_t i = 0; i < mModules.Length(); ++i ) {
    mModules[i]->Enable(aEnable);
  }
}

void
IOInterposer::Register(IOInterposeObserver::Operation aOp,
                       IOInterposeObserver* aObserver)
{
  // We can't actually use this assertion because we initialize this code
  // before XPCOM is initialized, so NS_IsMainThread() always returns false.
  // MOZ_ASSERT(NS_IsMainThread());
  if (aOp & IOInterposeObserver::OpRead) {
    mReadObservers.AppendElement(aObserver);
  }
  if (aOp & IOInterposeObserver::OpWrite) {
    mWriteObservers.AppendElement(aObserver);
  }
  if (aOp & IOInterposeObserver::OpFSync) {
    mFSyncObservers.AppendElement(aObserver);
  }
}

void
IOInterposer::Deregister(IOInterposeObserver::Operation aOp,
                         IOInterposeObserver* aObserver)
{
  // We can't actually use this assertion because we execute this code
  // after XPCOM is shut down, so NS_IsMainThread() always returns false.
  // MOZ_ASSERT(NS_IsMainThread());
  if (aOp & IOInterposeObserver::OpRead) {
    mReadObservers.RemoveElement(aObserver);
  }
  if (aOp & IOInterposeObserver::OpWrite) {
    mWriteObservers.RemoveElement(aObserver);
  }
  if (aOp & IOInterposeObserver::OpFSync) {
    mFSyncObservers.RemoveElement(aObserver);
  }
}

void
IOInterposer::Observe(IOInterposeObserver::Operation aOp, double& aDuration,
                      const char* aModuleInfo)
{
  MOZ_ASSERT(NS_IsMainThread());
  switch (aOp) {
    case IOInterposeObserver::OpRead:
      {
        for (uint32_t i = 0; i < mReadObservers.Length(); ++i) {
          mReadObservers[i]->Observe(aOp, aDuration, aModuleInfo);
        }
      }
      break;
    case IOInterposeObserver::OpWrite:
      {
        for (uint32_t i = 0; i < mWriteObservers.Length(); ++i) {
          mWriteObservers[i]->Observe(aOp, aDuration, aModuleInfo);
        }
      }
      break;
    case IOInterposeObserver::OpFSync:
      {
        for (uint32_t i = 0; i < mFSyncObservers.Length(); ++i) {
          mFSyncObservers[i]->Observe(aOp, aDuration, aModuleInfo);
        }
      }
      break;
    default:
      break;
  }
}

