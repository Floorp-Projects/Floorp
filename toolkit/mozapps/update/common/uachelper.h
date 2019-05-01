/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _UACHELPER_H_
#define _UACHELPER_H_

class UACHelper {
 public:
  static HANDLE OpenUserToken(DWORD sessionID);
  static HANDLE OpenLinkedToken(HANDLE token);
  static BOOL DisablePrivileges(HANDLE token);
  static bool CanUserElevate();

 private:
  static BOOL SetPrivilege(HANDLE token, LPCTSTR privs, BOOL enable);
  static BOOL DisableUnneededPrivileges(HANDLE token, LPCTSTR* unneededPrivs,
                                        size_t count);
  static LPCTSTR PrivsToDisable[];
};

#endif
