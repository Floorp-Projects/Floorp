/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "mozilla/net/SocketProcessChild.h"
#include "mozilla/net/SocketProcessParent.h"
#include "nsHttpActivityDistributor.h"
#include "nsHttpHandler.h"
#include "nsCOMPtr.h"
#include "nsIOService.h"
#include "nsNetUtil.h"
#include "nsQueryObject.h"
#include "nsThreadUtils.h"
#include "NullHttpChannel.h"

namespace mozilla {
namespace net {

using ObserverHolder = nsMainThreadPtrHolder<nsIHttpActivityObserver>;
using ObserverHandle = nsMainThreadPtrHandle<nsIHttpActivityObserver>;

NS_IMPL_ISUPPORTS(nsHttpActivityDistributor, nsIHttpActivityDistributor,
                  nsIHttpActivityObserver)

NS_IMETHODIMP
nsHttpActivityDistributor::ObserveActivity(nsISupports* aHttpChannel,
                                           uint32_t aActivityType,
                                           uint32_t aActivitySubtype,
                                           PRTime aTimestamp,
                                           uint64_t aExtraSizeData,
                                           const nsACString& aExtraStringData) {
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  for (size_t i = 0; i < mObservers.Length(); i++) {
    Unused << mObservers[i]->ObserveActivity(aHttpChannel, aActivityType,
                                             aActivitySubtype, aTimestamp,
                                             aExtraSizeData, aExtraStringData);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHttpActivityDistributor::ObserveConnectionActivity(
    const nsACString& aHost, int32_t aPort, bool aSSL, bool aHasECH,
    bool aIsHttp3, uint32_t aActivityType, uint32_t aActivitySubtype,
    PRTime aTimestamp, const nsACString& aExtraStringData) {
  MOZ_ASSERT(XRE_IsParentProcess() && NS_IsMainThread());

  for (size_t i = 0; i < mObservers.Length(); i++) {
    Unused << mObservers[i]->ObserveConnectionActivity(
        aHost, aPort, aSSL, aHasECH, aIsHttp3, aActivityType, aActivitySubtype,
        aTimestamp, aExtraStringData);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHttpActivityDistributor::ObserveActivityWithArgs(
    const HttpActivityArgs& aArgs, uint32_t aActivityType,
    uint32_t aActivitySubtype, PRTime aTimestamp, uint64_t aExtraSizeData,
    const nsACString& aExtraStringData) {
  HttpActivityArgs args(aArgs);
  nsCString extraStringData(aExtraStringData);
  if (XRE_IsSocketProcess()) {
    auto task = [args{std::move(args)}, aActivityType, aActivitySubtype,
                 aTimestamp, aExtraSizeData,
                 extraStringData{std::move(extraStringData)}]() {
      SocketProcessChild::GetSingleton()->SendObserveHttpActivity(
          args, aActivityType, aActivitySubtype, aTimestamp, aExtraSizeData,
          extraStringData);
    };

    if (!NS_IsMainThread()) {
      return NS_DispatchToMainThread(NS_NewRunnableFunction(
          "net::nsHttpActivityDistributor::ObserveActivityWithArgs", task));
    }

    task();
    return NS_OK;
  }

  MOZ_ASSERT(XRE_IsParentProcess());

  RefPtr<nsHttpActivityDistributor> self = this;
  auto task = [args{std::move(args)}, aActivityType, aActivitySubtype,
               aTimestamp, aExtraSizeData,
               extraStringData{std::move(extraStringData)},
               self{std::move(self)}]() {
    if (args.type() == HttpActivityArgs::Tuint64_t) {
      nsWeakPtr weakPtr = gHttpHandler->GetWeakHttpChannel(args.get_uint64_t());
      if (nsCOMPtr<nsIHttpChannel> channel = do_QueryReferent(weakPtr)) {
        Unused << self->ObserveActivity(channel, aActivityType,
                                        aActivitySubtype, aTimestamp,
                                        aExtraSizeData, extraStringData);
      }
    } else if (args.type() == HttpActivityArgs::THttpActivity) {
      nsCOMPtr<nsIURI> uri;
      nsAutoCString portStr(""_ns);
      int32_t port = args.get_HttpActivity().port();
      bool endToEndSSL = args.get_HttpActivity().endToEndSSL();
      if (port != -1 &&
          ((endToEndSSL && port != 443) || (!endToEndSSL && port != 80))) {
        portStr.AppendInt(port);
      }

      nsresult rv = NS_NewURI(getter_AddRefs(uri),
                              (endToEndSSL ? "https://"_ns : "http://"_ns) +
                                  args.get_HttpActivity().host() + portStr);
      if (NS_FAILED(rv)) {
        return;
      }

      RefPtr<NullHttpChannel> channel = new NullHttpChannel();
      rv = channel->Init(uri, 0, nullptr, 0, nullptr);
      MOZ_ASSERT(NS_SUCCEEDED(rv));

      Unused << self->ObserveActivity(
          static_cast<nsIChannel*>(channel), aActivityType, aActivitySubtype,
          aTimestamp, aExtraSizeData, extraStringData);
    } else if (args.type() == HttpActivityArgs::THttpConnectionActivity) {
      const HttpConnectionActivity& activity =
          args.get_HttpConnectionActivity();
      Unused << self->ObserveConnectionActivity(
          activity.host(), activity.port(), activity.ssl(), activity.hasECH(),
          activity.isHttp3(), aActivityType, aActivitySubtype, aTimestamp,
          activity.connInfoKey());
    }
  };

  if (!NS_IsMainThread()) {
    return NS_DispatchToMainThread(NS_NewRunnableFunction(
        "net::nsHttpActivityDistributor::ObserveActivityWithArgs", task));
  }

  task();
  return NS_OK;
}

bool nsHttpActivityDistributor::Activated() { return mActivated; }

bool nsHttpActivityDistributor::ObserveProxyResponseEnabled() {
  return mObserveProxyResponse;
}

bool nsHttpActivityDistributor::ObserveConnectionEnabled() {
  return mObserveConnection;
}

NS_IMETHODIMP
nsHttpActivityDistributor::GetIsActive(bool* isActive) {
  NS_ENSURE_ARG_POINTER(isActive);
  if (XRE_IsSocketProcess()) {
    *isActive = mActivated;
    return NS_OK;
  }

  MutexAutoLock lock(mLock);
  *isActive = mActivated = !!mObservers.Length();
  return NS_OK;
}

NS_IMETHODIMP nsHttpActivityDistributor::SetIsActive(bool aActived) {
  MOZ_RELEASE_ASSERT(XRE_IsSocketProcess());

  mActivated = aActived;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpActivityDistributor::AddObserver(nsIHttpActivityObserver* aObserver) {
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!NS_IsMainThread()) {
    // We only support calling this from the main thread.
    return NS_ERROR_FAILURE;
  }

  ObserverHandle observer(
      new ObserverHolder("nsIHttpActivityObserver", aObserver));

  bool wasEmpty = false;
  {
    MutexAutoLock lock(mLock);
    wasEmpty = mObservers.IsEmpty();
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    mObservers.AppendElement(observer);
  }

  if (wasEmpty) {
    mActivated = true;
    if (nsIOService::UseSocketProcess()) {
      auto task = []() {
        SocketProcessParent* parent = SocketProcessParent::GetSingleton();
        if (parent && parent->CanSend()) {
          Unused << parent->SendOnHttpActivityDistributorActivated(true);
        }
      };
      gIOService->CallOrWaitForSocketProcess(task);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHttpActivityDistributor::RemoveObserver(nsIHttpActivityObserver* aObserver) {
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!NS_IsMainThread()) {
    // We only support calling this from the main thread.
    return NS_ERROR_FAILURE;
  }

  ObserverHandle observer(
      new ObserverHolder("nsIHttpActivityObserver", aObserver));

  {
    MutexAutoLock lock(mLock);
    if (!mObservers.RemoveElement(observer)) {
      return NS_ERROR_FAILURE;
    }
    mActivated = mObservers.IsEmpty();
  }

  if (nsIOService::UseSocketProcess() && !mActivated) {
    auto task = []() {
      SocketProcessParent* parent = SocketProcessParent::GetSingleton();
      if (parent && parent->CanSend()) {
        Unused << parent->SendOnHttpActivityDistributorActivated(false);
      }
    };
    gIOService->CallOrWaitForSocketProcess(task);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHttpActivityDistributor::GetObserveProxyResponse(
    bool* aObserveProxyResponse) {
  NS_ENSURE_ARG_POINTER(aObserveProxyResponse);
  bool result = mObserveProxyResponse;
  *aObserveProxyResponse = result;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpActivityDistributor::SetObserveProxyResponse(bool aObserveProxyResponse) {
  if (!NS_IsMainThread()) {
    // We only support calling this from the main thread.
    return NS_ERROR_FAILURE;
  }

  mObserveProxyResponse = aObserveProxyResponse;
  if (nsIOService::UseSocketProcess()) {
    auto task = [aObserveProxyResponse]() {
      SocketProcessParent* parent = SocketProcessParent::GetSingleton();
      if (parent && parent->CanSend()) {
        Unused << parent->SendOnHttpActivityDistributorObserveProxyResponse(
            aObserveProxyResponse);
      }
    };
    gIOService->CallOrWaitForSocketProcess(task);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHttpActivityDistributor::GetObserveConnection(bool* aObserveConnection) {
  NS_ENSURE_ARG_POINTER(aObserveConnection);

  *aObserveConnection = mObserveConnection;
  return NS_OK;
}

NS_IMETHODIMP
nsHttpActivityDistributor::SetObserveConnection(bool aObserveConnection) {
  if (!NS_IsMainThread()) {
    // We only support calling this from the main thread.
    return NS_ERROR_FAILURE;
  }

  mObserveConnection = aObserveConnection;
  if (nsIOService::UseSocketProcess()) {
    auto task = [aObserveConnection]() {
      SocketProcessParent* parent = SocketProcessParent::GetSingleton();
      if (parent && parent->CanSend()) {
        Unused << parent->SendOnHttpActivityDistributorObserveConnection(
            aObserveConnection);
      }
    };
    gIOService->CallOrWaitForSocketProcess(task);
  }
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
