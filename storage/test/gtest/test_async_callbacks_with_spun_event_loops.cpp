/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

#include "storage_test_harness.h"
#include "prthread.h"
#include "nsIInterfaceRequestorUtils.h"
#include "mozilla/Attributes.h"

#include "sqlite3.h"

////////////////////////////////////////////////////////////////////////////////
//// Async Helpers

/**
 * Spins the events loop for current thread until aCondition is true.
 */
void spin_events_loop_until_true(const bool* const aCondition) {
  nsCOMPtr<nsIThread> thread(::do_GetCurrentThread());
  nsresult rv = NS_OK;
  bool processed = true;
  while (!(*aCondition) && NS_SUCCEEDED(rv)) {
    rv = thread->ProcessNextEvent(true, &processed);
  }
}

////////////////////////////////////////////////////////////////////////////////
//// mozIStorageStatementCallback implementation

class UnownedCallback final : public mozIStorageStatementCallback {
 public:
  NS_DECL_ISUPPORTS

  // Whether the object has been destroyed.
  static bool sAlive;
  // Whether the first result was received.
  static bool sResult;
  // Whether an error was received.
  static bool sError;

  explicit UnownedCallback(mozIStorageConnection* aDBConn)
      : mDBConn(aDBConn), mCompleted(false) {
    sAlive = true;
    sResult = false;
    sError = false;
  }

 private:
  ~UnownedCallback() {
    sAlive = false;
    blocking_async_close(mDBConn);
  }

 public:
  NS_IMETHOD HandleResult(mozIStorageResultSet* aResultSet) override {
    sResult = true;
    spin_events_loop_until_true(&mCompleted);
    if (!sAlive) {
      MOZ_CRASH("The statement callback was destroyed prematurely.");
    }
    return NS_OK;
  }

  NS_IMETHOD HandleError(mozIStorageError* aError) override {
    sError = true;
    spin_events_loop_until_true(&mCompleted);
    if (!sAlive) {
      MOZ_CRASH("The statement callback was destroyed prematurely.");
    }
    return NS_OK;
  }

  NS_IMETHOD HandleCompletion(uint16_t aReason) override {
    mCompleted = true;
    return NS_OK;
  }

 protected:
  nsCOMPtr<mozIStorageConnection> mDBConn;
  bool mCompleted;
};

NS_IMPL_ISUPPORTS(UnownedCallback, mozIStorageStatementCallback)

bool UnownedCallback::sAlive = false;
bool UnownedCallback::sResult = false;
bool UnownedCallback::sError = false;

////////////////////////////////////////////////////////////////////////////////
//// Tests

TEST(storage_async_callbacks_with_spun_event_loops,
     SpinEventsLoopInHandleResult)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // Create a test table and populate it.
  nsCOMPtr<mozIStorageStatement> stmt;
  db->CreateStatement("CREATE TABLE test (id INTEGER PRIMARY KEY)"_ns,
                      getter_AddRefs(stmt));
  stmt->Execute();
  stmt->Finalize();

  db->CreateStatement("INSERT INTO test (id) VALUES (?)"_ns,
                      getter_AddRefs(stmt));
  for (int32_t i = 0; i < 30; ++i) {
    stmt->BindInt32ByIndex(0, i);
    stmt->Execute();
    stmt->Reset();
  }
  stmt->Finalize();

  db->CreateStatement("SELECT * FROM test"_ns, getter_AddRefs(stmt));
  nsCOMPtr<mozIStoragePendingStatement> ps;
  do_check_success(
      stmt->ExecuteAsync(new UnownedCallback(db), getter_AddRefs(ps)));
  stmt->Finalize();

  spin_events_loop_until_true(&UnownedCallback::sResult);
}

TEST(storage_async_callbacks_with_spun_event_loops, SpinEventsLoopInHandleError)
{
  nsCOMPtr<mozIStorageConnection> db(getMemoryDatabase());

  // Create a test table and populate it.
  nsCOMPtr<mozIStorageStatement> stmt;
  db->CreateStatement("CREATE TABLE test (id INTEGER PRIMARY KEY)"_ns,
                      getter_AddRefs(stmt));
  stmt->Execute();
  stmt->Finalize();

  db->CreateStatement("INSERT INTO test (id) VALUES (1)"_ns,
                      getter_AddRefs(stmt));
  stmt->Execute();
  stmt->Finalize();

  // This will cause a constraint error.
  db->CreateStatement("INSERT INTO test (id) VALUES (1)"_ns,
                      getter_AddRefs(stmt));
  nsCOMPtr<mozIStoragePendingStatement> ps;
  do_check_success(
      stmt->ExecuteAsync(new UnownedCallback(db), getter_AddRefs(ps)));
  stmt->Finalize();

  spin_events_loop_until_true(&UnownedCallback::sError);
}
