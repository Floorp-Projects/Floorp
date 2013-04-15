/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZSTORAGESERVICE_H
#define MOZSTORAGESERVICE_H

#include "nsCOMPtr.h"
#include "nsICollation.h"
#include "nsIFile.h"
#include "nsIObserver.h"
#include "nsTArray.h"
#include "mozilla/Mutex.h"

#include "mozIStorageService.h"

class nsIMemoryReporter;
class nsIMemoryMultiReporter;
class nsIXPConnect;
struct sqlite3_vfs;

namespace mozilla {
namespace storage {

class Connection;
class Service : public mozIStorageService
              , public nsIObserver
{
public:
  /**
   * Initializes the service.  This must be called before any other function!
   */
  nsresult initialize();

  /**
   * Compares two strings using the Service's locale-aware collation.
   *
   * @param  aStr1
   *         The string to be compared against aStr2.
   * @param  aStr2
   *         The string to be compared against aStr1.
   * @param  aComparisonStrength
   *         The sorting strength, one of the nsICollation constants.
   * @return aStr1 - aStr2.  That is, if aStr1 < aStr2, returns a negative
   *         number.  If aStr1 > aStr2, returns a positive number.  If
   *         aStr1 == aStr2, returns 0.
   */
  int localeCompareStrings(const nsAString &aStr1,
                           const nsAString &aStr2,
                           int32_t aComparisonStrength);

  static Service *getSingleton();

  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGESERVICE
  NS_DECL_NSIOBSERVER

  /**
   * Obtains an already AddRefed pointer to XPConnect.  This is used by
   * language helpers.
   */
  static already_AddRefed<nsIXPConnect> getXPConnect();

  /**
   * Obtains the cached data for the toolkit.storage.synchronous preference.
   */
  static int32_t getSynchronousPref();

  /**
   * Obtains the default page size for this platform. The default value is
   * specified in the SQLite makefile (SQLITE_DEFAULT_PAGE_SIZE) but it may be
   * overriden with the PREF_TS_PAGESIZE hidden preference.
   */
  static int32_t getDefaultPageSize()
  {
    return sDefaultPageSize;
  }

  /**
   * Returns a boolean value indicating whether or not the given page size is
   * valid (currently understood as a power of 2 between 512 and 65536).
   */
  static bool pageSizeIsValid(int32_t aPageSize)
  {
    return aPageSize == 512 || aPageSize == 1024 || aPageSize == 2048 ||
           aPageSize == 4096 || aPageSize == 8192 || aPageSize == 16384 ||
           aPageSize == 32768 || aPageSize == 65536;
  }

  /**
   * Registers the connection with the storage service.  Connections are
   * registered so they can be iterated over.
   *
   * @pre mRegistrationMutex is not held
   *
   * @param  aConnection
   *         The connection to register.
   */
  void registerConnection(Connection *aConnection);

  /**
   * Unregisters the connection with the storage service.
   *
   * @pre mRegistrationMutex is not held
   *
   * @param  aConnection
   *         The connection to unregister.
   */
  void unregisterConnection(Connection *aConnection);

  /**
   * Gets the list of open connections.  Note that you must test each
   * connection with mozIStorageConnection::connectionReady before doing
   * anything with it, and skip it if it's not ready.
   *
   * @pre mRegistrationMutex is not held
   *
   * @param  aConnections
   *         An inout param;  it is cleared and the connections are appended to
   *         it.
   * @return The open connections.
   */
  void getConnections(nsTArray<nsRefPtr<Connection> >& aConnections);

private:
  Service();
  virtual ~Service();

  /**
   * Used for 1) locking around calls when initializing connections so that we
   * can ensure that the state of sqlite3_enable_shared_cache is sane and 2)
   * synchronizing access to mLocaleCollation.
   */
  Mutex mMutex;
  
  sqlite3_vfs *mSqliteVFS;

  /**
   * Protects mConnections.
   */
  Mutex mRegistrationMutex;

  /**
   * The list of connections we have created.  Modifications to it are
   * protected by |mRegistrationMutex|.
   */
  nsTArray<nsRefPtr<Connection> > mConnections;

  /**
   * Shuts down the storage service, freeing all of the acquired resources.
   */
  void shutdown();

  /**
   * Lazily creates and returns a collation created from the application's
   * locale that all statements of all Connections of this Service may use.
   * Since the collation's lifetime is that of the Service and no statement may
   * execute outside the lifetime of the Service, this method returns a raw
   * pointer.
   */
  nsICollation *getLocaleCollation();

  /**
   * Lazily created collation that all statements of all Connections of this
   * Service may use.  The collation is created from the application's locale.
   *
   * @note Collation implementations are platform-dependent and in general not
   *       thread-safe.  Access to this collation should be synchronized.
   */
  nsCOMPtr<nsICollation> mLocaleCollation;

  nsCOMPtr<nsIFile> mProfileStorageFile;

  nsCOMPtr<nsIMemoryReporter> mStorageSQLiteReporter;
  nsCOMPtr<nsIMemoryMultiReporter> mStorageSQLiteMultiReporter;

  static Service *gService;

  static nsIXPConnect *sXPConnect;

  static int32_t sSynchronousPref;
  static int32_t sDefaultPageSize;
};

} // namespace storage
} // namespace mozilla

#endif /* MOZSTORAGESERVICE_H */
