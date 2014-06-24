/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozStorageArgValueArray_h
#define mozStorageArgValueArray_h

#include "mozIStorageValueArray.h"
#include "mozilla/Attributes.h"

#include "sqlite3.h"

namespace mozilla {
namespace storage {

class ArgValueArray MOZ_FINAL : public mozIStorageValueArray
{
public:
  ArgValueArray(int32_t aArgc, sqlite3_value **aArgv);

  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGEVALUEARRAY

private:
  ~ArgValueArray() {}

  uint32_t mArgc;
  sqlite3_value **mArgv;
};

} // namespace storage
} // namespace mozilla

#endif // mozStorageArgValueArray_h
