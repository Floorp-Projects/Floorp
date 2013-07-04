/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_storage_h_
#define mozilla_storage_h_

////////////////////////////////////////////////////////////////////////////////
//// Public Interfaces

#include "mozStorageCID.h"
#include "mozIStorageAggregateFunction.h"
#include "mozIStorageConnection.h"
#include "mozIStorageError.h"
#include "mozIStorageFunction.h"
#include "mozIStoragePendingStatement.h"
#include "mozIStorageProgressHandler.h"
#include "mozIStorageResultSet.h"
#include "mozIStorageRow.h"
#include "mozIStorageService.h"
#include "mozIStorageStatement.h"
#include "mozIStorageStatementCallback.h"
#include "mozIStorageBindingParamsArray.h"
#include "mozIStorageBindingParams.h"
#include "mozIStorageVacuumParticipant.h"
#include "mozIStorageCompletionCallback.h"
#include "mozIStorageAsyncStatement.h"
#include "mozIStorageAsyncConnection.h"

////////////////////////////////////////////////////////////////////////////////
//// Native Language Helpers

#include "mozStorageHelper.h"
#include "mozilla/storage/StatementCache.h"
#include "mozilla/storage/Variant.h"

#endif // mozilla_storage_h_
