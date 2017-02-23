/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_sandboxing_sandboxPermissions_h
#define mozilla_sandboxing_sandboxPermissions_h

#include <stdint.h>
#include <windows.h>

namespace mozilla {

namespace sandboxing {
  class PermissionsService;
}

/*
 * This object wraps a PermissionsService object.  This object is available
 * in libXUL but PermissionsService is not.
 */
class SandboxPermissions
{
public:
  /*
   * Type of callback function that the sandbox uses to report file
   * accesses that were denied.
   * Parameter is a boolean indicating the access request was read-only
   * (false) or read-write (true)
   */
  typedef void (*FileAccessViolationFunc)(bool);

  /*
   * Prepare this object by providing it with the internal permissions service.
   */
  static void Initialize(sandboxing::PermissionsService* aPermissionsService,
                         FileAccessViolationFunc aFileAccessViolationFunc);

  /*
   * Allow future access to aFilename by the process with the given ID.
   */
  void GrantFileAccess(uint32_t aProcessId, const wchar_t* aFilename,
                       bool aPermitWrite);

  /*
   * Clears all special file access for the given process.
   */
  void RemovePermissionsForProcess(uint32_t aProcessId);

private:
  static sandboxing::PermissionsService* sPermissionsService;
};

} // mozilla

#endif  // mozilla_sandboxing_sandboxPermissions_h
