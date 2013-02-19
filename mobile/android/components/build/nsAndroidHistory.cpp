/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAndroidHistory.h"
#include "AndroidBridge.h"
#include "Link.h"

using namespace mozilla;
using mozilla::dom::Link;

NS_IMPL_ISUPPORTS2(nsAndroidHistory, IHistory, nsIRunnable)

nsAndroidHistory* nsAndroidHistory::sHistory = NULL;

/*static*/
nsAndroidHistory*
nsAndroidHistory::GetSingleton()
{
  if (!sHistory) {
    sHistory = new nsAndroidHistory();
    NS_ENSURE_TRUE(sHistory, nullptr);
  }

  NS_ADDREF(sHistory);
  return sHistory;
}

nsAndroidHistory::nsAndroidHistory()
{
  mListeners.Init();
}

NS_IMETHODIMP
nsAndroidHistory::RegisterVisitedCallback(nsIURI *aURI, Link *aContent)
{
  if (!aContent || !aURI)
    return NS_OK;

  nsAutoCString uri;
  nsresult rv = aURI->GetSpec(uri);
  if (NS_FAILED(rv)) return rv;
  NS_ConvertUTF8toUTF16 uriString(uri);

  nsTArray<Link*>* list = mListeners.Get(uriString);
  if (! list) {
    list = new nsTArray<Link*>();
    mListeners.Put(uriString, list);
  }
  list->AppendElement(aContent);

  AndroidBridge *bridge = AndroidBridge::Bridge();
  if (bridge) {
    bridge->CheckURIVisited(uriString);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAndroidHistory::UnregisterVisitedCallback(nsIURI *aURI, Link *aContent)
{
  if (!aContent || !aURI)
    return NS_OK;

  nsAutoCString uri;
  nsresult rv = aURI->GetSpec(uri);
  if (NS_FAILED(rv)) return rv;
  NS_ConvertUTF8toUTF16 uriString(uri);

  nsTArray<Link*>* list = mListeners.Get(uriString);
  if (! list)
    return NS_OK;

  list->RemoveElement(aContent);
  if (list->IsEmpty()) {
    mListeners.Remove(uriString);
    delete list;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAndroidHistory::VisitURI(nsIURI *aURI, nsIURI *aLastVisitedURI, uint32_t aFlags)
{
  if (!aURI)
    return NS_OK;

  if (!(aFlags & VisitFlags::TOP_LEVEL))
    return NS_OK;

  if (aFlags & VisitFlags::REDIRECT_SOURCE)
    return NS_OK;

  if (aFlags & VisitFlags::UNRECOVERABLE_ERROR)
    return NS_OK;

  AndroidBridge *bridge = AndroidBridge::Bridge();
  if (bridge) {
    nsAutoCString uri;
    nsresult rv = aURI->GetSpec(uri);
    if (NS_FAILED(rv)) return rv;
    NS_ConvertUTF8toUTF16 uriString(uri);
    bridge->MarkURIVisited(uriString);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAndroidHistory::SetURITitle(nsIURI *aURI, const nsAString& aTitle)
{
  AndroidBridge *bridge = AndroidBridge::Bridge();
  if (bridge) {
    nsAutoCString uri;
    nsresult rv = aURI->GetSpec(uri);
    if (NS_FAILED(rv)) return rv;
    NS_ConvertUTF8toUTF16 uriString(uri);
    bridge->SetURITitle(uriString, aTitle);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAndroidHistory::NotifyVisited(nsIURI *aURI)
{
  if (aURI && sHistory) {
    nsAutoCString spec;
    (void)aURI->GetSpec(spec);
    sHistory->mPendingURIs.Push(NS_ConvertUTF8toUTF16(spec));
    NS_DispatchToMainThread(sHistory);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAndroidHistory::Run()
{
  while (! mPendingURIs.IsEmpty()) {
    nsString uriString = mPendingURIs.Pop();
    nsTArray<Link*>* list = sHistory->mListeners.Get(uriString);
    if (list) {
      for (unsigned int i = 0; i < list->Length(); i++) {
        list->ElementAt(i)->SetLinkState(eLinkState_Visited);
      }
      // as per the IHistory interface contract, remove the
      // Link pointers once they have been notified
      mListeners.Remove(uriString);
      delete list;
    }
  }
  return NS_OK;
}
