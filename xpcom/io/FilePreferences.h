/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIObserver.h"

namespace mozilla {
namespace FilePreferences {

void InitPrefs();
void InitDirectoriesWhitelist();
bool IsBlockedUNCPath(const nsAString& aFilePath);

namespace testing {

void SetBlockUNCPaths(bool aBlock);
void AddDirectoryToWhitelist(nsAString const& aPath);
bool NormalizePath(nsAString const & aPath, nsAString & aNormalized);

}

} // FilePreferences
} // mozilla
