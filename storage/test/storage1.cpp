
#include <stdlib.h>
#include <stdio.h>

#include "nsIComponentManager.h"
#include "nsISimpleEnumerator.h"
#include "nsIServiceManager.h"
#include "nsILocalFile.h"
#include "nsIVariant.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsCOMPtr.h"
#include "nsStringAPI.h"

#include "mozIStorageService.h"
#include "mozIStorageConnection.h"
#include "mozIStorageValueArray.h"
#include "mozIStorageStatement.h"
#include "mozIStorageFunction.h"
#include "mozIStorageAggregateFunction.h"
#include "mozIStorageProgressHandler.h"

#include "mozStorageCID.h"

#include "nsXPCOMCID.h"

#define TEST_CHECK_ERROR(rv) \
        do { if (NS_FAILED(rv)) {              \
            dbConn->GetLastError(&gerr); \
            dbConn->GetLastErrorString(gerrstr); \
            fprintf (stderr, "Error: %d 0x%08x %s\n", gerr, gerr, gerrstr.get()); \
            return 0; \
            } } while (0)

#define TEST_DB NS_LITERAL_CSTRING("foo.sdb")

int gerr;
nsCString gerrstr;

class TestFunc : public mozIStorageFunction {
public:
    TestFunc() { }
    NS_DECL_ISUPPORTS
    NS_DECL_MOZISTORAGEFUNCTION
};

NS_IMPL_ISUPPORTS1(TestFunc, mozIStorageFunction)

NS_IMETHODIMP
TestFunc::OnFunctionCall (mozIStorageValueArray *sva, nsIVariant **_retval)
{
    nsCOMPtr<nsIWritableVariant> outVar;

    fprintf (stderr, "* function call!\n");

    outVar = do_CreateInstance(NS_VARIANT_CONTRACTID);
    if(!outVar)
        return NS_ERROR_FAILURE;
    outVar->SetAsInt32(0xDEADBEEF);
    NS_ADDREF(*_retval = outVar);
    return NS_OK;
}

class TestAggregateFunc : public mozIStorageAggregateFunction {
private:
    PRInt32 mCalls;
public:
    TestAggregateFunc() : mCalls(0) { }
    NS_DECL_ISUPPORTS
    NS_DECL_MOZISTORAGEAGGREGATEFUNCTION
};

NS_IMPL_ISUPPORTS1(TestAggregateFunc, mozIStorageAggregateFunction)

NS_IMETHODIMP
TestAggregateFunc::OnStep (mozIStorageValueArray *sva)
{
    ++mCalls;
    fprintf (stderr, "* aggregate step %d!\n", mCalls);
    return NS_OK;
}

NS_IMETHODIMP
TestAggregateFunc::OnFinal (nsIVariant **_retval)
{
    nsCOMPtr<nsIWritableVariant> outVar;

    fprintf (stderr, "* aggregate result %d!\n", - mCalls * mCalls);

    outVar = do_CreateInstance(NS_VARIANT_CONTRACTID);
    if(!outVar)
        return NS_ERROR_FAILURE;
    outVar->SetAsInt32(-mCalls * mCalls);
    NS_ADDREF(*_retval = outVar);
    return NS_OK;
}

class TestProgressHandler : public mozIStorageProgressHandler {
private:
    PRUint32 mTicks;
public:
    TestProgressHandler() : mTicks(0) { }
    NS_DECL_ISUPPORTS
    NS_DECL_MOZISTORAGEPROGRESSHANDLER
    PRUint32 getTicks() const { return mTicks; }
};

NS_IMPL_ISUPPORTS1(TestProgressHandler, mozIStorageProgressHandler)

NS_IMETHODIMP
TestProgressHandler::OnProgress (mozIStorageConnection *aConnection, PRBool *_retval)
{
    ++mTicks;
    *_retval = PR_FALSE;
    return NS_OK;
}

int
main (int argc, char **argv)
{
    nsresult rv;
    TestFunc *tf;
    TestAggregateFunc *taf;

    NS_InitXPCOM2(nsnull, nsnull, nsnull);

    nsCOMPtr<mozIStorageService> dbSrv;
    dbSrv = do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> tmpDir;
    rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tmpDir));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = tmpDir->AppendNative(TEST_DB);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCAutoString tmpPath;
    rv = tmpDir->GetNativePath(tmpPath);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILocalFile> f;
    rv = NS_NewNativeLocalFile (tmpPath, PR_FALSE, getter_AddRefs(f));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<mozIStorageConnection> dbConn;
    rv = dbSrv->OpenDatabase(f, getter_AddRefs(dbConn));
    NS_ENSURE_SUCCESS(rv, rv);

    tf = new TestFunc();
    rv = dbConn->CreateFunction(NS_LITERAL_CSTRING("x_test"), -1, tf);
    NS_ENSURE_SUCCESS(rv, rv);

    // Must be error: function exists
    rv = dbConn->CreateFunction(NS_LITERAL_CSTRING("x_test"), -1, tf);
    NS_ENSURE_TRUE((rv == NS_ERROR_FAILURE), NS_ERROR_FAILURE);

    rv = dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("SELECT x_test(1)"));
    NS_ENSURE_SUCCESS(rv, rv);

    taf = new TestAggregateFunc();
    rv = dbConn->CreateAggregateFunction(NS_LITERAL_CSTRING("x_aggr"), -1, taf);
    NS_ENSURE_SUCCESS(rv, rv);

    // Must be error: function exists with this name
    rv = dbConn->CreateAggregateFunction(NS_LITERAL_CSTRING("x_aggr"), -1, taf);
    NS_ENSURE_TRUE((rv == NS_ERROR_FAILURE), NS_ERROR_FAILURE);

    // Must be error: function exists with this implementation
    rv = dbConn->CreateAggregateFunction(NS_LITERAL_CSTRING("x_aggr2"), -1, taf);
    NS_ENSURE_TRUE((rv == NS_ERROR_FAILURE), NS_ERROR_FAILURE);

    rv = dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("DROP TABLE foo"));
    // TEST_CHECK_ERROR(rv);

    rv = dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("CREATE TABLE foo (i INTEGER)"));
    TEST_CHECK_ERROR(rv);

    nsCOMPtr<mozIStorageStatement> dbFooInsertStatement;
    rv = dbConn->CreateStatement (NS_LITERAL_CSTRING("INSERT INTO foo VALUES ( ?1 )"), getter_AddRefs(dbFooInsertStatement));
    TEST_CHECK_ERROR(rv);

    nsCOMPtr<mozIStorageStatement> dbFooSelectStatement;
    rv = dbConn->CreateStatement (NS_LITERAL_CSTRING("SELECT i FROM foo"), getter_AddRefs(dbFooSelectStatement));
    TEST_CHECK_ERROR(rv);

    nsCOMPtr<mozIStorageStatement> dbAggrSelectStatement;
    rv = dbConn->CreateStatement (NS_LITERAL_CSTRING("SELECT x_aggr(i) FROM foo"), getter_AddRefs(dbAggrSelectStatement));
    TEST_CHECK_ERROR(rv);

    for (int i = 0; i < 10; i++) {
        rv = dbFooInsertStatement->BindInt32Parameter (0, i);
        TEST_CHECK_ERROR(rv);

        rv = dbFooInsertStatement->Execute ();
        TEST_CHECK_ERROR(rv);
    }

    fprintf (stderr, "10 values written to foo...\n");

    nsCOMPtr<mozIStorageValueArray> dbRow = do_QueryInterface(dbFooSelectStatement);
    PRBool hasMore = PR_FALSE;

    while ((dbFooSelectStatement->ExecuteStep(&hasMore) == NS_OK) && hasMore)
    {
        PRUint32 len;

        dbRow->GetNumEntries (&len);
        fprintf (stderr, "Row[length %d]: %d '%s'\n", len, dbRow->AsInt32(0), dbRow->AsSharedUTF8String(0, 0));
    }

    TEST_CHECK_ERROR(rv);
    fprintf (stderr, "Done. %d 0x%08x %p\n", rv, rv, dbRow.get());

    dbRow = do_QueryInterface(dbAggrSelectStatement);
    hasMore = PR_FALSE;

    while ((dbAggrSelectStatement->ExecuteStep(&hasMore) == NS_OK) && hasMore)
    {
        PRUint32 len;
        dbRow->GetNumEntries (&len);
        fprintf (stderr, "Row[length %d]: %d '%s'\n", len, dbRow->AsInt32(0), dbRow->AsSharedUTF8String(0, 0));
    }

    TEST_CHECK_ERROR(rv);
    fprintf (stderr, "Done. %d 0x%08x %p\n", rv, rv, dbRow.get());

    TestProgressHandler *tph = new TestProgressHandler();
    nsCOMPtr<mozIStorageProgressHandler> oldHandler;
    rv = dbConn->SetProgressHandler(1, tph, getter_AddRefs(oldHandler));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<mozIStorageStatement> dbSortSelectStatement;
    rv = dbConn->CreateStatement (NS_LITERAL_CSTRING("SELECT i FROM foo ORDER BY i DESC"), getter_AddRefs(dbSortSelectStatement));
    TEST_CHECK_ERROR(rv);

    rv = dbSortSelectStatement->ExecuteStep(&hasMore);
    TEST_CHECK_ERROR(rv);
    fprintf (stderr, "Statement execution takes %d ticks\n", tph->getTicks());

    return 0;
}
