/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "storage_test_harness.h"

#include "mozStorageHelper.h"

/**
 * This file test our statement scoper in mozStorageHelper.h.
 */

TEST(storage_statement_scoper, automatic_reset)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // Need to create a table to populate sqlite_master with an entry.
  (void)db->ExecuteSimpleSQL("CREATE TABLE test (id INTEGER PRIMARY KEY)"_ns);

  nsCOMPtr<mozIStorageStatement> stmt;
  (void)db->CreateStatement("SELECT * FROM sqlite_master"_ns,
                            getter_AddRefs(stmt));

  // Reality check - make sure we start off in the right state.
  int32_t state = -1;
  (void)stmt->GetState(&state);
  do_check_true(state == mozIStorageStatement::MOZ_STORAGE_STATEMENT_READY);

  // Start executing the statement, which will put it into an executing state.
  {
    mozStorageStatementScoper scoper(stmt);
    bool hasMore;
    do_check_true(NS_SUCCEEDED(stmt->ExecuteStep(&hasMore)));

    // Reality check that we are executing.
    state = -1;
    (void)stmt->GetState(&state);
    do_check_true(state ==
                  mozIStorageStatement::MOZ_STORAGE_STATEMENT_EXECUTING);
  }

  // And we should be ready again.
  state = -1;
  (void)stmt->GetState(&state);
  do_check_true(state == mozIStorageStatement::MOZ_STORAGE_STATEMENT_READY);
}

TEST(storage_statement_scoper, Abandon)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // Need to create a table to populate sqlite_master with an entry.
  (void)db->ExecuteSimpleSQL("CREATE TABLE test (id INTEGER PRIMARY KEY)"_ns);

  nsCOMPtr<mozIStorageStatement> stmt;
  (void)db->CreateStatement("SELECT * FROM sqlite_master"_ns,
                            getter_AddRefs(stmt));

  // Reality check - make sure we start off in the right state.
  int32_t state = -1;
  (void)stmt->GetState(&state);
  do_check_true(state == mozIStorageStatement::MOZ_STORAGE_STATEMENT_READY);

  // Start executing the statement, which will put it into an executing state.
  {
    mozStorageStatementScoper scoper(stmt);
    bool hasMore;
    do_check_true(NS_SUCCEEDED(stmt->ExecuteStep(&hasMore)));

    // Reality check that we are executing.
    state = -1;
    (void)stmt->GetState(&state);
    do_check_true(state ==
                  mozIStorageStatement::MOZ_STORAGE_STATEMENT_EXECUTING);

    // And call Abandon.  We should not reset now when we fall out of scope.
    scoper.Abandon();
  }

  // And we should still be executing.
  state = -1;
  (void)stmt->GetState(&state);
  do_check_true(state == mozIStorageStatement::MOZ_STORAGE_STATEMENT_EXECUTING);
}
