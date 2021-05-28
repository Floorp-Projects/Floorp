/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MsiDatabase_h__
#define __MsiDatabase_h__

#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "nsWindowsHelpers.h"

#include <windows.h>
#include <msi.h>
#include <msiquery.h>

namespace mozilla {

class MsiDatabase final {
  static UniquePtr<wchar_t[]> GetRecordString(MSIHANDLE aRecord,
                                              UINT aFieldIndex);

  nsAutoMsiHandle mDatabase;

  MsiDatabase() = default;
  explicit MsiDatabase(const wchar_t* aDatabasePath);

 public:
  // A callback function passed to ExecuteSingleColumnQuery uses this type
  // to control the enumeration loop.
  enum class CallbackResult { Continue, Stop };

  static Maybe<MsiDatabase> FromProductId(const wchar_t* aProductId);

  MsiDatabase(const MsiDatabase&) = delete;
  MsiDatabase& operator=(const MsiDatabase&) = delete;
  MsiDatabase(MsiDatabase&& aOther) : mDatabase(aOther.mDatabase.disown()) {}
  MsiDatabase& operator=(MsiDatabase&& aOther) {
    if (this != &aOther) {
      mDatabase.own(aOther.mDatabase.disown());
    }
    return *this;
  }

  explicit operator bool() const;

  template <typename CallbackT>
  bool ExecuteSingleColumnQuery(const wchar_t* aQuery,
                                const CallbackT& aCallback) const {
    MSIHANDLE handle;
    UINT ret = ::MsiDatabaseOpenViewW(mDatabase, aQuery, &handle);
    if (ret != ERROR_SUCCESS) {
      return false;
    }

    nsAutoMsiHandle view(handle);

    ret = ::MsiViewExecute(view, 0);
    if (ret != ERROR_SUCCESS) {
      return false;
    }

    for (;;) {
      ret = ::MsiViewFetch(view, &handle);
      if (ret == ERROR_NO_MORE_ITEMS) {
        break;
      } else if (ret != ERROR_SUCCESS) {
        return false;
      }

      nsAutoMsiHandle record(handle);
      UniquePtr<wchar_t[]> guidStr = GetRecordString(record, 1);
      if (!guidStr) {
        continue;
      }

      CallbackResult result = aCallback(guidStr.get());
      if (result == CallbackResult::Continue) {
        continue;
      } else if (result == CallbackResult::Stop) {
        break;
      } else {
        MOZ_ASSERT_UNREACHABLE("Unexpected CallbackResult.");
      }
    }

    return true;
  }
};

}  // namespace mozilla

#endif  // __MsiDatabase_h__
