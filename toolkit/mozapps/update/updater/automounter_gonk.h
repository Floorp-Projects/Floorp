/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AUTOMOUNTER_GONK_H__
#define AUTOMOUNTER_GONK_H__

typedef enum {
  ReadOnly,
  ReadWrite,
  Unknown
} MountAccess;

/**
 * This class will remount the /system partition as read-write in Gonk to allow
 * the updater write access. Upon destruction, /system will be remounted back to
 * read-only. If something causes /system to remain read-write, this class will
 * reboot the device and allow the system to mount as read-only.
 *
 * Code inspired from AOSP system/core/adb/remount_service.c
 */
class GonkAutoMounter
{
public:
  GonkAutoMounter();
  ~GonkAutoMounter();

  const MountAccess GetAccess()
  {
    return mAccess;
  }

private:
  bool RemountSystem(MountAccess access);
  bool ForceRemountReadOnly();
  bool UpdateMountStatus();
  bool ProcessMount(const char *mount);
  bool MountSystem(unsigned long flags);
  void Reboot();

private:
  char *mDevice;
  MountAccess mAccess;
};

#endif // AUTOMOUNTER_GONK_H__
