/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is storage test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "storage_test_harness.h"

#include "mozStorageHelper.h"

/**
 * This file test our statement scoper in mozStorageHelper.h.
 */

void
test_automatic_reset()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // Need to create a table to populate sqlite_master with an entry.
  (void)db->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE test (id INTEGER PRIMARY KEY)"
  ));

  nsCOMPtr<mozIStorageStatement> stmt;
  (void)db->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT * FROM sqlite_master"
  ), getter_AddRefs(stmt));

  // Reality check - make sure we start off in the right state.
  PRInt32 state = -1;
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

void
test_Abandon()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // Need to create a table to populate sqlite_master with an entry.
  (void)db->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
    "CREATE TABLE test (id INTEGER PRIMARY KEY)"
  ));

  nsCOMPtr<mozIStorageStatement> stmt;
  (void)db->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT * FROM sqlite_master"
  ), getter_AddRefs(stmt));

  // Reality check - make sure we start off in the right state.
  PRInt32 state = -1;
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

void (*gTests[])(void) = {
  test_automatic_reset,
  test_Abandon,
};

const char *file = __FILE__;
#define TEST_NAME "statement scoper"
#define TEST_FILE file
#include "storage_test_harness_tail.h"
