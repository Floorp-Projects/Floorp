/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WinTokenUtils_h
#define mozilla_WinTokenUtils_h

#include "mozilla/WinHeaderOnlyUtils.h"

namespace mozilla {

/**
 * Windows UAC can be disabled via the registry. This checks if the user has
 * Administrator privileges and UAC has been disabled.
 *
 * @return `Ok(true)` when the current process has Administrator privileges
 *         *and* UAC has been disabled, otherwise `Ok(false)` or
 *         `Err(WindowsError)`.
 */
LauncherResult<bool> IsAdminWithoutUac();

/**
 * Checks if the current process has Administrator privileges.
 *
 * @return `Ok(true)` when the current process has Administrator privileges,
 *         otherwise `Ok(false)` or `Err(WindowsError)`.
 *
 */
LauncherResult<bool> UserHasAdminPrivileges();

/**
 * Checks if the current process user is LocalSystem.
 *
 * LocalSystem (or just SYSTEM in Task Manager) is a high privileged account
 * similar to Administrator. Unlike Administrator which runs under a User
 * context, LocalSystem runs under a System context and has some privileges
 * beyond that of Administrator. We use LocalSystem for privileges necessary for
 * our updater.
 *
 * Note that LocalSystem is not equivalent to the lower privileged LocalSerivce
 * account.
 *
 * @return `Ok(true)` when the current process user is LocalSystem, otherwise
 *         `Ok(false)` or `Err(WindowsError)`.
 */
LauncherResult<bool> UserIsLocalSystem();

}  // namespace mozilla

#endif  // mozilla_WinTokenUtils_h
