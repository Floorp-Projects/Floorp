/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits.h>
#include <stdio.h>

#include "nsError.h"
#include "nsMemory.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsIProgrammingLanguage.h"
#include "Variant.h"

#include "mozIStorageError.h"

#include "mozStorageBindingParams.h"
#include "mozStorageConnection.h"
#include "mozStorageAsyncStatementJSHelper.h"
#include "mozStorageAsyncStatementParams.h"
#include "mozStoragePrivateHelpers.h"
#include "mozStorageStatementRow.h"
#include "mozStorageStatement.h"
#include "nsDOMClassInfo.h"

#include "prlog.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo *gStorageLog;
#endif

namespace mozilla {
namespace storage {

////////////////////////////////////////////////////////////////////////////////
//// nsIClassInfo

NS_IMPL_CI_INTERFACE_GETTER(AsyncStatement,
                            mozIStorageAsyncStatement,
                            mozIStorageBaseStatement,
                            mozIStorageBindingParams,
                            mozilla::storage::StorageBaseStatementInternal)

class AsyncStatementClassInfo : public nsIClassInfo
{
public:
  MOZ_CONSTEXPR AsyncStatementClassInfo() {}

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHODIMP
  GetInterfaces(uint32_t *_count, nsIID ***_array) MOZ_OVERRIDE
  {
    return NS_CI_INTERFACE_GETTER_NAME(AsyncStatement)(_count, _array);
  }

  NS_IMETHODIMP
  GetHelperForLanguage(uint32_t aLanguage, nsISupports **_helper) MOZ_OVERRIDE
  {
    if (aLanguage == nsIProgrammingLanguage::JAVASCRIPT) {
      static AsyncStatementJSHelper sJSHelper;
      *_helper = &sJSHelper;
      return NS_OK;
    }

    *_helper = nullptr;
    return NS_OK;
  }

  NS_IMETHODIMP
  GetContractID(char **_contractID) MOZ_OVERRIDE
  {
    *_contractID = nullptr;
    return NS_OK;
  }

  NS_IMETHODIMP
  GetClassDescription(char **_desc) MOZ_OVERRIDE
  {
    *_desc = nullptr;
    return NS_OK;
  }

  NS_IMETHODIMP
  GetClassID(nsCID **_id) MOZ_OVERRIDE
  {
    *_id = nullptr;
    return NS_OK;
  }

  NS_IMETHODIMP
  GetImplementationLanguage(uint32_t *_language) MOZ_OVERRIDE
  {
    *_language = nsIProgrammingLanguage::CPLUSPLUS;
    return NS_OK;
  }

  NS_IMETHODIMP
  GetFlags(uint32_t *_flags) MOZ_OVERRIDE
  {
    *_flags = 0;
    return NS_OK;
  }

  NS_IMETHODIMP
  GetClassIDNoAlloc(nsCID *_cid) MOZ_OVERRIDE
  {
    return NS_ERROR_NOT_AVAILABLE;
  }
};

NS_IMETHODIMP_(MozExternalRefCountType) AsyncStatementClassInfo::AddRef() { return 2; }
NS_IMETHODIMP_(MozExternalRefCountType) AsyncStatementClassInfo::Release() { return 1; }
NS_IMPL_QUERY_INTERFACE(AsyncStatementClassInfo, nsIClassInfo)

static AsyncStatementClassInfo sAsyncStatementClassInfo;

////////////////////////////////////////////////////////////////////////////////
//// AsyncStatement

AsyncStatement::AsyncStatement()
: StorageBaseStatementInternal()
, mFinalized(false)
{
}

nsresult
AsyncStatement::initialize(Connection *aDBConnection,
                           sqlite3 *aNativeConnection,
                           const nsACString &aSQLStatement)
{
  MOZ_ASSERT(aDBConnection, "No database connection given!");
  MOZ_ASSERT(!aDBConnection->isClosed(), "Database connection should be valid");
  MOZ_ASSERT(aNativeConnection, "No native connection given!");

  mDBConnection = aDBConnection;
  mNativeConnection = aNativeConnection;
  mSQLString = aSQLStatement;

  PR_LOG(gStorageLog, PR_LOG_NOTICE, ("Inited async statement '%s' (0x%p)",
                                      mSQLString.get()));

#ifdef DEBUG
  // We want to try and test for LIKE and that consumers are using
  // escapeStringForLIKE instead of just trusting user input.  The idea to
  // check to see if they are binding a parameter after like instead of just
  // using a string.  We only do this in debug builds because it's expensive!
  const nsCaseInsensitiveCStringComparator c;
  nsACString::const_iterator start, end, e;
  aSQLStatement.BeginReading(start);
  aSQLStatement.EndReading(end);
  e = end;
  while (::FindInReadable(NS_LITERAL_CSTRING(" LIKE"), start, e, c)) {
    // We have a LIKE in here, so we perform our tests
    // FindInReadable moves the iterator, so we have to get a new one for
    // each test we perform.
    nsACString::const_iterator s1, s2, s3;
    s1 = s2 = s3 = start;

    if (!(::FindInReadable(NS_LITERAL_CSTRING(" LIKE ?"), s1, end, c) ||
          ::FindInReadable(NS_LITERAL_CSTRING(" LIKE :"), s2, end, c) ||
          ::FindInReadable(NS_LITERAL_CSTRING(" LIKE @"), s3, end, c))) {
      // At this point, we didn't find a LIKE statement followed by ?, :,
      // or @, all of which are valid characters for binding a parameter.
      // We will warn the consumer that they may not be safely using LIKE.
      NS_WARNING("Unsafe use of LIKE detected!  Please ensure that you "
                 "are using mozIStorageAsyncStatement::escapeStringForLIKE "
                 "and that you are binding that result to the statement "
                 "to prevent SQL injection attacks.");
    }

    // resetting start and e
    start = e;
    e = end;
  }
#endif

  return NS_OK;
}

mozIStorageBindingParams *
AsyncStatement::getParams()
{
  nsresult rv;

  // If we do not have an array object yet, make it.
  if (!mParamsArray) {
    nsCOMPtr<mozIStorageBindingParamsArray> array;
    rv = NewBindingParamsArray(getter_AddRefs(array));
    NS_ENSURE_SUCCESS(rv, nullptr);

    mParamsArray = static_cast<BindingParamsArray *>(array.get());
  }

  // If there isn't already any rows added, we'll have to add one to use.
  if (mParamsArray->length() == 0) {
    nsRefPtr<AsyncBindingParams> params(new AsyncBindingParams(mParamsArray));
    NS_ENSURE_TRUE(params, nullptr);

    rv = mParamsArray->AddParams(params);
    NS_ENSURE_SUCCESS(rv, nullptr);

    // We have to unlock our params because AddParams locks them.  This is safe
    // because no reference to the params object was, or ever will be given out.
    params->unlock(nullptr);

    // We also want to lock our array at this point - we don't want anything to
    // be added to it.
    mParamsArray->lock();
  }

  return *mParamsArray->begin();
}

/**
 * If we are here then we know there are no pending async executions relying on
 * us (StatementData holds a reference to us; this also goes for our own
 * AsyncStatementFinalizer which proxies its release to the calling thread) and
 * so it is always safe to destroy our sqlite3_stmt if one exists.  We can be
 * destroyed on the caller thread by garbage-collection/reference counting or on
 * the async thread by the last execution of a statement that already lost its
 * main-thread refs.
 */
AsyncStatement::~AsyncStatement()
{
  destructorAsyncFinalize();
  cleanupJSHelpers();

  // If we are getting destroyed on the wrong thread, proxy the connection
  // release to the right thread.  I'm not sure why we do this.
  bool onCallingThread = false;
  (void)mDBConnection->threadOpenedOn->IsOnCurrentThread(&onCallingThread);
  if (!onCallingThread) {
    // NS_ProxyRelase only magic forgets for us if mDBConnection is an
    // nsCOMPtr.  Which it is not; it's an nsRefPtr.
    Connection *forgottenConn = nullptr;
    mDBConnection.swap(forgottenConn);
    (void)::NS_ProxyRelease(forgottenConn->threadOpenedOn,
                            static_cast<mozIStorageConnection *>(forgottenConn));
  }
}

void
AsyncStatement::cleanupJSHelpers()
{
  // We are considered dead at this point, so any wrappers for row or params
  // need to lose their reference to us.
  if (mStatementParamsHolder) {
    nsCOMPtr<nsIXPConnectWrappedNative> wrapper =
      do_QueryInterface(mStatementParamsHolder);
    nsCOMPtr<mozIStorageStatementParams> iParams =
      do_QueryWrappedNative(wrapper);
    AsyncStatementParams *params =
      static_cast<AsyncStatementParams *>(iParams.get());
    params->mStatement = nullptr;
    mStatementParamsHolder = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
//// nsISupports

NS_IMPL_ADDREF(AsyncStatement)
NS_IMPL_RELEASE(AsyncStatement)

NS_INTERFACE_MAP_BEGIN(AsyncStatement)
  NS_INTERFACE_MAP_ENTRY(mozIStorageAsyncStatement)
  NS_INTERFACE_MAP_ENTRY(mozIStorageBaseStatement)
  NS_INTERFACE_MAP_ENTRY(mozIStorageBindingParams)
  NS_INTERFACE_MAP_ENTRY(mozilla::storage::StorageBaseStatementInternal)
  if (aIID.Equals(NS_GET_IID(nsIClassInfo))) {
    foundInterface = static_cast<nsIClassInfo *>(&sAsyncStatementClassInfo);
  }
  else
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, mozIStorageAsyncStatement)
NS_INTERFACE_MAP_END


////////////////////////////////////////////////////////////////////////////////
//// StorageBaseStatementInternal

Connection *
AsyncStatement::getOwner()
{
  return mDBConnection;
}

int
AsyncStatement::getAsyncStatement(sqlite3_stmt **_stmt)
{
#ifdef DEBUG
  // Make sure we are never called on the connection's owning thread.
  bool onOpenedThread = false;
  (void)mDBConnection->threadOpenedOn->IsOnCurrentThread(&onOpenedThread);
  NS_ASSERTION(!onOpenedThread,
               "We should only be called on the async thread!");
#endif

  if (!mAsyncStatement) {
    int rc = mDBConnection->prepareStatement(mNativeConnection, mSQLString,
                                             &mAsyncStatement);
    if (rc != SQLITE_OK) {
      PR_LOG(gStorageLog, PR_LOG_ERROR,
             ("Sqlite statement prepare error: %d '%s'", rc,
              ::sqlite3_errmsg(mNativeConnection)));
      PR_LOG(gStorageLog, PR_LOG_ERROR,
             ("Statement was: '%s'", mSQLString.get()));
      *_stmt = nullptr;
      return rc;
    }
    PR_LOG(gStorageLog, PR_LOG_NOTICE, ("Initialized statement '%s' (0x%p)",
                                        mSQLString.get(),
                                        mAsyncStatement));
  }

  *_stmt = mAsyncStatement;
  return SQLITE_OK;
}

nsresult
AsyncStatement::getAsynchronousStatementData(StatementData &_data)
{
  if (mFinalized)
    return NS_ERROR_UNEXPECTED;

  // Pass null for the sqlite3_stmt; it will be requested on demand from the
  // async thread.
  _data = StatementData(nullptr, bindingParamsArray(), this);

  return NS_OK;
}

already_AddRefed<mozIStorageBindingParams>
AsyncStatement::newBindingParams(mozIStorageBindingParamsArray *aOwner)
{
  if (mFinalized)
    return nullptr;

  nsCOMPtr<mozIStorageBindingParams> params(new AsyncBindingParams(aOwner));
  return params.forget();
}


////////////////////////////////////////////////////////////////////////////////
//// mozIStorageAsyncStatement

// (nothing is specific to mozIStorageAsyncStatement)

////////////////////////////////////////////////////////////////////////////////
//// StorageBaseStatementInternal

// proxy to StorageBaseStatementInternal using its define helper.
MIXIN_IMPL_STORAGEBASESTATEMENTINTERNAL(
  AsyncStatement,
  if (mFinalized) return NS_ERROR_UNEXPECTED;)

NS_IMETHODIMP
AsyncStatement::Finalize()
{
  if (mFinalized)
    return NS_OK;

  mFinalized = true;

  PR_LOG(gStorageLog, PR_LOG_NOTICE, ("Finalizing statement '%s'",
                                      mSQLString.get()));

  asyncFinalize();
  cleanupJSHelpers();

  return NS_OK;
}

NS_IMETHODIMP
AsyncStatement::BindParameters(mozIStorageBindingParamsArray *aParameters)
{
  if (mFinalized)
    return NS_ERROR_UNEXPECTED;

  BindingParamsArray *array = static_cast<BindingParamsArray *>(aParameters);
  if (array->getOwner() != this)
    return NS_ERROR_UNEXPECTED;

  if (array->length() == 0)
    return NS_ERROR_UNEXPECTED;

  mParamsArray = array;
  mParamsArray->lock();

  return NS_OK;
}

NS_IMETHODIMP
AsyncStatement::GetState(int32_t *_state)
{
  if (mFinalized)
    *_state = MOZ_STORAGE_STATEMENT_INVALID;
  else
    *_state = MOZ_STORAGE_STATEMENT_READY;

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// mozIStorageBindingParams

BOILERPLATE_BIND_PROXIES(
  AsyncStatement, 
  if (mFinalized) return NS_ERROR_UNEXPECTED;
)

} // namespace storage
} // namespace mozilla
