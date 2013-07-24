/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "nsHttpActivityDistributor.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"

using namespace mozilla;
typedef nsMainThreadPtrHolder<nsIHttpActivityObserver> ObserverHolder;
typedef nsMainThreadPtrHandle<nsIHttpActivityObserver> ObserverHandle;
typedef nsTArray<ObserverHandle> ObserverArray;

class nsHttpActivityEvent : public nsRunnable
{
public:
    nsHttpActivityEvent(nsISupports *aHttpChannel,
                        uint32_t aActivityType,
                        uint32_t aActivitySubtype,
                        PRTime aTimestamp,
                        uint64_t aExtraSizeData,
                        const nsACString & aExtraStringData,
                        ObserverArray *aObservers)
        : mHttpChannel(aHttpChannel)
        , mActivityType(aActivityType)
        , mActivitySubtype(aActivitySubtype)
        , mTimestamp(aTimestamp)
        , mExtraSizeData(aExtraSizeData)
        , mExtraStringData(aExtraStringData)
        , mObservers(*aObservers)
    {
    }

    NS_IMETHOD Run()
    {
        for (size_t i = 0 ; i < mObservers.Length() ; i++)
            mObservers[i]->ObserveActivity(mHttpChannel, mActivityType,
                                           mActivitySubtype, mTimestamp,
                                           mExtraSizeData, mExtraStringData);
        return NS_OK;
    }

private:
    virtual ~nsHttpActivityEvent()
    {
    }

    nsCOMPtr<nsISupports> mHttpChannel;
    uint32_t mActivityType;
    uint32_t mActivitySubtype;
    PRTime mTimestamp;
    uint64_t mExtraSizeData;
    nsCString mExtraStringData;

    ObserverArray mObservers;
};

NS_IMPL_ISUPPORTS2(nsHttpActivityDistributor,
                   nsIHttpActivityDistributor,
                   nsIHttpActivityObserver)

nsHttpActivityDistributor::nsHttpActivityDistributor()
    : mLock("nsHttpActivityDistributor.mLock")
{
}

nsHttpActivityDistributor::~nsHttpActivityDistributor()
{
}

NS_IMETHODIMP
nsHttpActivityDistributor::ObserveActivity(nsISupports *aHttpChannel,
                                           uint32_t aActivityType,
                                           uint32_t aActivitySubtype,
                                           PRTime aTimestamp,
                                           uint64_t aExtraSizeData,
                                           const nsACString & aExtraStringData)
{
    nsRefPtr<nsIRunnable> event;
    {
        MutexAutoLock lock(mLock);

        if (!mObservers.Length())
            return NS_OK;

        event = new nsHttpActivityEvent(aHttpChannel, aActivityType,
                                        aActivitySubtype, aTimestamp,
                                        aExtraSizeData, aExtraStringData,
                                        &mObservers);
    }
    NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);
    return NS_DispatchToMainThread(event);
}

NS_IMETHODIMP
nsHttpActivityDistributor::GetIsActive(bool *isActive)
{
    NS_ENSURE_ARG_POINTER(isActive);
    MutexAutoLock lock(mLock);
    *isActive = !!mObservers.Length();
    return NS_OK;
}

NS_IMETHODIMP
nsHttpActivityDistributor::AddObserver(nsIHttpActivityObserver *aObserver)
{
    MutexAutoLock lock(mLock);

    ObserverHandle observer(new ObserverHolder(aObserver));
    if (!mObservers.AppendElement(observer))
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

NS_IMETHODIMP
nsHttpActivityDistributor::RemoveObserver(nsIHttpActivityObserver *aObserver)
{
    MutexAutoLock lock(mLock);

    ObserverHandle observer(new ObserverHolder(aObserver));
    if (!mObservers.RemoveElement(observer))
        return NS_ERROR_FAILURE;

    return NS_OK;
}
