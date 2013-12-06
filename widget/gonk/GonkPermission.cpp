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

#undef LOG
#include <android/log.h>
#define ALOGE(args...)  __android_log_print(ANDROID_LOG_ERROR, "gonkperm" , ## args)

using namespace android;
using namespace mozilla;

bool
GonkPermissionService::checkPermission(const String16& permission, int32_t pid,
                                     int32_t uid)
{
  if (0 == uid)
    return true;

  // Camera/audio record permissions are only for apps with the
  // "camera" permission.  These apps are also the only apps granted
  // the AID_SDCARD_RW supplemental group (bug 785592)

  if (uid < AID_APP) {
    ALOGE("%s for pid=%d,uid=%d denied: not an app",
      String8(permission).string(), pid, uid);
    return false;
  }

  String8 perm8(permission);

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

  char filename[32];
  snprintf(filename, sizeof(filename), "/proc/%d/status", pid);
  FILE *f = fopen(filename, "r");
  if (!f) {
    ALOGE("%s for pid=%d,uid=%d denied: unable to open %s",
      String8(permission).string(), pid, uid, filename);
    return false;
  }

  char line[80];
  while (fgets(line, sizeof(line), f)) {
    char *save;
    char *name = strtok_r(line, "\t", &save);
    if (!name)
      continue;

    if (strcmp(name, "Groups:"))
      continue;
    char *group;
    while ((group = strtok_r(NULL, " \n", &save))) {
      #define _STR(x) #x
      #define STR(x) _STR(x)
      if (!strcmp(group, STR(AID_SDCARD_RW))) {
        fclose(f);
        return true;
      }
    }
    break;
  }
  fclose(f);

  ALOGE("%s for pid=%d,uid=%d denied: missing group",
    String8(permission).string(), pid, uid);
  return false;
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
