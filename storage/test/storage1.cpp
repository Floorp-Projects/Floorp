
#include <stdlib.h>
#include <stdio.h>

#include "nsIComponentManager.h"
#include "nsISimpleEnumerator.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsILocalFile.h"

#include "mozIStorageService.h"
#include "mozIStorageConnection.h"
#include "mozIStorageValue.h"
#include "mozIStorageStatement.h"
#include "mozIStorageFunction.h"

#include "mozStorageCID.h"

static NS_DEFINE_CID(kmozStorageServiceCID, MOZ_STORAGE_SERVICE_CID);
static NS_DEFINE_CID(kmozStorageConnectionCID, MOZ_STORAGE_CONNECTION_CID);

#define TEST_CHECK_ERROR(rv) \
        do { if (NS_FAILED(rv)) {              \
            dbConn->GetLastError(&gerr); \
            dbConn->GetLastErrorString(gerrstr); \
            fprintf (stderr, "Error: %d 0x%08x %s\n", gerr, gerr, gerrstr.get()); \
            return 0; \
            } } while (0)

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
TestFunc::OnFunctionCall (mozIStorageValueArray *sva)
{
    fprintf (stderr, "* function call!\n");
    return NS_OK;
}

int
main (int argc, char **argv)
{
    nsresult rv;

    NS_InitXPCOM2(nsnull, nsnull, nsnull);

    nsCOMPtr<mozIStorageService> dbSrv;
    dbSrv = do_GetService(kmozStorageServiceCID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsILocalFile> f;
    rv = NS_NewNativeLocalFile (NS_LITERAL_CSTRING("foo.sdb"), PR_FALSE, getter_AddRefs(f));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<mozIStorageConnection> dbConn;
    rv = dbSrv->OpenDatabase(f, getter_AddRefs(dbConn));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = dbConn->CreateFunction("x_test", -1, new TestFunc());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = dbConn->ExecuteSimpleSQL(NS_LITERAL_CSTRING("SELECT x_test(1)"));
    NS_ENSURE_SUCCESS(rv, rv);

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

    rv = dbConn->CreateTrigger("foo_trig", mozIStorageConnection::TRIGGER_EVENT_INSERT,
                               "foo", "x_test", "1");
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
        
        dbRow->GetNumColumns (&len);
        fprintf (stderr, "Row[length %d]: %d '%s'\n", len, dbRow->AsInt32(0), dbRow->AsSharedCString(0, 0));
    }

    TEST_CHECK_ERROR(rv);
    fprintf (stderr, "Done. %d 0x%08x %p\n", rv, rv, dbRow.get());

#if 0
    // try an enumerator
    fprintf (stderr, "Trying via an enumerator\n");

    nsCOMPtr<nsISimpleEnumerator> dbEnum;

    (void) dbFooSelectStatement->Reset ();
    rv = dbFooSelectStatement->ExecuteEnumerator (getter_AddRefs(dbEnum));
    TEST_CHECK_ERROR(rv);

    PRBool b;
    while ((dbEnum->HasMoreElements(&b) == NS_OK) && b) {
        dbEnum->GetNext (getter_AddRefs (dbRow));
        PRUint32 len;
        dbRow->GetLength (&len);
        fprintf (stderr, "Row[length %d]: %d '%s'\n", len, dbRow->AsInt32(0), dbRow->AsSharedCString(0, 0));
    }
#endif
}
