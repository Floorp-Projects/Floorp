/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "prio.h"
#include "nscore.h"

/**
 * OpenAnonymousTemporaryFile
 *
 * Creates and opens a temporary file which has a random name. Callers have no
 * control over the file name, and the file is opened in a temporary location
 * which is appropriate for the platform.
 *
 * Upon success, aOutFileDesc contains an opened handle to the temporary file.
 * The caller is responsible for closing the file when they're finished with it.
 *
 * The file will be deleted when the file handle is closed. On non-Windows
 * platforms the file will be unlinked before this function returns. On Windows
 * the OS supplied delete-on-close mechanism is unreliable if the application
 * crashes or the computer power cycles unexpectedly, so unopened temporary
 * files are purged at some time after application startup.
 *
 */
nsresult
NS_OpenAnonymousTemporaryFile(PRFileDesc** aOutFileDesc);

