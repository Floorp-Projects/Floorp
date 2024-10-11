/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WinTokenUtils_h
#define mozilla_WinTokenUtils_h

#include "mozilla/WinHeaderOnlyUtils.h"

namespace mozilla {

LauncherResult<bool> IsAdminWithoutUac();

}  // namespace mozilla

#endif  // mozilla_WinTokenUtils_h
