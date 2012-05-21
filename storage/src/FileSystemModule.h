/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_storage_FileSystemModule_h
#define mozilla_storage_FileSystemModule_h

#include "nscore.h"

struct sqlite3;

namespace mozilla {
namespace storage {

NS_HIDDEN_(int) RegisterFileSystemModule(sqlite3* aDB, const char* aName);

} // namespace storage
} // namespace mozilla

#endif // mozilla_storage_FileSystemModule_h
