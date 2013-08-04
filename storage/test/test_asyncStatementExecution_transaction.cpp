/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

#include "storage_test_harness.h"

#include "nsIEventTarget.h"
#include "mozStorageConnection.h"

#include "sqlite3.h"

using namespace mozilla;
using namespace mozilla::storage;

////////////////////////////////////////////////////////////////////////////////
//// Helpers

/**
 * Commit hook to detect transactions.
 *
 * @param aArg
 *        An integer pointer that will be incremented for each commit.
 */
int commit_hook(void *aArg)
{
  int *arg = static_cast<int *>(aArg);
  (*arg)++;
  return 0;
}

/**
 * Executes the passed-in statements and checks if a transaction is created.
 * When done statements are finalized and database connection is closed.
 *
 * @param aDB
 *        The database connection.
 * @param aStmts
 *        Vector of statements.
 * @param aStmtsLen
 *        Number of statements.
 * @param aTransactionExpected
 *        Whether a transaction is expected or not.
 */
void
check_transaction(mozIStorageConnection *aDB,
                  mozIStorageBaseStatement **aStmts,
                  uint32_t aStmtsLen,
                  bool aTransactionExpected)
{
  // -- install a transaction commit hook.
  int commit = 0;
  ::sqlite3_commit_hook(*static_cast<Connection *>(aDB), commit_hook, &commit);

  nsRefPtr<AsyncStatementSpinner> asyncSpin(new AsyncStatementSpinner());
  nsCOMPtr<mozIStoragePendingStatement> asyncPend;
  do_check_success(aDB->ExecuteAsync(aStmts, aStmtsLen, asyncSpin,
                                     getter_AddRefs(asyncPend)));
  do_check_true(asyncPend);

  // -- complete the execution
  asyncSpin->SpinUntilCompleted();

  // -- uninstall the transaction commit hook.
  ::sqlite3_commit_hook(*static_cast<Connection *>(aDB), nullptr, nullptr);

  // -- check transaction
  do_check_eq(aTransactionExpected, !!commit);

  // -- check that only one transaction was created.
  if (aTransactionExpected) {
    do_check_eq(1, commit);
  }

  // -- cleanup
  for (uint32_t i = 0; i < aStmtsLen; ++i) {
    aStmts[i]->Finalize();
  }
  blocking_async_close(aDB);
}

////////////////////////////////////////////////////////////////////////////////
//// Tests

/**
 * Test that executing multiple readonly AsyncStatements doesn't create a
 * transaction.
 */
void
test_MultipleAsyncReadStatements()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // -- create statements and execute them
  nsCOMPtr<mozIStorageAsyncStatement> stmt1;
  db->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "SELECT * FROM sqlite_master"
  ), getter_AddRefs(stmt1));

  nsCOMPtr<mozIStorageAsyncStatement> stmt2;
  db->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "SELECT * FROM sqlite_master"
  ), getter_AddRefs(stmt2));

  mozIStorageBaseStatement *stmts[] = {
    stmt1,
    stmt2,
  };

  check_transaction(db, stmts, ArrayLength(stmts), false);
}

/**
 * Test that executing multiple readonly Statements doesn't create a
 * transaction.
 */
void
test_MultipleReadStatements()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // -- create statements and execute them
  nsCOMPtr<mozIStorageStatement> stmt1;
  db->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT * FROM sqlite_master"
  ), getter_AddRefs(stmt1));

  nsCOMPtr<mozIStorageStatement> stmt2;
  db->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT * FROM sqlite_master"
  ), getter_AddRefs(stmt2));

  mozIStorageBaseStatement *stmts[] = {
    stmt1,
    stmt2,
  };

  check_transaction(db, stmts, ArrayLength(stmts), false);
}

/**
 * Test that executing multiple AsyncStatements causing writes creates a
 * transaction.
 */
void
test_MultipleAsyncReadWriteStatements()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // -- create statements and execute them
  nsCOMPtr<mozIStorageAsyncStatement> stmt1;
  db->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "SELECT * FROM sqlite_master"
  ), getter_AddRefs(stmt1));

  nsCOMPtr<mozIStorageAsyncStatement> stmt2;
  db->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "CREATE TABLE test (id INTEGER PRIMARY KEY)"
  ), getter_AddRefs(stmt2));

  mozIStorageBaseStatement *stmts[] = {
    stmt1,
    stmt2,
  };

  check_transaction(db, stmts, ArrayLength(stmts), true);
}

/**
 * Test that executing multiple Statements causing writes creates a transaction.
 */
void
test_MultipleReadWriteStatements()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // -- create statements and execute them
  nsCOMPtr<mozIStorageStatement> stmt1;
  db->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT * FROM sqlite_master"
  ), getter_AddRefs(stmt1));

  nsCOMPtr<mozIStorageStatement> stmt2;
  db->CreateStatement(NS_LITERAL_CSTRING(
    "CREATE TABLE test (id INTEGER PRIMARY KEY)"
  ), getter_AddRefs(stmt2));

  mozIStorageBaseStatement *stmts[] = {
    stmt1,
    stmt2,
  };

  check_transaction(db, stmts, ArrayLength(stmts), true);
}

/**
 * Test that executing multiple AsyncStatements causing writes creates a
 * single transaction.
 */
void
test_MultipleAsyncWriteStatements()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // -- create statements and execute them
  nsCOMPtr<mozIStorageAsyncStatement> stmt1;
  db->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "CREATE TABLE test1 (id INTEGER PRIMARY KEY)"
  ), getter_AddRefs(stmt1));

  nsCOMPtr<mozIStorageAsyncStatement> stmt2;
  db->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "CREATE TABLE test2 (id INTEGER PRIMARY KEY)"
  ), getter_AddRefs(stmt2));

  mozIStorageBaseStatement *stmts[] = {
    stmt1,
    stmt2,
  };

  check_transaction(db, stmts, ArrayLength(stmts), true);
}

/**
 * Test that executing multiple Statements causing writes creates a
 * single transaction.
 */
void
test_MultipleWriteStatements()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // -- create statements and execute them
  nsCOMPtr<mozIStorageStatement> stmt1;
  db->CreateStatement(NS_LITERAL_CSTRING(
    "CREATE TABLE test1 (id INTEGER PRIMARY KEY)"
  ), getter_AddRefs(stmt1));

  nsCOMPtr<mozIStorageStatement> stmt2;
  db->CreateStatement(NS_LITERAL_CSTRING(
    "CREATE TABLE test2 (id INTEGER PRIMARY KEY)"
  ), getter_AddRefs(stmt2));

  mozIStorageBaseStatement *stmts[] = {
    stmt1,
    stmt2,
  };

  check_transaction(db, stmts, ArrayLength(stmts), true);
}

/**
 * Test that executing a single read-only AsyncStatement doesn't create a
 * transaction.
 */
void
test_SingleAsyncReadStatement()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // -- create statements and execute them
  nsCOMPtr<mozIStorageAsyncStatement> stmt;
  db->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "SELECT * FROM sqlite_master"
  ), getter_AddRefs(stmt));

  mozIStorageBaseStatement *stmts[] = {
    stmt,
  };

  check_transaction(db, stmts, ArrayLength(stmts), false);
}

/**
 * Test that executing a single read-only Statement doesn't create a
 * transaction.
 */
void
test_SingleReadStatement()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // -- create statements and execute them
  nsCOMPtr<mozIStorageStatement> stmt;
  db->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT * FROM sqlite_master"
  ), getter_AddRefs(stmt));

  mozIStorageBaseStatement *stmts[] = {
    stmt,
  };

  check_transaction(db, stmts, ArrayLength(stmts), false);
}

/**
 * Test that executing a single AsyncStatement causing writes creates a
 * transaction.
 */
void
test_SingleAsyncWriteStatement()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // -- create statements and execute them
  nsCOMPtr<mozIStorageAsyncStatement> stmt;
  db->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "CREATE TABLE test (id INTEGER PRIMARY KEY)"
  ), getter_AddRefs(stmt));

  mozIStorageBaseStatement *stmts[] = {
    stmt,
  };

  check_transaction(db, stmts, ArrayLength(stmts), true);
}

/**
 * Test that executing a single Statement causing writes creates a transaction.
 */
void
test_SingleWriteStatement()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // -- create statements and execute them
  nsCOMPtr<mozIStorageStatement> stmt;
  db->CreateStatement(NS_LITERAL_CSTRING(
    "CREATE TABLE test (id INTEGER PRIMARY KEY)"
  ), getter_AddRefs(stmt));

  mozIStorageBaseStatement *stmts[] = {
    stmt,
  };

  check_transaction(db, stmts, ArrayLength(stmts), true);
}

/**
 * Test that executing a single read-only AsyncStatement with multiple params
 * doesn't create a transaction.
 */
void
test_MultipleParamsAsyncReadStatement()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // -- create statements and execute them
  nsCOMPtr<mozIStorageAsyncStatement> stmt;
  db->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "SELECT :param FROM sqlite_master"
  ), getter_AddRefs(stmt));

  // -- bind multiple BindingParams
  nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
  stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
  for (int32_t i = 0; i < 2; i++) {
    nsCOMPtr<mozIStorageBindingParams> params;
    paramsArray->NewBindingParams(getter_AddRefs(params));
    params->BindInt32ByName(NS_LITERAL_CSTRING("param"), 1);
    paramsArray->AddParams(params);
  }
  stmt->BindParameters(paramsArray);
  paramsArray = nullptr;

  mozIStorageBaseStatement *stmts[] = {
    stmt,
  };

  check_transaction(db, stmts, ArrayLength(stmts), false);
}

/**
 * Test that executing a single read-only Statement with multiple params
 * doesn't create a transaction.
 */
void
test_MultipleParamsReadStatement()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // -- create statements and execute them
  nsCOMPtr<mozIStorageStatement> stmt;
  db->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT :param FROM sqlite_master"
  ), getter_AddRefs(stmt));

  // -- bind multiple BindingParams
  nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
  stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
  for (int32_t i = 0; i < 2; i++) {
    nsCOMPtr<mozIStorageBindingParams> params;
    paramsArray->NewBindingParams(getter_AddRefs(params));
    params->BindInt32ByName(NS_LITERAL_CSTRING("param"), 1);
    paramsArray->AddParams(params);
  }
  stmt->BindParameters(paramsArray);
  paramsArray = nullptr;

  mozIStorageBaseStatement *stmts[] = {
    stmt,
  };

  check_transaction(db, stmts, ArrayLength(stmts), false);
}

/**
 * Test that executing a single write AsyncStatement with multiple params
 * creates a transaction.
 */
void
test_MultipleParamsAsyncWriteStatement()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // -- create a table for writes
  nsCOMPtr<mozIStorageStatement> tableStmt;
  db->CreateStatement(NS_LITERAL_CSTRING(
    "CREATE TABLE test (id INTEGER PRIMARY KEY)"
  ), getter_AddRefs(tableStmt));
  tableStmt->Execute();
  tableStmt->Finalize();

  // -- create statements and execute them
  nsCOMPtr<mozIStorageAsyncStatement> stmt;
  db->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "DELETE FROM test WHERE id = :param"
  ), getter_AddRefs(stmt));

  // -- bind multiple BindingParams
  nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
  stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
  for (int32_t i = 0; i < 2; i++) {
    nsCOMPtr<mozIStorageBindingParams> params;
    paramsArray->NewBindingParams(getter_AddRefs(params));
    params->BindInt32ByName(NS_LITERAL_CSTRING("param"), 1);
    paramsArray->AddParams(params);
  }
  stmt->BindParameters(paramsArray);
  paramsArray = nullptr;

  mozIStorageBaseStatement *stmts[] = {
    stmt,
  };

  check_transaction(db, stmts, ArrayLength(stmts), true);
}

/**
 * Test that executing a single write Statement with multiple params
 * creates a transaction.
 */
void
test_MultipleParamsWriteStatement()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // -- create a table for writes
  nsCOMPtr<mozIStorageStatement> tableStmt;
  db->CreateStatement(NS_LITERAL_CSTRING(
    "CREATE TABLE test (id INTEGER PRIMARY KEY)"
  ), getter_AddRefs(tableStmt));
  tableStmt->Execute();
  tableStmt->Finalize();

  // -- create statements and execute them
  nsCOMPtr<mozIStorageStatement> stmt;
  db->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM test WHERE id = :param"
  ), getter_AddRefs(stmt));

  // -- bind multiple BindingParams
  nsCOMPtr<mozIStorageBindingParamsArray> paramsArray;
  stmt->NewBindingParamsArray(getter_AddRefs(paramsArray));
  for (int32_t i = 0; i < 2; i++) {
    nsCOMPtr<mozIStorageBindingParams> params;
    paramsArray->NewBindingParams(getter_AddRefs(params));
    params->BindInt32ByName(NS_LITERAL_CSTRING("param"), 1);
    paramsArray->AddParams(params);
  }
  stmt->BindParameters(paramsArray);
  paramsArray = nullptr;

  mozIStorageBaseStatement *stmts[] = {
    stmt,
  };

  check_transaction(db, stmts, ArrayLength(stmts), true);
}

void (*gTests[])(void) = {
  test_MultipleAsyncReadStatements,
  test_MultipleReadStatements,
  test_MultipleAsyncReadWriteStatements,
  test_MultipleReadWriteStatements,
  test_MultipleAsyncWriteStatements,
  test_MultipleWriteStatements,
  test_SingleAsyncReadStatement,
  test_SingleReadStatement,
  test_SingleAsyncWriteStatement,
  test_SingleWriteStatement,
  test_MultipleParamsAsyncReadStatement,
  test_MultipleParamsReadStatement,
  test_MultipleParamsAsyncWriteStatement,
  test_MultipleParamsWriteStatement,
};

const char *file = __FILE__;
#define TEST_NAME "async statement execution transaction"
#define TEST_FILE file
#include "storage_test_harness_tail.h"
