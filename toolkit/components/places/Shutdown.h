/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_places_Shutdown_h_
#define mozilla_places_Shutdown_h_

#include "nsIAsyncShutdown.h"
#include "Database.h"
#include "nsProxyRelease.h"

namespace mozilla {
namespace places {

class Database;

/**
 * This is most of the code responsible for Places shutdown.
 *
 * PHASE 1 (Legacy clients shutdown)
 * The shutdown procedure begins when the Database singleton receives
 * profile-change-teardown (note that tests will instead notify nsNavHistory,
 * that forwards the notification to the Database instance).
 * Database::Observe first of all checks if initialization was completed
 * properly, to avoid race conditions, then it notifies "places-shutdown" to
 * legacy clients. Legacy clients are supposed to start and complete any
 * shutdown critical work in the same tick, since we won't wait for them.

 * PHASE 2 (Modern clients shutdown)
 * Modern clients should instead register as a blocker by passing a promise to
 * nsINavHistoryService::shutdownClient (for example see Sanitizer.jsm), so they
 * block Places shutdown until the promise is resolved.
 * When profile-change-teardown is observed by async shutdown, it calls
 * ClientsShutdownBlocker::BlockShutdown. This class is registered as a teardown
 * phase blocker in Database::Init (see Database::mClientsShutdown).
 * ClientsShutdownBlocker::BlockShudown waits for all the clients registered
 * through nsINavHistoryService::shutdownClient. When all the clients are done,
 * its `Done` method is invoked, and it stops blocking the shutdown phase, so
 * that it can continue.
 *
 * PHASE 3 (Connection shutdown)
 * ConnectionBlocker is registered as a profile-before-change blocker in
 * Database::Init (see Database::mConnectionShutdown).
 * When profile-before-change is observer by async shutdown, it calls
 * ConnectionShutdownBlocker::BlockShutdown.
 * Then the control is passed to Database::Shutdown, that executes some sanity
 * checks, clears cached statements and proceeds with asyncClose.
 * Once the connection is definitely closed, Database will call back
 * ConnectionBlocker::Complete. At this point a final
 * places-connection-closed notification is sent, for testing purposes.
 */

/**
 * A base AsyncShutdown blocker in charge of shutting down Places.
 */
class PlacesShutdownBlocker : public nsIAsyncShutdownBlocker
                            , public nsIAsyncShutdownCompletionCallback
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIASYNCSHUTDOWNBLOCKER
  NS_DECL_NSIASYNCSHUTDOWNCOMPLETIONCALLBACK

  explicit PlacesShutdownBlocker(const nsString& aName);

  already_AddRefed<nsIAsyncShutdownClient> GetClient();

  /**
   * `true` if we have not started shutdown, i.e.  if
   * `BlockShutdown()` hasn't been called yet, false otherwise.
   */
  static bool IsStarted() {
    return sIsStarted;
  }

  // The current state, used internally and for forensics/debugging purposes.
  // Not all the states make sense for all the derived classes.
  enum States {
    NOT_STARTED,
    // Execution of `BlockShutdown` in progress.
    RECEIVED_BLOCK_SHUTDOWN,

    // Values specific to ClientsShutdownBlocker
    // a. Set while we are waiting for clients to do their job and unblock us.
    CALLED_WAIT_CLIENTS,
    // b. Set when all the clients are done.
    RECEIVED_DONE,

    // Values specific to ConnectionShutdownBlocker
    // a. Set after we notified observers that Places is closing the connection.
    NOTIFIED_OBSERVERS_PLACES_WILL_CLOSE_CONNECTION,
    // b. Set after we pass control to Database::Shutdown, and wait for it to
    // close the connection and call our `Complete` method when done.
    CALLED_STORAGESHUTDOWN,
    // c. Set when Database has closed the connection and passed control to
    // us through `Complete`.
    RECEIVED_STORAGESHUTDOWN_COMPLETE,
    // d. We have notified observers that Places has closed the connection.
    NOTIFIED_OBSERVERS_PLACES_CONNECTION_CLOSED,
  };
  States State() {
    return mState;
  }

protected:
  // The blocker name, also used as barrier name.
  nsString mName;
  // The current state, see States.
  States mState;
  // The barrier optionally used to wait for clients.
  nsMainThreadPtrHandle<nsIAsyncShutdownBarrier> mBarrier;
  // The parent object who registered this as a blocker.
  nsMainThreadPtrHandle<nsIAsyncShutdownClient> mParentClient;

  // As tests may resurrect a dead `Database`, we use a counter to
  // give the instances of `PlacesShutdownBlocker` unique names.
  uint16_t mCounter;
  static uint16_t sCounter;

  static Atomic<bool> sIsStarted;

  virtual ~PlacesShutdownBlocker() {}
};

/**
 * Blocker also used to wait for clients, through an owned barrier.
 */
class ClientsShutdownBlocker final : public PlacesShutdownBlocker
{
public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(ClientsShutdownBlocker,
				       PlacesShutdownBlocker)

  explicit ClientsShutdownBlocker();

  NS_IMETHOD Done() override;
private:
  ~ClientsShutdownBlocker() {}
};

/**
 * Blocker used to wait when closing the database connection.
 */
class ConnectionShutdownBlocker final : public PlacesShutdownBlocker
                                      , public mozIStorageCompletionCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_MOZISTORAGECOMPLETIONCALLBACK

  explicit ConnectionShutdownBlocker(mozilla::places::Database* aDatabase);

  NS_IMETHOD Done() override;

private:
  ~ConnectionShutdownBlocker() {}

  // The owning database.
  // The cycle is broken in method Complete(), once the connection
  // has been closed by mozStorage.
  RefPtr<mozilla::places::Database> mDatabase;
};

} // namespace places
} // namespace mozilla

#endif // mozilla_places_Shutdown_h_
