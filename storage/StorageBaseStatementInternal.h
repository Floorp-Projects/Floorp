/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_storage_StorageBaseStatementInternal_h_
#define mozilla_storage_StorageBaseStatementInternal_h_

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "mozStorageHelper.h"

struct sqlite3;
struct sqlite3_stmt;
class mozIStorageBindingParamsArray;
class mozIStorageBindingParams;
class mozIStorageStatementCallback;
class mozIStoragePendingStatement;

namespace mozilla {
namespace storage {

#define STORAGEBASESTATEMENTINTERNAL_IID \
  {0xd18856c9, 0xbf07, 0x4ae2, {0x94, 0x5b, 0x1a, 0xdd, 0x49, 0x19, 0x55, 0x2a}}

class Connection;
class StatementData;

class AsyncStatementFinalizer;

/**
 * Implementation-only interface and shared logix mix-in corresponding to
 * mozIStorageBaseStatement.  Both Statement and AsyncStatement inherit from
 * this. The interface aspect makes them look the same to implementation innards
 * that aren't publicly accessible.  The mix-in avoids code duplication in
 * common implementations of mozIStorageBaseStatement, albeit with some minor
 * performance/space overhead because we have to use defines to officially
 * implement the methods on Statement/AsyncStatement (and proxy to this base
 * class.)
 */
class StorageBaseStatementInternal : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(STORAGEBASESTATEMENTINTERNAL_IID)

  /**
   * @return the connection that this statement belongs to.
   */
  Connection *getOwner()
  {
    return mDBConnection;
  }

  /**
   * Return the asynchronous statement, creating it if required.
   *
   * This is for use by the asynchronous execution code for StatementData
   * created by AsyncStatements.  Statement internally uses this method to
   * prepopulate StatementData with the sqlite3_stmt.
   *
   * @param[out] stmt
   *             The sqlite3_stmt for asynchronous use.
   * @return The SQLite result code for creating the statement if created,
   *         SQLITE_OK if creation was not required.
   */
  virtual int getAsyncStatement(sqlite3_stmt **_stmt) = 0;

  /**
   * Obtains the StatementData needed for asynchronous execution.
   *
   * This is for use by Connection to retrieve StatementData from statements
   * when executeAsync is invoked.
   *
   * @param[out] _data
   *             A reference to a StatementData object that will be populated
   *             upon successful execution of this method.
   * @return NS_OK if we were able to assemble the data, failure otherwise.
   */
  virtual nsresult getAsynchronousStatementData(StatementData &_data) = 0;

  /**
   * Construct a new BindingParams to be owned by the provided binding params
   * array.  This method exists so that BindingParamsArray does not need
   * factory logic to determine what type of BindingParams to instantiate.
   *
   * @param aOwner
   *        The binding params array to own the newly created binding params.
   * @return The new mozIStorageBindingParams instance appropriate to the
   *         underlying statement type.
   */
  virtual already_AddRefed<mozIStorageBindingParams> newBindingParams(
    mozIStorageBindingParamsArray *aOwner
  ) = 0;

protected: // mix-in bits are protected
  StorageBaseStatementInternal();

  RefPtr<Connection> mDBConnection;
  sqlite3 *mNativeConnection;

  /**
   * Our asynchronous statement.
   *
   * For Statement this is populated by the first invocation to
   * getAsyncStatement.
   *
   * For AsyncStatement, this is null at creation time and initialized by the
   * async thread when it calls getAsyncStatement the first time the statement
   * is executed.  (Or in the event of badly formed SQL, every time.)
   */
  sqlite3_stmt *mAsyncStatement;

  /**
   * Initiate asynchronous finalization by dispatching an event to the
   * asynchronous thread to finalize mAsyncStatement.  This acquires a reference
   * to this statement and proxies it back to the connection's owning thread
   * for release purposes.
   *
   * In the event the asynchronous thread is already gone or we otherwise fail
   * to dispatch an event to it we failover to invoking internalAsyncFinalize
   * directly.  (That's what the asynchronous finalizer would have called.)
   *
   * @note You must not call this method from your destructor because its
   *       operation assumes we are still alive.  Call internalAsyncFinalize
   *       directly in that case.
   */
  void asyncFinalize();

  /**
   * Cleanup the async sqlite3_stmt stored in mAsyncStatement if it exists by
   * attempting to dispatch to the asynchronous thread if available, finalizing
   * on this thread if it is not.
   *
   * @note Call this from your destructor, call asyncFinalize otherwise.
   */
  void destructorAsyncFinalize();

  NS_IMETHOD NewBindingParamsArray(mozIStorageBindingParamsArray **_array);
  NS_IMETHOD ExecuteAsync(mozIStorageStatementCallback *aCallback,
                          mozIStoragePendingStatement **_stmt);
  NS_IMETHOD EscapeStringForLIKE(const nsAString &aValue,
                                 char16_t aEscapeChar,
                                 nsAString &_escapedString);

  // Needs access to internalAsyncFinalize
  friend class AsyncStatementFinalizer;
};

NS_DEFINE_STATIC_IID_ACCESSOR(StorageBaseStatementInternal,
                              STORAGEBASESTATEMENTINTERNAL_IID)

#define NS_DECL_STORAGEBASESTATEMENTINTERNAL \
  virtual Connection *getOwner(); \
  virtual int getAsyncStatement(sqlite3_stmt **_stmt) override; \
  virtual nsresult getAsynchronousStatementData(StatementData &_data) override; \
  virtual already_AddRefed<mozIStorageBindingParams> newBindingParams( \
    mozIStorageBindingParamsArray *aOwner) override;

/**
 * Helper macro to implement the proxying implementations.  Because we are
 * implementing methods that are part of mozIStorageBaseStatement and the
 * implementation classes already use NS_DECL_MOZISTORAGEBASESTATEMENT we don't
 * need to provide declaration support.
 */
#define MIX_IMPL(_class, _optionalGuard, _method, _declArgs, _invokeArgs) \
  NS_IMETHODIMP _class::_method _declArgs                                 \
  {                                                                       \
    _optionalGuard                                                        \
    return StorageBaseStatementInternal::_method _invokeArgs;             \
  }


/**
 * Define proxying implementation for the given _class.  If a state invariant
 * needs to be checked and an early return possibly performed, pass the clause
 * to use as _optionalGuard.
 */
#define MIXIN_IMPL_STORAGEBASESTATEMENTINTERNAL(_class, _optionalGuard) \
  MIX_IMPL(_class, _optionalGuard,                                      \
           NewBindingParamsArray,                                       \
           (mozIStorageBindingParamsArray **_array),                    \
           (_array))                                                    \
  MIX_IMPL(_class, _optionalGuard,                                      \
           ExecuteAsync,                                                \
           (mozIStorageStatementCallback *aCallback,                    \
            mozIStoragePendingStatement **_stmt),                       \
           (aCallback, _stmt))                                          \
  MIX_IMPL(_class, _optionalGuard,                                      \
           EscapeStringForLIKE,                                         \
           (const nsAString &aValue, char16_t aEscapeChar,              \
            nsAString &_escapedString),                                 \
           (aValue, aEscapeChar, _escapedString))

/**
 * Name-building helper for BIND_GEN_IMPL.
 */
#define BIND_NAME_CONCAT(_nameBit, _concatBit) \
  Bind##_nameBit##_concatBit

/**
 * We have type-specific convenience methods for C++ implementations in
 * 3 different forms; 2 by index, 1 by name.  The following macro allows
 * us to avoid having to define repetitive things by hand.
 *
 * Because of limitations of macros and our desire to avoid requiring special
 * permutations for the null and blob cases (whose argument count varies),
 * we require that the argument declarations and corresponding invocation
 * usages are passed in.
 *
 * @param _class
 *        The class name.
 * @param _guard
 *        The guard clause to inject.
 * @param _declName
 *        The argument list (with parens) for the ByName variants.
 * @param _declIndex
 *        The argument list (with parens) for the index variants.
 * @param _invArgs
 *        The invocation argumment list.
 */
#define BIND_GEN_IMPL(_class, _guard, _name, _declName, _declIndex, _invArgs) \
  NS_IMETHODIMP _class::BIND_NAME_CONCAT(_name, ByName) _declName             \
  {                                                                           \
    _guard                                                                    \
    mozIStorageBindingParams *params = getParams();                           \
    NS_ENSURE_TRUE(params, NS_ERROR_OUT_OF_MEMORY);                           \
    return params->BIND_NAME_CONCAT(_name, ByName) _invArgs;                  \
  }                                                                           \
  NS_IMETHODIMP _class::BIND_NAME_CONCAT(_name, ByIndex) _declIndex           \
  {                                                                           \
    _guard                                                                    \
    mozIStorageBindingParams *params = getParams();                           \
    NS_ENSURE_TRUE(params, NS_ERROR_OUT_OF_MEMORY);                           \
    return params->BIND_NAME_CONCAT(_name, ByIndex) _invArgs;                 \
  }                                                                           \
  NS_IMETHODIMP _class::BIND_NAME_CONCAT(_name, Parameter) _declIndex         \
  {                                                                           \
    WARN_DEPRECATED();                                                        \
    _guard                                                                    \
    mozIStorageBindingParams *params = getParams();                           \
    NS_ENSURE_TRUE(params, NS_ERROR_OUT_OF_MEMORY);                           \
    return params->BIND_NAME_CONCAT(_name, ByIndex) _invArgs;                 \
  }

/**
 * Implement BindByName/BindByIndex for the given class.
 *
 * @param _class The class name.
 * @param _optionalGuard The guard clause to inject.
 */
#define BIND_BASE_IMPLS(_class, _optionalGuard)             \
  NS_IMETHODIMP _class::BindByName(const nsACString &aName, \
                                   nsIVariant *aValue)      \
  {                                                         \
    _optionalGuard                                          \
    mozIStorageBindingParams *params = getParams();         \
    NS_ENSURE_TRUE(params, NS_ERROR_OUT_OF_MEMORY);         \
    return params->BindByName(aName, aValue);               \
  }                                                         \
  NS_IMETHODIMP _class::BindByIndex(uint32_t aIndex,        \
                                    nsIVariant *aValue)     \
  {                                                         \
    _optionalGuard                                          \
    mozIStorageBindingParams *params = getParams();         \
    NS_ENSURE_TRUE(params, NS_ERROR_OUT_OF_MEMORY);         \
    return params->BindByIndex(aIndex, aValue);             \
  }

/**
 * Define the various Bind*Parameter, Bind*ByIndex, Bind*ByName stubs that just
 * end up proxying to the params object.
 */
#define BOILERPLATE_BIND_PROXIES(_class, _optionalGuard) \
  BIND_BASE_IMPLS(_class, _optionalGuard)                \
  BIND_GEN_IMPL(_class, _optionalGuard,                  \
                UTF8String,                              \
                (const nsACString &aWhere,               \
                 const nsACString &aValue),              \
                (uint32_t aWhere,                        \
                 const nsACString &aValue),              \
                (aWhere, aValue))                        \
  BIND_GEN_IMPL(_class, _optionalGuard,                  \
                String,                                  \
                (const nsACString &aWhere,               \
                 const nsAString  &aValue),              \
                (uint32_t aWhere,                        \
                 const nsAString  &aValue),              \
                (aWhere, aValue))                        \
  BIND_GEN_IMPL(_class, _optionalGuard,                  \
                Double,                                  \
                (const nsACString &aWhere,               \
                 double aValue),                         \
                (uint32_t aWhere,                        \
                 double aValue),                         \
                (aWhere, aValue))                        \
  BIND_GEN_IMPL(_class, _optionalGuard,                  \
                Int32,                                   \
                (const nsACString &aWhere,               \
                 int32_t aValue),                        \
                (uint32_t aWhere,                        \
                 int32_t aValue),                        \
                (aWhere, aValue))                        \
  BIND_GEN_IMPL(_class, _optionalGuard,                  \
                Int64,                                   \
                (const nsACString &aWhere,               \
                 int64_t aValue),                        \
                (uint32_t aWhere,                        \
                 int64_t aValue),                        \
                (aWhere, aValue))                        \
  BIND_GEN_IMPL(_class, _optionalGuard,                  \
                Null,                                    \
                (const nsACString &aWhere),              \
                (uint32_t aWhere),                       \
                (aWhere))                                \
  BIND_GEN_IMPL(_class, _optionalGuard,                  \
                Blob,                                    \
                (const nsACString &aWhere,               \
                 const uint8_t *aValue,                  \
                 uint32_t aValueSize),                   \
                (uint32_t aWhere,                        \
                 const uint8_t *aValue,                  \
                 uint32_t aValueSize),                   \
                (aWhere, aValue, aValueSize))            \
  BIND_GEN_IMPL(_class, _optionalGuard,                  \
                StringAsBlob,                            \
                (const nsACString &aWhere,               \
                 const nsAString& aValue),               \
                (uint32_t aWhere,                        \
                 const nsAString& aValue),               \
                (aWhere, aValue))                        \
  BIND_GEN_IMPL(_class, _optionalGuard,                  \
                UTF8StringAsBlob,                        \
                (const nsACString &aWhere,               \
                 const nsACString& aValue),              \
                (uint32_t aWhere,                        \
                 const nsACString& aValue),              \
                (aWhere, aValue))                        \
  BIND_GEN_IMPL(_class, _optionalGuard,                  \
                AdoptedBlob,                             \
                (const nsACString &aWhere,               \
                 uint8_t *aValue,                        \
                 uint32_t aValueSize),                   \
                (uint32_t aWhere,                        \
                 uint8_t *aValue,                        \
                 uint32_t aValueSize),                   \
                (aWhere, aValue, aValueSize))



} // namespace storage
} // namespace mozilla

#endif // mozilla_storage_StorageBaseStatementInternal_h_
