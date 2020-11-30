/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLOBALSEMAPHORE_H
#define GLOBALSEMAPHORE_H

namespace mozilla {

#ifdef XP_WIN
#  include <windows.h>
using GlobalSemHandle = HANDLE;
#else
#  include <semaphore.h>
using GlobalSemHandle = sem_t*;
#endif

/*
 * nameToken should be a string very briefly naming the semaphore you are
 * creating, and it should be unique systemwide except for across multiple
 * instances of the same application.
 * installPath should be the path to the directory containing the application.
 *
 * Taken together, those two parameters will be used to form a string which is
 * unique to this copy of this application.
 * Creating the semaphore will fail if the final string (the token plus the path
 * hash) is longer than the platform's maximum. On Windows that's MAX_PATH, on
 * POSIX systems it's either _POSIX_PATH_MAX or perhaps SEM_NAME_LEN.
 *
 * Returns nullptr upon failure, on all platforms.
 */
GlobalSemHandle OpenGlobalSemaphore(const char* nameToken,
                                    const char16_t* installPath);

void ReleaseGlobalSemaphore(GlobalSemHandle sem);

// aResult will be set to true if another instance *was* found, false if not.
// Return value is true on success, false on error (and aResult won't be set).
bool IsOtherInstanceRunning(GlobalSemHandle sem, bool* aResult);

};  // namespace mozilla

#endif  // GLOBALEMAPHORE_H
