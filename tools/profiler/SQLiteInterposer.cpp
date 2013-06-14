/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SQLiteInterposer.h"

using namespace mozilla;

static StaticAutoPtr<SQLiteInterposer> sSingleton;
static const char* sModuleInfo = "SQLite";

/* static */ IOInterposerModule*
SQLiteInterposer::GetInstance(IOInterposeObserver* aObserver,
                              IOInterposeObserver::Operation aOpsToInterpose)
{
  // We can't actually use this assertion because we initialize this code
  // before XPCOM is initialized, so NS_IsMainThread() always returns false.
  // MOZ_ASSERT(NS_IsMainThread());
  if (!sSingleton) {
    nsAutoPtr<SQLiteInterposer> newObj(new SQLiteInterposer());
    if (!newObj->Init(aObserver, aOpsToInterpose)) {
      return nullptr;
    }
    sSingleton = newObj.forget();
  }
  return sSingleton;
}

/* static */ void
SQLiteInterposer::ClearInstance()
{
  // We can't actually use this assertion because we execute this code
  // after XPCOM is shut down, so NS_IsMainThread() always returns false.
  // MOZ_ASSERT(NS_IsMainThread());
  sSingleton = nullptr;
}

SQLiteInterposer::SQLiteInterposer()
  :mObserver(nullptr),
   mOps(IOInterposeObserver::OpNone),
   mEnabled(false)
{
}

SQLiteInterposer::~SQLiteInterposer()
{
  // We can't actually use this assertion because we execute this code
  // after XPCOM is shut down, so NS_IsMainThread() always returns false.
  // MOZ_ASSERT(NS_IsMainThread());
  mOps = IOInterposeObserver::OpNone;
  sSingleton = nullptr;
  Enable(false);
}

bool
SQLiteInterposer::Init(IOInterposeObserver* aObserver,
                       IOInterposeObserver::Operation aOpsToInterpose)
{
  // We can't actually use this assertion because we initialize this code
  // before XPCOM is initialized, so NS_IsMainThread() always returns false.
  // MOZ_ASSERT(NS_IsMainThread());
  if (!aObserver || !(aOpsToInterpose & IOInterposeObserver::OpAll)) {
    return false;
  }
  mObserver = aObserver;
  mOps = aOpsToInterpose;
  return true;
}

void
SQLiteInterposer::Enable(bool aEnable)
{
  mEnabled = aEnable ? 1 : 0;
}

/* static */ void
SQLiteInterposer::OnRead(double& aDuration)
{
  if (!NS_IsMainThread() || !sSingleton) {
    return;
  }
  if (sSingleton->mEnabled &&
      (sSingleton->mOps & IOInterposeObserver::OpRead)) {
    sSingleton->mObserver->Observe(IOInterposeObserver::OpRead, aDuration,
                                   sModuleInfo);
  }
}

/* static */ void
SQLiteInterposer::OnWrite(double& aDuration)
{
  if (!NS_IsMainThread() || !sSingleton) {
    return;
  }
  if (sSingleton->mEnabled && 
      (sSingleton->mOps & IOInterposeObserver::OpWrite)) {
    sSingleton->mObserver->Observe(IOInterposeObserver::OpWrite, aDuration,
                                   sModuleInfo);
  }
}

/* static */ void
SQLiteInterposer::OnFSync(double& aDuration)
{
  if (!NS_IsMainThread() || !sSingleton) {
    return;
  }
  if (sSingleton->mEnabled &&
      (sSingleton->mOps & IOInterposeObserver::OpFSync)) {
    sSingleton->mObserver->Observe(IOInterposeObserver::OpFSync, aDuration,
                                   sModuleInfo);
  }
}

