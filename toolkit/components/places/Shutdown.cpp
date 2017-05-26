/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Shutdown.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace places {

uint16_t PlacesShutdownBlocker::sCounter = 0;
Atomic<bool> PlacesShutdownBlocker::sIsStarted(false);

PlacesShutdownBlocker::PlacesShutdownBlocker(const nsString& aName)
  : mName(aName)
  , mState(NOT_STARTED)
  , mCounter(sCounter++)
{
  MOZ_ASSERT(NS_IsMainThread());
  // During tests, we can end up with the Database singleton being resurrected.
  // Make sure that each instance of DatabaseShutdown has a unique name.
  if (mCounter > 1) {
    mName.AppendInt(mCounter);
  }
}

// nsIAsyncShutdownBlocker
NS_IMETHODIMP
PlacesShutdownBlocker::GetName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}

// nsIAsyncShutdownBlocker
NS_IMETHODIMP
PlacesShutdownBlocker::GetState(nsIPropertyBag** _state)
{
  NS_ENSURE_ARG_POINTER(_state);

  nsCOMPtr<nsIWritablePropertyBag2> bag =
    do_CreateInstance("@mozilla.org/hash-property-bag;1");
  NS_ENSURE_TRUE(bag, NS_ERROR_OUT_OF_MEMORY);
  bag.forget(_state);

  // Put `mState` in field `progress`
  RefPtr<nsVariant> progress = new nsVariant();
  nsresult rv = progress->SetAsUint8(mState);
  if (NS_WARN_IF(NS_FAILED(rv))) return rv;
  rv = static_cast<nsIWritablePropertyBag2*>(*_state)->SetPropertyAsInterface(
    NS_LITERAL_STRING("progress"), progress);
  if (NS_WARN_IF(NS_FAILED(rv))) return rv;

  // Put `mBarrier`'s state in field `barrier`, if possible
  if (!mBarrier) {
    return NS_OK;
  }
  nsCOMPtr<nsIPropertyBag> barrierState;
  rv = mBarrier->GetState(getter_AddRefs(barrierState));
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  RefPtr<nsVariant> barrier = new nsVariant();
  rv = barrier->SetAsInterface(NS_GET_IID(nsIPropertyBag), barrierState);
  if (NS_WARN_IF(NS_FAILED(rv))) return rv;
  rv = static_cast<nsIWritablePropertyBag2*>(*_state)->SetPropertyAsInterface(
    NS_LITERAL_STRING("Barrier"), barrier);
  if (NS_WARN_IF(NS_FAILED(rv))) return rv;

  return NS_OK;
}

// nsIAsyncShutdownBlocker
NS_IMETHODIMP
PlacesShutdownBlocker::BlockShutdown(nsIAsyncShutdownClient* aParentClient)
{
  MOZ_ASSERT(false, "should always be overridden");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMPL_ISUPPORTS(
  PlacesShutdownBlocker,
  nsIAsyncShutdownBlocker
)

////////////////////////////////////////////////////////////////////////////////

ClientsShutdownBlocker::ClientsShutdownBlocker()
  : PlacesShutdownBlocker(NS_LITERAL_STRING("Places Clients shutdown"))
{
  MOZ_ASSERT(NS_IsMainThread());
  // Create a barrier that will be exposed to clients through GetClient(), so
  // they can block Places shutdown.
  nsCOMPtr<nsIAsyncShutdownService> asyncShutdown = services::GetAsyncShutdown();
  MOZ_ASSERT(asyncShutdown);
  if (asyncShutdown) {
    nsCOMPtr<nsIAsyncShutdownBarrier> barrier;
    MOZ_ALWAYS_SUCCEEDS(asyncShutdown->MakeBarrier(mName, getter_AddRefs(barrier)));
    mBarrier = new nsMainThreadPtrHolder<nsIAsyncShutdownBarrier>(barrier);
  }
}

already_AddRefed<nsIAsyncShutdownClient>
ClientsShutdownBlocker::GetClient()
{
  nsCOMPtr<nsIAsyncShutdownClient> client;
  if (mBarrier) {
    MOZ_ALWAYS_SUCCEEDS(mBarrier->GetClient(getter_AddRefs(client)));
  }
  return client.forget();
}

// nsIAsyncShutdownBlocker
NS_IMETHODIMP
ClientsShutdownBlocker::BlockShutdown(nsIAsyncShutdownClient* aParentClient)
{
  MOZ_ASSERT(NS_IsMainThread());
  mParentClient = new nsMainThreadPtrHolder<nsIAsyncShutdownClient>(aParentClient);
  mState = RECEIVED_BLOCK_SHUTDOWN;

  if (NS_WARN_IF(!mBarrier)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Wait until all the clients have removed their blockers.
  MOZ_ALWAYS_SUCCEEDS(mBarrier->Wait(this));

  mState = CALLED_WAIT_CLIENTS;
  return NS_OK;
}

// nsIAsyncShutdownCompletionCallback
NS_IMETHODIMP
ClientsShutdownBlocker::Done()
{
  // At this point all the clients are done, we can stop blocking the shutdown
  // phase.
  mState = RECEIVED_DONE;

  // mParentClient is nullptr in tests.
  if (mParentClient) {
    nsresult rv = mParentClient->RemoveBlocker(this);
    if (NS_WARN_IF(NS_FAILED(rv))) return rv;
    mParentClient = nullptr;
  }
  mBarrier = nullptr;
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(
  ClientsShutdownBlocker,
  PlacesShutdownBlocker,
  nsIAsyncShutdownCompletionCallback
)

////////////////////////////////////////////////////////////////////////////////

ConnectionShutdownBlocker::ConnectionShutdownBlocker(Database* aDatabase)
  : PlacesShutdownBlocker(NS_LITERAL_STRING("Places Connection shutdown"))
  , mDatabase(aDatabase)
{
  // Do nothing.
}

// nsIAsyncShutdownBlocker
NS_IMETHODIMP
ConnectionShutdownBlocker::BlockShutdown(nsIAsyncShutdownClient* aParentClient)
{
  MOZ_ASSERT(NS_IsMainThread());
  mParentClient = new nsMainThreadPtrHolder<nsIAsyncShutdownClient>(aParentClient);
  mState = RECEIVED_BLOCK_SHUTDOWN;
  // Annotate that Database shutdown started.
  sIsStarted = true;

  // Fire internal database closing notification.
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  MOZ_ASSERT(os);
  if (os) {
    Unused << os->NotifyObservers(nullptr, TOPIC_PLACES_WILL_CLOSE_CONNECTION, nullptr);
  }
  mState = NOTIFIED_OBSERVERS_PLACES_WILL_CLOSE_CONNECTION;

  // At this stage, any use of this database is forbidden. Get rid of
  // `gDatabase`. Note, however, that the database could be
  // resurrected.  This can happen in particular during tests.
  MOZ_ASSERT(Database::gDatabase == nullptr || Database::gDatabase == mDatabase);
  Database::gDatabase = nullptr;

  // Database::Shutdown will invoke Complete once the connection is closed.
  mDatabase->Shutdown(true);
  mState = CALLED_STORAGESHUTDOWN;
  return NS_OK;
}

// mozIStorageCompletionCallback
NS_IMETHODIMP
ConnectionShutdownBlocker::Complete(nsresult, nsISupports*)
{
  MOZ_ASSERT(NS_IsMainThread());
  mState = RECEIVED_STORAGESHUTDOWN_COMPLETE;

  // The connection is closed, the Database has no more use, so we can break
  // possible cycles.
  mDatabase = nullptr;

  // Notify the connection has gone.
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  MOZ_ASSERT(os);
  if (os) {
    MOZ_ALWAYS_SUCCEEDS(os->NotifyObservers(nullptr,
					    TOPIC_PLACES_CONNECTION_CLOSED,
					    nullptr));
  }
  mState = NOTIFIED_OBSERVERS_PLACES_CONNECTION_CLOSED;

  // mParentClient is nullptr in tests
  if (mParentClient) {
    nsresult rv = mParentClient->RemoveBlocker(this);
    if (NS_WARN_IF(NS_FAILED(rv))) return rv;
    mParentClient = nullptr;
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(
  ConnectionShutdownBlocker,
  PlacesShutdownBlocker,
  mozIStorageCompletionCallback
)

} // namespace places
} // namespace mozilla
