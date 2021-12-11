/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZSTORAGESERVICE_H
#define MOZSTORAGESERVICE_H

#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "nsTArray.h"
#include "mozilla/Mutex.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/intl/Collator.h"

#include "mozIStorageService.h"

class nsIMemoryReporter;
struct sqlite3_vfs;
namespace mozilla::intl {
class Collator;
}

namespace mozilla {
namespace storage {

class Connection;
class Service : public mozIStorageService,
                public nsIObserver,
                public nsIMemoryReporter {
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
   * @param  aSensitivity
   *         The sorting sensitivity.
   * @return aStr1 - aStr2.  That is, if aStr1 < aStr2, returns a negative
   *         number.  If aStr1 > aStr2, returns a positive number.  If
   *         aStr1 == aStr2, returns 0.
   */
  int localeCompareStrings(const nsAString& aStr1, const nsAString& aStr2,
                           mozilla::intl::Collator::Sensitivity aSensitivity);

  static already_AddRefed<Service> getSingleton();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZISTORAGESERVICE
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIMEMORYREPORTER

  /**
   * Returns a boolean value indicating whether or not the given page size is
   * valid (currently understood as a power of 2 between 512 and 65536).
   */
  static bool pageSizeIsValid(int32_t aPageSize) {
    return aPageSize == 512 || aPageSize == 1024 || aPageSize == 2048 ||
           aPageSize == 4096 || aPageSize == 8192 || aPageSize == 16384 ||
           aPageSize == 32768 || aPageSize == 65536;
  }

  static const int32_t kDefaultPageSize = 32768;

  /**
   * Registers the connection with the storage service.  Connections are
   * registered so they can be iterated over.
   *
   * @pre mRegistrationMutex is not held
   *
   * @param  aConnection
   *         The connection to register.
   */
  void registerConnection(Connection* aConnection);

  /**
   * Unregisters the connection with the storage service.
   *
   * @pre mRegistrationMutex is not held
   *
   * @param  aConnection
   *         The connection to unregister.
   */
  void unregisterConnection(Connection* aConnection);

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
  void getConnections(nsTArray<RefPtr<Connection> >& aConnections);

 private:
  Service();
  virtual ~Service();

  /**
   * Used for 1) locking around calls when initializing connections so that we
   * can ensure that the state of sqlite3_enable_shared_cache is sane and 2)
   * synchronizing access to mLocaleCollation.
   */
  Mutex mMutex;

  struct AutoVFSRegistration {
    int Init(UniquePtr<sqlite3_vfs> aVFS);
    ~AutoVFSRegistration();

   private:
    UniquePtr<sqlite3_vfs> mVFS;
  };

  // The order of these members should match the order of Init calls in
  // initialize(), to ensure that the unregistration takes place in the reverse
  // order.
  AutoVFSRegistration mTelemetrySqliteVFS;
  AutoVFSRegistration mTelemetryExclSqliteVFS;
  AutoVFSRegistration mObfuscatingSqliteVFS;

  /**
   * Protects mConnections.
   */
  Mutex mRegistrationMutex;

  /**
   * The list of connections we have created.  Modifications to it are
   * protected by |mRegistrationMutex|.
   */
  nsTArray<RefPtr<Connection> > mConnections;

  /**
   * Frees as much heap memory as possible from all of the known open
   * connections.
   */
  void minimizeMemory();

  /**
   * Lazily creates and returns a collator created from the application's
   * locale that all statements of all Connections of this Service may use.
   * Since the collator's lifetime is that of the Service and no statement may
   * execute outside the lifetime of the Service, this method returns a raw
   * pointer.
   */
  mozilla::intl::Collator* getCollator();

  /**
   * Lazily created collator that all statements of all Connections of this
   * Service may use.  The collator is created from the application's locale.
   *
   * @note The collator is not thread-safe since the options can be changed
   * between calls. Access should be synchronized.
   */
  mozilla::UniquePtr<mozilla::intl::Collator> mCollator = nullptr;

  nsCOMPtr<nsIFile> mProfileStorageFile;

  nsCOMPtr<nsIMemoryReporter> mStorageSQLiteReporter;

  static Service* gService;

  mozilla::intl::Collator::Sensitivity mLastSensitivity;
};

}  // namespace storage
}  // namespace mozilla

#endif /* MOZSTORAGESERVICE_H */
