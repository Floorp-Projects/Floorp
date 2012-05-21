/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHttpActivityDistributor.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"

using namespace mozilla;

class nsHttpActivityEvent : public nsRunnable
{
public:
    nsHttpActivityEvent(nsISupports *aHttpChannel,
                        PRUint32 aActivityType,
                        PRUint32 aActivitySubtype,
                        PRTime aTimestamp,
                        PRUint64 aExtraSizeData,
                        const nsACString & aExtraStringData,
                        nsCOMArray<nsIHttpActivityObserver> *aObservers)
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
        for (PRInt32 i = 0 ; i < mObservers.Count() ; i++)
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
    PRUint32 mActivityType;
    PRUint32 mActivitySubtype;
    PRTime mTimestamp;
    PRUint64 mExtraSizeData;
    nsCString mExtraStringData;

    nsCOMArray<nsIHttpActivityObserver> mObservers;
};

NS_IMPL_THREADSAFE_ISUPPORTS2(nsHttpActivityDistributor,
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
                                           PRUint32 aActivityType,
                                           PRUint32 aActivitySubtype,
                                           PRTime aTimestamp,
                                           PRUint64 aExtraSizeData,
                                           const nsACString & aExtraStringData)
{
    nsRefPtr<nsIRunnable> event;
    {
        MutexAutoLock lock(mLock);

        if (!mObservers.Count())
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
    *isActive = !!mObservers.Count();
    return NS_OK;
}

NS_IMETHODIMP
nsHttpActivityDistributor::AddObserver(nsIHttpActivityObserver *aObserver)
{
    MutexAutoLock lock(mLock);

    if (!mObservers.AppendObject(aObserver))
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

NS_IMETHODIMP
nsHttpActivityDistributor::RemoveObserver(nsIHttpActivityObserver *aObserver)
{
    MutexAutoLock lock(mLock);

    if (!mObservers.RemoveObject(aObserver))
        return NS_ERROR_FAILURE;

    return NS_OK;
}
