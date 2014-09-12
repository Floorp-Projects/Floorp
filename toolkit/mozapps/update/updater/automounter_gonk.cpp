/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <android/log.h>
#include <cutils/android_reboot.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "automounter_gonk.h"
#include "updatedefines.h"
#include "updatelogging.h"

#define LOG_TAG "GonkAutoMounter"

#define GONK_LOG(level, format, ...) \
  LOG((LOG_TAG ": " format "\n", ##__VA_ARGS__)); \
  __android_log_print(level, LOG_TAG, format, ##__VA_ARGS__)

#define LOGI(format, ...) GONK_LOG(ANDROID_LOG_INFO, format, ##__VA_ARGS__)
#define LOGE(format, ...) GONK_LOG(ANDROID_LOG_ERROR, format, ##__VA_ARGS__)

const char *kGonkMountsPath = "/proc/mounts";
const char *kGonkSystemPath = "/system";

GonkAutoMounter::GonkAutoMounter() : mDevice(nullptr), mAccess(Unknown)
{
  if (!RemountSystem(ReadWrite)) {
    LOGE("Could not remount %s as read-write.", kGonkSystemPath);
  }
}

GonkAutoMounter::~GonkAutoMounter()
{
  bool result = RemountSystem(ReadOnly);
  free(mDevice);

  if (!result) {
    // Don't take any chances when remounting as read-only fails, just reboot.
    Reboot();
  }
}

void
GonkAutoMounter::Reboot()
{
  // The android_reboot wrapper provides more safety, doing fancier read-only
  // remounting and attempting to sync() the filesystem first. If this fails
  // our only hope is to force a reboot directly without these protections.
  // For more, see system/core/libcutils/android_reboot.c
  LOGE("Could not remount %s as read-only, forcing a system reboot.",
       kGonkSystemPath);
  LogFlush();

  if (android_reboot(ANDROID_RB_RESTART, 0, nullptr) != 0) {
    LOGE("Safe system reboot failed, attempting to force");
    LogFlush();

    if (reboot(RB_AUTOBOOT) != 0) {
      LOGE("CRITICAL: Failed to force restart");
    }
  }
}

static const char *
MountAccessToString(MountAccess access)
{
  switch (access) {
    case ReadOnly: return "read-only";
    case ReadWrite: return "read-write";
    default: return "unknown";
  }
}

bool
GonkAutoMounter::RemountSystem(MountAccess access)
{
  if (!UpdateMountStatus()) {
    return false;
  }

  if (mAccess == access) {
    return true;
  }

  unsigned long flags = MS_REMOUNT;
  if (access == ReadOnly) {
    flags |= MS_RDONLY;
    // Give the system a chance to flush file buffers
    sync();
  }

  if (!MountSystem(flags)) {
    return false;
  }

  // Check status again to verify /system has been properly remounted
  if (!UpdateMountStatus()) {
    return false;
  }

  if (mAccess != access) {
    LOGE("Updated mount status %s should be %s",
         MountAccessToString(mAccess),
         MountAccessToString(access));
    return false;
  }

  return true;
}

bool
GonkAutoMounter::UpdateMountStatus()
{
  FILE *mountsFile = NS_tfopen(kGonkMountsPath, "r");

  if (mountsFile == nullptr) {
    LOGE("Error opening %s: %s", kGonkMountsPath, strerror(errno));
    return false;
  }

  // /proc/mounts returns a 0 size from fstat, so we use the same
  // pre-allocated buffer size that ADB does here
  const int mountsMaxSize = 4096;
  char mountData[mountsMaxSize];
  size_t read = fread(mountData, 1, mountsMaxSize - 1, mountsFile);
  mountData[read + 1] = '\0';

  if (ferror(mountsFile)) {
    LOGE("Error reading %s, %s", kGonkMountsPath, strerror(errno));
    fclose(mountsFile);
    return false;
  }

  char *token, *tokenContext;
  bool foundSystem = false;

  for (token = strtok_r(mountData, "\n", &tokenContext);
       token;
       token = strtok_r(nullptr, "\n", &tokenContext))
  {
    if (ProcessMount(token)) {
      foundSystem = true;
      break;
    }
  }

  fclose(mountsFile);

  if (!foundSystem) {
    LOGE("Couldn't find %s mount in %s", kGonkSystemPath, kGonkMountsPath);
  }
  return foundSystem;
}

bool
GonkAutoMounter::ProcessMount(const char *mount)
{
  const int strSize = 256;
  char mountDev[strSize];
  char mountDir[strSize];
  char mountAccess[strSize];

  int rv = sscanf(mount, "%255s %255s %*s %255s %*d %*d\n",
                  mountDev, mountDir, mountAccess);
  mountDev[strSize - 1] = '\0';
  mountDir[strSize - 1] = '\0';
  mountAccess[strSize - 1] = '\0';

  if (rv != 3) {
    return false;
  }

  if (strcmp(kGonkSystemPath, mountDir) != 0) {
    return false;
  }

  free(mDevice);
  mDevice = strdup(mountDev);
  mAccess = Unknown;

  char *option, *optionContext;
  for (option = strtok_r(mountAccess, ",", &optionContext);
       option;
       option = strtok_r(nullptr, ",", &optionContext))
  {
    if (strcmp("ro", option) == 0) {
      mAccess = ReadOnly;
      break;
    } else if (strcmp("rw", option) == 0) {
      mAccess = ReadWrite;
      break;
    }
  }

  return true;
}

/*
 * Mark the given block device as read-write or read-only, using the BLKROSET
 * ioctl.
 */
static void SetBlockReadWriteStatus(const char *blockdev, bool setReadOnly) {
  int fd;
  int roMode = setReadOnly ? 1 : 0;

  fd = open(blockdev, O_RDONLY);
  if (fd < 0) {
    return;
  }

  if (ioctl(fd, BLKROSET, &roMode) == -1) {
    LOGE("Error setting read-only mode on %s to %s: %s", blockdev,
         setReadOnly ? "true": "false", strerror(errno));
  }
  close(fd);
}


bool
GonkAutoMounter::MountSystem(unsigned long flags)
{
  if (!mDevice) {
    LOGE("No device was found for %s", kGonkSystemPath);
    return false;
  }

  // Without setting the block device ro mode to false, we get a permission
  // denied error while trying to remount it in read-write.
  SetBlockReadWriteStatus(mDevice, (flags & MS_RDONLY));

  const char *readOnly = flags & MS_RDONLY ? "read-only" : "read-write";
  int result = mount(mDevice, kGonkSystemPath, "none", flags, nullptr);

  if (result != 0) {
    LOGE("Error mounting %s as %s: %s", kGonkSystemPath, readOnly,
         strerror(errno));
    return false;
  }

  LOGI("Mounted %s partition as %s", kGonkSystemPath, readOnly);
  return true;
}
