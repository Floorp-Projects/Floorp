/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _USERTOKEN_H_
#define _USERTOKEN_H_

#include <windows.h>

HANDLE GetUserProcessToken(LPCWSTR updaterPath, int updaterArgc,
                           LPCWSTR updaterArgv[]);

#endif  // _USERTOKEN_H_
