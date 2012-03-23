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
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kartikaya Gupta <kgupta@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsAndroidHistory.h"
#include "AndroidBridge.h"
#include "Link.h"

using namespace mozilla;
using mozilla::dom::Link;

NS_IMPL_ISUPPORTS1(nsAndroidHistory, IHistory)

nsAndroidHistory* nsAndroidHistory::sHistory = NULL;

/*static*/
nsAndroidHistory*
nsAndroidHistory::GetSingleton()
{
  if (!sHistory) {
    sHistory = new nsAndroidHistory();
    NS_ENSURE_TRUE(sHistory, nsnull);
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

  nsCAutoString uri;
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

  nsCAutoString uri;
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
nsAndroidHistory::VisitURI(nsIURI *aURI, nsIURI *aLastVisitedURI, PRUint32 aFlags)
{
  if (!aURI)
    return NS_OK;

  if (!(aFlags & VisitFlags::TOP_LEVEL))
    return NS_OK;

  AndroidBridge *bridge = AndroidBridge::Bridge();
  if (bridge) {
    nsCAutoString uri;
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
  // we don't do anything with this right now
  return NS_OK;
}

void /*static*/
nsAndroidHistory::NotifyURIVisited(const nsString& aUriString)
{
  if (! sHistory)
    return;
  sHistory->mPendingURIs.Push(aUriString);
  NS_DispatchToMainThread(sHistory);
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
