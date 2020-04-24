/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "mozilla/net/SocketProcessChild.h"
#include "mozilla/net/SocketProcessParent.h"
#include "nsHttpActivityDistributor.h"
#include "nsCOMPtr.h"
#include "nsIOService.h"
#include "nsThreadUtils.h"
#include "NullHttpChannel.h"

namespace mozilla {
namespace net {

typedef nsMainThreadPtrHolder<nsIHttpActivityObserver> ObserverHolder;
typedef nsMainThreadPtrHandle<nsIHttpActivityObserver> ObserverHandle;

NS_IMPL_ISUPPORTS(nsHttpActivityDistributor, nsIHttpActivityDistributor,
                  nsIHttpActivityObserver)

nsHttpActivityDistributor::nsHttpActivityDistributor()
    : mLock("nsHttpActivityDistributor.mLock"), mActivated(false) {}

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
      nsAutoCString portStr(NS_LITERAL_CSTRING(""));
      int32_t port = args.get_HttpActivity().port();
      bool endToEndSSL = args.get_HttpActivity().endToEndSSL();
      if (port != -1 &&
          ((endToEndSSL && port != 443) || (!endToEndSSL && port != 80))) {
        portStr.AppendInt(port);
      }

      nsresult rv = NS_NewURI(getter_AddRefs(uri),
                              (endToEndSSL ? NS_LITERAL_CSTRING("https://")
                                           : NS_LITERAL_CSTRING("http://")) +
                                  args.get_HttpActivity().host() + portStr);
      if (NS_FAILED(rv)) {
        return;
      }

      RefPtr<NullHttpChannel> channel = new NullHttpChannel();
      rv = channel->Init(uri, 0, nullptr, 0, nullptr);
      MOZ_ASSERT(NS_SUCCEEDED(rv));

      Unused << self->ObserveActivity(
          nsCOMPtr<nsISupports>(do_QueryObject(channel)), aActivityType,
          aActivitySubtype, aTimestamp, aExtraSizeData, extraStringData);
    }
  };

  return NS_DispatchToMainThread(NS_NewRunnableFunction(
      "net::nsHttpActivityDistributor::ObserveActivityWithArgs", task));
}

NS_IMETHODIMP
nsHttpActivityDistributor::GetIsActive(bool* isActive) {
  NS_ENSURE_ARG_POINTER(isActive);
  MutexAutoLock lock(mLock);
  if (XRE_IsSocketProcess()) {
    *isActive = mActivated;
    return NS_OK;
  }

  *isActive = !!mObservers.Length();
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

  if (nsIOService::UseSocketProcess() && wasEmpty) {
    SocketProcessParent* parent = SocketProcessParent::GetSingleton();
    if (parent && parent->CanSend()) {
      Unused << parent->SendOnHttpActivityDistributorActivated(true);
    } else {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHttpActivityDistributor::RemoveObserver(nsIHttpActivityObserver* aObserver) {
  MOZ_ASSERT(XRE_IsParentProcess());

  ObserverHandle observer(
      new ObserverHolder("nsIHttpActivityObserver", aObserver));

  bool isEmpty = false;
  {
    MutexAutoLock lock(mLock);
    if (!mObservers.RemoveElement(observer)) return NS_ERROR_FAILURE;
    isEmpty = mObservers.IsEmpty();
  }

  if (nsIOService::UseSocketProcess() && isEmpty) {
    SocketProcessParent* parent = SocketProcessParent::GetSingleton();
    if (parent && parent->CanSend()) {
      Unused << parent->SendOnHttpActivityDistributorActivated(false);
    } else {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
