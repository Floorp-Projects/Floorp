/*
 * Copyright (C) 2012 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <binder/IPermissionController.h>
#include <private/android_filesystem_config.h>
#include "GonkPermission.h"

#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/SyncRunnable.h"
#include "nsIAppsService.h"
#include "mozIApplication.h"
#include "nsThreadUtils.h"

#undef LOG
#include <android/log.h>
#define ALOGE(args...)  __android_log_print(ANDROID_LOG_ERROR, "gonkperm" , ## args)

using namespace android;
using namespace mozilla;

// Checking permissions needs to happen on the main thread, but the
// binder callback is called on a special binder thread, so we use
// this runnable for that.
class GonkPermissionChecker : public nsRunnable {
  int32_t mPid;
  bool mCanUseCamera;

  explicit GonkPermissionChecker(int32_t pid)
    : mPid(pid)
    , mCanUseCamera(false)
  {
  }

public:
  static already_AddRefed<GonkPermissionChecker> Inspect(int32_t pid)
  {
    nsRefPtr<GonkPermissionChecker> that = new GonkPermissionChecker(pid);
    nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
    MOZ_ASSERT(mainThread);
    SyncRunnable::DispatchToThread(mainThread, that);
    return that.forget();
  }

  bool CanUseCamera()
  {
    return mCanUseCamera;
  }

  NS_IMETHOD Run();
};

NS_IMETHODIMP
GonkPermissionChecker::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Find our ContentParent.
  dom::ContentParent *contentParent = nullptr;
  {
    nsTArray<dom::ContentParent*> parents;
    dom::ContentParent::GetAll(parents);
    for (uint32_t i = 0; i < parents.Length(); ++i) {
      if (parents[i]->Pid() == mPid) {
	contentParent = parents[i];
	break;
      }
    }
  }
  if (!contentParent) {
    ALOGE("pid=%d denied: can't find ContentParent", mPid);
    return NS_OK;
  }

  // Now iterate its apps...
  for (uint32_t i = 0; i < contentParent->ManagedPBrowserParent().Length(); i++) {
    dom::TabParent *tabParent =
      static_cast<dom::TabParent*>(contentParent->ManagedPBrowserParent()[i]);
    nsCOMPtr<mozIApplication> mozApp = tabParent->GetOwnOrContainingApp();
    if (!mozApp) {
      continue;
    }

    // ...and check if any of them has camera access.
    bool appCanUseCamera;
    nsresult rv = mozApp->HasPermission("camera", &appCanUseCamera);
    if (NS_SUCCEEDED(rv) && appCanUseCamera) {
      mCanUseCamera = true;
      return NS_OK;
    }
  }
  return NS_OK;
}

bool
GonkPermissionService::checkPermission(const String16& permission, int32_t pid,
                                     int32_t uid)
{
  // root can do anything.
  if (0 == uid) {
    return true;
  }

  String8 perm8(permission);

  // Some ril implementations need android.permission.MODIFY_AUDIO_SETTINGS
  if (uid == AID_RADIO &&
      perm8 == "android.permission.MODIFY_AUDIO_SETTINGS") {
    return true;
  }

  // No other permissions apply to non-app processes.
  if (uid < AID_APP) {
    ALOGE("%s for pid=%d,uid=%d denied: not an app",
      String8(permission).string(), pid, uid);
    return false;
  }

  // Only these permissions can be granted to apps through this service.
  if (perm8 != "android.permission.CAMERA" &&
    perm8 != "android.permission.RECORD_AUDIO") {
    ALOGE("%s for pid=%d,uid=%d denied: unsupported permission",
      String8(permission).string(), pid, uid);
    return false;
  }

  // Users granted the permission through a prompt dialog.
  // Before permission managment of gUM is done, app cannot remember the
  // permission.
  PermissionGrant permGrant(perm8.string(), pid);
  if (nsTArray<PermissionGrant>::NoIndex != mGrantArray.IndexOf(permGrant)) {
    mGrantArray.RemoveElement(permGrant);
    return true;
  }

  // Camera/audio record permissions are allowed for apps with the
  // "camera" permission.
  nsRefPtr<GonkPermissionChecker> checker =
    GonkPermissionChecker::Inspect(pid);
  bool canUseCamera = checker->CanUseCamera();
  if (!canUseCamera) {
    ALOGE("%s for pid=%d,uid=%d denied: not granted by user or app manifest",
      String8(permission).string(), pid, uid);
  }
  return canUseCamera;
}

static GonkPermissionService* gGonkPermissionService = NULL;

/* static */
void
GonkPermissionService::instantiate()
{
  defaultServiceManager()->addService(String16(getServiceName()),
    GetInstance());
}

/* static */
GonkPermissionService*
GonkPermissionService::GetInstance()
{
  if (!gGonkPermissionService) {
    gGonkPermissionService = new GonkPermissionService();
  }
  return gGonkPermissionService;
}

void
GonkPermissionService::addGrantInfo(const char* permission, int32_t pid)
{
  mGrantArray.AppendElement(PermissionGrant(permission, pid));
}
