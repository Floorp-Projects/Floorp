/*
 *Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "storage_test_harness.h"
#include "mozIStorageRow.h"
#include "mozIStorageResultSet.h"

/**
 * This file tests AsXXX (AsInt32, AsInt64, ...) helpers.
 */

////////////////////////////////////////////////////////////////////////////////
//// Event Loop Spinning

class Spinner : public AsyncStatementSpinner
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_ASYNCSTATEMENTSPINNER
  Spinner() {}
};

NS_IMPL_ISUPPORTS_INHERITED0(Spinner,
                             AsyncStatementSpinner)

NS_IMETHODIMP
Spinner::HandleResult(mozIStorageResultSet *aResultSet)
{
  nsCOMPtr<mozIStorageRow> row;
  do_check_true(NS_SUCCEEDED(aResultSet->GetNextRow(getter_AddRefs(row))) && row);

  do_check_eq(row->AsInt32(0), 0);
  do_check_eq(row->AsInt64(0), 0);
  do_check_eq(row->AsDouble(0), 0.0);

  PRUint32 len = 100;
  do_check_eq(row->AsSharedUTF8String(0, &len), NULL);
  do_check_eq(len, 0);
  len = 100;
  do_check_eq(row->AsSharedWString(0, &len), NULL);
  do_check_eq(len, 0);
  len = 100;
  do_check_eq(row->AsSharedBlob(0, &len), NULL);
  do_check_eq(len, 0);

  do_check_eq(row->IsNull(0), true);
  return NS_OK;
}

void
test_NULLFallback()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  nsCOMPtr<mozIStorageStatement> stmt;
  (void)db->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT NULL"
  ), getter_AddRefs(stmt));

  nsCOMPtr<mozIStorageValueArray> valueArray = do_QueryInterface(stmt);
  do_check_true(valueArray);

  bool hasMore;
  do_check_true(NS_SUCCEEDED(stmt->ExecuteStep(&hasMore)) && hasMore);

  do_check_eq(stmt->AsInt32(0), 0);
  do_check_eq(stmt->AsInt64(0), 0);
  do_check_eq(stmt->AsDouble(0), 0.0);
  PRUint32 len = 100;
  do_check_eq(stmt->AsSharedUTF8String(0, &len), NULL);
  do_check_eq(len, 0);
  len = 100;
  do_check_eq(stmt->AsSharedWString(0, &len), NULL);
  do_check_eq(len, 0);
  len = 100;
  do_check_eq(stmt->AsSharedBlob(0, &len), NULL);
  do_check_eq(len, 0);
  do_check_eq(stmt->IsNull(0), true);

  do_check_eq(valueArray->AsInt32(0), 0);
  do_check_eq(valueArray->AsInt64(0), 0);
  do_check_eq(valueArray->AsDouble(0), 0.0);
  len = 100;
  do_check_eq(valueArray->AsSharedUTF8String(0, &len), NULL);
  do_check_eq(len, 0);
  len = 100;
  do_check_eq(valueArray->AsSharedWString(0, &len), NULL);
  do_check_eq(len, 0);
  len = 100;
  do_check_eq(valueArray->AsSharedBlob(0, &len), NULL);
  do_check_eq(len, 0);
  do_check_eq(valueArray->IsNull(0), true);
}

void
test_asyncNULLFallback()
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  nsCOMPtr<mozIStorageAsyncStatement> stmt;
  (void)db->CreateAsyncStatement(NS_LITERAL_CSTRING(
    "SELECT NULL"
  ), getter_AddRefs(stmt));

  nsCOMPtr<mozIStoragePendingStatement> pendingStmt;
  do_check_true(NS_SUCCEEDED(stmt->ExecuteAsync(nsnull, getter_AddRefs(pendingStmt))));
  do_check_true(pendingStmt);
  stmt->Finalize();
  nsRefPtr<Spinner> asyncSpin(new Spinner());
  db->AsyncClose(asyncSpin);
  asyncSpin->SpinUntilCompleted();

}

void (*gTests[])(void) = {
  test_NULLFallback
, test_asyncNULLFallback
};

const char *file = __FILE__;
#define TEST_NAME "AsXXX helpers"
#define TEST_FILE file
#include "storage_test_harness_tail.h"
