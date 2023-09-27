/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef STORAGE_OBFUSCATINGVFS_H_
#define STORAGE_OBFUSCATINGVFS_H_

#include "mozilla/UniquePtr.h"

struct sqlite3_vfs;
struct sqlite3_file;

template <typename T>
struct already_AddRefed;

namespace mozilla::dom::quota {
class QuotaObject;
}

namespace mozilla::storage::obfsvfs {

const char* GetVFSName();

UniquePtr<sqlite3_vfs> ConstructVFS(const char* aBaseVFSName);

already_AddRefed<dom::quota::QuotaObject> GetQuotaObjectForFile(
    sqlite3_file* pFile);

}  // namespace mozilla::storage::obfsvfs

#endif  // STORAGE_OBFUSCATINGVFS_H_
