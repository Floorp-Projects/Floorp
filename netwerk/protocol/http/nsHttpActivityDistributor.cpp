/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Michal Novotny <michal.novotny@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
