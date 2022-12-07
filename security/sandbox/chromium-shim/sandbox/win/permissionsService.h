/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_sandboxing_permissionsService_h
#define mozilla_sandboxing_permissionsService_h

#include <string>
#include <unordered_map>

namespace mozilla {
namespace sandboxing {

/*
 * Represents additional permissions granted to sandboxed processes.
 * The members are virtual so that the object can be created in any
 * library that links with libsandbox_s and then shared with and used
 * by libXUL, which does not link with libsandbox_s.
 */
class PermissionsService
{
public:
  static PermissionsService* GetInstance();

  /*
   * Allow future access to aFilename by the plugin process.
   */
  virtual void GrantFileAccess(uint32_t aProcessId, const wchar_t* aFilename,
                               bool aPermitWrite);

  /*
   * Type of callback function that the sandbox uses to report file
   * accesses that were denied.
   * Parameter is a boolean indicating the access request was read-only
   * (false) or read-write (true)
   */
  typedef void (*FileAccessViolationFunc)(bool);

  /*
   * Sets the callback function that is called whenever a file access is
   * denied by the sandbox.
   */
  virtual void SetFileAccessViolationFunc(FileAccessViolationFunc aFavFunc);

  /*
   * Returns true if the user has granted the sandboxed plugin process the
   * requested permission to open the file.
   * Calls aFavFunc with file info if the file access was blocked.
   */
  virtual bool UserGrantedFileAccess(uint32_t aProcessId, const wchar_t* aFilename,
                                     uint32_t aAccess, uint32_t aDisposition);

  /*
   * Clears all special file access for the given plugin process.
   */
  virtual void RemovePermissionsForProcess(uint32_t aProcessId);

private:
  PermissionsService();
  void ReportBlockedFile(bool aNeedsWrite);

  // Maps from filenames to a boolean indicating read-only permission (false) or
  // read-write permission (true).
  typedef std::unordered_map<std::wstring, bool> FilePermissionMap;

  // Maps from process ID to map of user-granted file permissions for
  // that process.
  typedef std::unordered_map<uint32_t, FilePermissionMap> ProcessFilePermissionMap;

  ProcessFilePermissionMap mProcessFilePermissions;
  FileAccessViolationFunc mFileAccessViolationFunc;
};

} // namespace sandboxing
} // namespace mozilla

#endif // mozilla_sandboxing_permissionsService_h
