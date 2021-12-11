/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Shutdown.h"
#include "mozilla/Unused.h"
#include "mozilla/SimpleEnumerator.h"
#include "nsIProperty.h"
#include "nsIWritablePropertyBag.h"

namespace mozilla {
namespace places {

uint16_t PlacesShutdownBlocker::sCounter = 0;
Atomic<bool> PlacesShutdownBlocker::sIsStarted(false);

PlacesShutdownBlocker::PlacesShutdownBlocker(const nsString& aName)
    : mName(aName), mState(NOT_STARTED), mCounter(sCounter++) {
  MOZ_ASSERT(NS_IsMainThread());
  // During tests, we can end up with the Database singleton being resurrected.
  // Make sure that each instance of DatabaseShutdown has a unique name.
  if (mCounter > 1) {
    mName.AppendInt(mCounter);
  }
  // Create a barrier that will be exposed to clients through GetClient(), so
  // they can block Places shutdown.
  nsCOMPtr<nsIAsyncShutdownService> asyncShutdown =
      services::GetAsyncShutdownService();
  MOZ_ASSERT(asyncShutdown);
  if (asyncShutdown) {
    nsCOMPtr<nsIAsyncShutdownBarrier> barrier;
    nsresult rv = asyncShutdown->MakeBarrier(mName, getter_AddRefs(barrier));
    MOZ_ALWAYS_SUCCEEDS(rv);
    if (NS_SUCCEEDED(rv) && barrier) {
      mBarrier = new nsMainThreadPtrHolder<nsIAsyncShutdownBarrier>(
          "PlacesShutdownBlocker::mBarrier", barrier);
    }
  }
}

// nsIAsyncShutdownBlocker
NS_IMETHODIMP
PlacesShutdownBlocker::GetName(nsAString& aName) {
  aName = mName;
  return NS_OK;
}

// nsIAsyncShutdownBlocker
NS_IMETHODIMP
PlacesShutdownBlocker::GetState(nsIPropertyBag** _state) {
  NS_ENSURE_ARG_POINTER(_state);

  nsCOMPtr<nsIWritablePropertyBag> bag =
      do_CreateInstance("@mozilla.org/hash-property-bag;1");
  NS_ENSURE_TRUE(bag, NS_ERROR_OUT_OF_MEMORY);

  RefPtr<nsVariant> progress = new nsVariant();
  Unused << NS_WARN_IF(NS_FAILED(progress->SetAsUint8(mState)));
  Unused << NS_WARN_IF(
      NS_FAILED(bag->SetProperty(u"PlacesShutdownProgress"_ns, progress)));

  if (mBarrier) {
    nsCOMPtr<nsIPropertyBag> barrierState;
    if (NS_SUCCEEDED(mBarrier->GetState(getter_AddRefs(barrierState))) &&
        barrierState) {
      nsCOMPtr<nsISimpleEnumerator> enumerator;
      if (NS_SUCCEEDED(
              barrierState->GetEnumerator(getter_AddRefs(enumerator))) &&
          enumerator) {
        for (const auto& property : SimpleEnumerator<nsIProperty>(enumerator)) {
          nsAutoString prefix(u"Barrier: "_ns);
          nsAutoString name;
          Unused << NS_WARN_IF(NS_FAILED(property->GetName(name)));
          prefix.Append(name);
          nsCOMPtr<nsIVariant> value;
          Unused << NS_WARN_IF(
              NS_FAILED(property->GetValue(getter_AddRefs(value))));
          Unused << NS_WARN_IF(NS_FAILED(bag->SetProperty(prefix, value)));
        }
      }
    }
  }
  bag.forget(_state);
  return NS_OK;
}

already_AddRefed<nsIAsyncShutdownClient> PlacesShutdownBlocker::GetClient() {
  nsCOMPtr<nsIAsyncShutdownClient> client;
  if (mBarrier) {
    MOZ_ALWAYS_SUCCEEDS(mBarrier->GetClient(getter_AddRefs(client)));
  }
  return client.forget();
}

// nsIAsyncShutdownBlocker
NS_IMETHODIMP
PlacesShutdownBlocker::BlockShutdown(nsIAsyncShutdownClient* aParentClient) {
  MOZ_ASSERT(NS_IsMainThread());
  mParentClient = new nsMainThreadPtrHolder<nsIAsyncShutdownClient>(
      "ClientsShutdownBlocker::mParentClient", aParentClient);
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
PlacesShutdownBlocker::Done() {
  MOZ_ASSERT(false, "Should always be overridden");
  return NS_OK;
}

NS_IMPL_ISUPPORTS(PlacesShutdownBlocker, nsIAsyncShutdownBlocker,
                  nsIAsyncShutdownCompletionCallback)

////////////////////////////////////////////////////////////////////////////////

ClientsShutdownBlocker::ClientsShutdownBlocker()
    : PlacesShutdownBlocker(u"Places Clients shutdown"_ns) {
  // Do nothing.
}

// nsIAsyncShutdownCompletionCallback
NS_IMETHODIMP
ClientsShutdownBlocker::Done() {
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

////////////////////////////////////////////////////////////////////////////////

ConnectionShutdownBlocker::ConnectionShutdownBlocker(Database* aDatabase)
    : PlacesShutdownBlocker(u"Places Connection shutdown"_ns),
      mDatabase(aDatabase) {
  // Do nothing.
}

// nsIAsyncShutdownCompletionCallback
NS_IMETHODIMP
ConnectionShutdownBlocker::Done() {
  // At this point all the clients are done, we can stop blocking the shutdown
  // phase.
  mState = RECEIVED_DONE;

  // Annotate that Database shutdown started.
  sIsStarted = true;

  // At this stage, any use of this database is forbidden. Get rid of
  // `gDatabase`. Note, however, that the database could be
  // resurrected.  This can happen in particular during tests.
  MOZ_ASSERT(Database::gDatabase == nullptr ||
             Database::gDatabase == mDatabase);
  Database::gDatabase = nullptr;

  // Database::Shutdown will invoke Complete once the connection is closed.
  mDatabase->Shutdown();
  mState = CALLED_STORAGESHUTDOWN;
  mBarrier = nullptr;
  return NS_OK;
}

// mozIStorageCompletionCallback
NS_IMETHODIMP
ConnectionShutdownBlocker::Complete(nsresult, nsISupports*) {
  MOZ_ASSERT(NS_IsMainThread());
  mState = RECEIVED_STORAGESHUTDOWN_COMPLETE;

  // The connection is closed, the Database has no more use, so we can break
  // possible cycles.
  mDatabase = nullptr;

  // Notify the connection has gone.
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  MOZ_ASSERT(os);
  if (os) {
    MOZ_ALWAYS_SUCCEEDS(
        os->NotifyObservers(nullptr, TOPIC_PLACES_CONNECTION_CLOSED, nullptr));
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

NS_IMPL_ISUPPORTS_INHERITED(ConnectionShutdownBlocker, PlacesShutdownBlocker,
                            mozIStorageCompletionCallback)

}  // namespace places
}  // namespace mozilla
