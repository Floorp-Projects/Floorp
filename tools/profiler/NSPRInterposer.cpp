/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NSPRInterposer.h"

#include "private/pprio.h"

using namespace mozilla;

StaticAutoPtr<NSPRInterposer> NSPRInterposer::sSingleton;
const char* NSPRAutoTimer::sModuleInfo = "NSPR";

/* static */ IOInterposerModule*
NSPRInterposer::GetInstance(IOInterposeObserver* aObserver,
                            IOInterposeObserver::Operation aOpsToInterpose)
{
  // We can't actually use this assertion because we initialize this code
  // before XPCOM is initialized, so NS_IsMainThread() always returns false.
  // MOZ_ASSERT(NS_IsMainThread());
  if (!sSingleton) {
    nsAutoPtr<NSPRInterposer> newObj(new NSPRInterposer());
    if (!newObj->Init(aObserver, aOpsToInterpose)) {
      return nullptr;
    }
    sSingleton = newObj.forget();
  }
  return sSingleton;
}

/* static */ void
NSPRInterposer::ClearInstance()
{
  // We can't actually use this assertion because we execute this code
  // after XPCOM is shut down, so NS_IsMainThread() always returns false.
  // MOZ_ASSERT(NS_IsMainThread());
  sSingleton = nullptr;
}

NSPRInterposer::NSPRInterposer()
  :mObserver(nullptr),
   mFileIOMethods(nullptr),
   mEnabled(false),
   mOrigReadFn(nullptr),
   mOrigWriteFn(nullptr),
   mOrigFSyncFn(nullptr)
{
}

NSPRInterposer::~NSPRInterposer()
{
  // We can't actually use this assertion because we execute this code
  // after XPCOM is shut down, so NS_IsMainThread() always returns false.
  // MOZ_ASSERT(NS_IsMainThread());
  Enable(false);
  mFileIOMethods->read = mOrigReadFn;
  mFileIOMethods->write = mOrigWriteFn;
  mFileIOMethods->fsync = mOrigFSyncFn;
  sSingleton = nullptr;
}

bool
NSPRInterposer::Init(IOInterposeObserver* aObserver,
                     IOInterposeObserver::Operation aOpsToInterpose)
{
  // We can't actually use this assertion because we initialize this code
  // before XPCOM is initialized, so NS_IsMainThread() always returns false.
  // MOZ_ASSERT(NS_IsMainThread());
  if (!aObserver || !(aOpsToInterpose & IOInterposeObserver::OpAll)) {
    return false;
  }
  mObserver = aObserver;
  // Yes, this is not very kosher; now show me an example of function
  // interposing an unsuspecting target that /is/.
  mFileIOMethods = const_cast<PRIOMethods*>(PR_GetFileMethods());
  if (!mFileIOMethods) {
    return false;
  }
  mOrigReadFn = mFileIOMethods->read;
  mOrigWriteFn = mFileIOMethods->write;
  mOrigFSyncFn = mFileIOMethods->fsync;
  if (!mOrigReadFn || !mOrigWriteFn || !mOrigFSyncFn) {
    return false;
  }
  if (aOpsToInterpose & IOInterposeObserver::OpRead) {
    mFileIOMethods->read = &NSPRInterposer::Read;
  }
  if (aOpsToInterpose & IOInterposeObserver::OpWrite) {
    mFileIOMethods->write = &NSPRInterposer::Write;
  }
  if (aOpsToInterpose & IOInterposeObserver::OpFSync) {
    mFileIOMethods->fsync = &NSPRInterposer::FSync;
  }
  return true;
}

void
NSPRInterposer::Enable(bool aEnable)
{
  mEnabled = aEnable ? 1 : 0;
}

PRInt32 PR_CALLBACK
NSPRInterposer::Read(PRFileDesc* aFd, void* aBuf, PRInt32 aAmt)
{
  // If we don't have valid pointers, something is very wrong.
  NS_ASSERTION(sSingleton, "NSPRInterposer::sSingleton not available!");
  NS_ASSERTION(sSingleton->mOrigReadFn, "mOrigReadFn not available!");
  NS_ASSERTION(sSingleton->mObserver, "NSPRInterposer not initialized!");

  NSPRAutoTimer timer(IOInterposeObserver::OpRead);
  return sSingleton->mOrigReadFn(aFd, aBuf, aAmt);
}


PRInt32 PR_CALLBACK
NSPRInterposer::Write(PRFileDesc* aFd, const void* aBuf, PRInt32 aAmt)
{
  // If we don't have valid pointers, something is very wrong.
  NS_ASSERTION(sSingleton, "NSPRInterposer::sSingleton not available!");
  NS_ASSERTION(sSingleton->mOrigWriteFn, "mOrigWriteFn not available!");
  NS_ASSERTION(sSingleton->mObserver, "NSPRInterposer not initialized!");

  NSPRAutoTimer timer(IOInterposeObserver::OpWrite);
  return sSingleton->mOrigWriteFn(aFd, aBuf, aAmt);
}

PRStatus PR_CALLBACK
NSPRInterposer::FSync(PRFileDesc* aFd)
{
  // If we don't have valid pointers, something is very wrong.
  NS_ASSERTION(sSingleton, "NSPRInterposer::sSingleton not available!");
  NS_ASSERTION(sSingleton->mOrigFSyncFn, "mOrigFSyncFn not available!");
  NS_ASSERTION(sSingleton->mObserver, "NSPRInterposer not initialized!");

  NSPRAutoTimer timer(IOInterposeObserver::OpFSync);
  return sSingleton->mOrigFSyncFn(aFd);
}

