/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_storage_IStorageBindingParamsInternal_h_
#define mozilla_storage_IStorageBindingParamsInternal_h_

#include "nsISupports.h"

struct sqlite3_stmt;
class mozIStorageError;

namespace mozilla {
namespace storage {

#define ISTORAGEBINDINGPARAMSINTERNAL_IID \
  {0x4c43d33a, 0xc620, 0x41b8, {0xba, 0x1d, 0x50, 0xc5, 0xb1, 0xe9, 0x1a, 0x04}}

/**
 * Implementation-only interface for mozIStorageBindingParams.  This defines the
 * set of methods required by the asynchronous execution code in order to
 * consume the contents stored in mozIStorageBindingParams instances.
 */
class IStorageBindingParamsInternal : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(ISTORAGEBINDINGPARAMSINTERNAL_IID)

  /**
   * Binds our stored data to the statement.
   *
   * @param aStatement
   *        The statement to bind our data to.
   * @return nullptr on success, or a mozIStorageError object if an error
   *         occurred.
   */
  virtual already_AddRefed<mozIStorageError> bind(sqlite3_stmt *aStatement) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(IStorageBindingParamsInternal,
                              ISTORAGEBINDINGPARAMSINTERNAL_IID)

#define NS_DECL_ISTORAGEBINDINGPARAMSINTERNAL \
  already_AddRefed<mozIStorageError> bind(sqlite3_stmt *aStatement) MOZ_OVERRIDE;

} // storage
} // mozilla

#endif // mozilla_storage_IStorageBindingParamsInternal_h_
