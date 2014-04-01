/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadUtils.h"
#include "nsAndroidHistory.h"
#include "AndroidBridge.h"
#include "Link.h"
#include "nsIURI.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"

#define NS_LINK_VISITED_EVENT_TOPIC "link-visited"

using namespace mozilla;
using mozilla::dom::Link;

NS_IMPL_ISUPPORTS2(nsAndroidHistory, IHistory, nsIRunnable)

nsAndroidHistory* nsAndroidHistory::sHistory = nullptr;

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
}

NS_IMETHODIMP
nsAndroidHistory::RegisterVisitedCallback(nsIURI *aURI, Link *aContent)
{
  if (!aContent || !aURI)
    return NS_OK;

  // Silently return if URI is something we would never add to DB.
  bool canAdd;
  nsresult rv = CanAddURI(aURI, &canAdd);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!canAdd) {
    return NS_OK;
  }

  nsAutoCString uri;
  rv = aURI->GetSpec(uri);
  if (NS_FAILED(rv)) return rv;
  NS_ConvertUTF8toUTF16 uriString(uri);

  nsTArray<Link*>* list = mListeners.Get(uriString);
  if (! list) {
    list = new nsTArray<Link*>();
    mListeners.Put(uriString, list);
  }
  list->AppendElement(aContent);

  if (AndroidBridge::HasEnv()) {
    mozilla::widget::android::GeckoAppShell::CheckURIVisited(uriString);
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

void
nsAndroidHistory::AppendToRecentlyVisitedURIs(nsIURI* aURI) {
  if (mRecentlyVisitedURIs.Length() < RECENTLY_VISITED_URI_SIZE) {
    // Append a new element while the array is not full.
    mRecentlyVisitedURIs.AppendElement(aURI);
  } else {
    // Otherwise, replace the oldest member.
    mRecentlyVisitedURIsNextIndex %= RECENTLY_VISITED_URI_SIZE;
    mRecentlyVisitedURIs.ElementAt(mRecentlyVisitedURIsNextIndex) = aURI;
    mRecentlyVisitedURIsNextIndex++;
  }
}

inline bool
nsAndroidHistory::IsRecentlyVisitedURI(nsIURI* aURI) {
  bool equals = false;
  RecentlyVisitedArray::index_type i;
  RecentlyVisitedArray::size_type length = mRecentlyVisitedURIs.Length();
  for (i = 0; i < length && !equals; ++i) {
    aURI->Equals(mRecentlyVisitedURIs.ElementAt(i), &equals);
  }
  return equals;
}

void
nsAndroidHistory::AppendToEmbedURIs(nsIURI* aURI) {
  if (mEmbedURIs.Length() < EMBED_URI_SIZE) {
    // Append a new element while the array is not full.
    mEmbedURIs.AppendElement(aURI);
  } else {
    // Otherwise, replace the oldest member.
    mEmbedURIsNextIndex %= EMBED_URI_SIZE;
    mEmbedURIs.ElementAt(mEmbedURIsNextIndex) = aURI;
    mEmbedURIsNextIndex++;
  }
}

inline bool
nsAndroidHistory::IsEmbedURI(nsIURI* aURI) {
  bool equals = false;
  EmbedArray::index_type i;
  EmbedArray::size_type length = mEmbedURIs.Length();
  for (i = 0; i < length && !equals; ++i) {
    aURI->Equals(mEmbedURIs.ElementAt(i), &equals);
  }
  return equals;
}

NS_IMETHODIMP
nsAndroidHistory::VisitURI(nsIURI *aURI, nsIURI *aLastVisitedURI, uint32_t aFlags)
{
  if (!aURI)
    return NS_OK;

  // Silently return if URI is something we shouldn't add to DB.
  bool canAdd;
  nsresult rv = CanAddURI(aURI, &canAdd);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!canAdd) {
    return NS_OK;
  }

  if (aLastVisitedURI) {
    bool same;
    rv = aURI->Equals(aLastVisitedURI, &same);
    NS_ENSURE_SUCCESS(rv, rv);
    if (same && IsRecentlyVisitedURI(aURI)) {
      // Do not save refresh visits if we have visited this URI recently.
      return NS_OK;
    }
  }

  if (!(aFlags & VisitFlags::TOP_LEVEL)) {
    AppendToEmbedURIs(aURI);
    return NS_OK;
  }

  if (aFlags & VisitFlags::REDIRECT_SOURCE)
    return NS_OK;

  if (aFlags & VisitFlags::UNRECOVERABLE_ERROR)
    return NS_OK;

  if (AndroidBridge::HasEnv()) {
    nsAutoCString uri;
    rv = aURI->GetSpec(uri);
    if (NS_FAILED(rv)) return rv;
    NS_ConvertUTF8toUTF16 uriString(uri);
    mozilla::widget::android::GeckoAppShell::MarkURIVisited(uriString);
  }

  AppendToRecentlyVisitedURIs(aURI);

  // Finally, notify that we've been visited.
  nsCOMPtr<nsIObserverService> obsService =
    mozilla::services::GetObserverService();
  if (obsService) {
    obsService->NotifyObservers(aURI, NS_LINK_VISITED_EVENT_TOPIC, nullptr);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAndroidHistory::SetURITitle(nsIURI *aURI, const nsAString& aTitle)
{
  // Silently return if URI is something we shouldn't add to DB.
  bool canAdd;
  nsresult rv = CanAddURI(aURI, &canAdd);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!canAdd) {
    return NS_OK;
  }

  if (IsEmbedURI(aURI)) {
    return NS_OK;
  }

  if (AndroidBridge::HasEnv()) {
    nsAutoCString uri;
    nsresult rv = aURI->GetSpec(uri);
    if (NS_FAILED(rv)) return rv;
    NS_ConvertUTF8toUTF16 uriString(uri);
    mozilla::widget::android::GeckoAppShell::SetURITitle(uriString, aTitle);
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

// Filter out unwanted URIs such as "chrome:", "mailbox:", etc.
//
// The model is if we don't know differently then add which basically means
// we are suppose to try all the things we know not to allow in and then if
// we don't bail go on and allow it in.
//
// Logic ported from nsNavHistory::CanAddURI.

NS_IMETHODIMP
nsAndroidHistory::CanAddURI(nsIURI* aURI, bool* canAdd)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG_POINTER(canAdd);

  nsAutoCString scheme;
  nsresult rv = aURI->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  // first check the most common cases (HTTP, HTTPS) to allow in to avoid most
  // of the work
  if (scheme.EqualsLiteral("http")) {
    *canAdd = true;
    return NS_OK;
  }
  if (scheme.EqualsLiteral("https")) {
    *canAdd = true;
    return NS_OK;
  }

  // now check for all bad things
  if (scheme.EqualsLiteral("about") ||
      scheme.EqualsLiteral("imap") ||
      scheme.EqualsLiteral("news") ||
      scheme.EqualsLiteral("mailbox") ||
      scheme.EqualsLiteral("moz-anno") ||
      scheme.EqualsLiteral("view-source") ||
      scheme.EqualsLiteral("chrome") ||
      scheme.EqualsLiteral("resource") ||
      scheme.EqualsLiteral("data") ||
      scheme.EqualsLiteral("wyciwyg") ||
      scheme.EqualsLiteral("javascript") ||
      scheme.EqualsLiteral("blob")) {
    *canAdd = false;
    return NS_OK;
  }
  *canAdd = true;
  return NS_OK;
}
