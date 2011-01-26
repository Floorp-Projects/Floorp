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
 * The Original Code is PlaceInfo object.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com>
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

#include "PlaceInfo.h"
#include "VisitInfo.h"
#include "nsIURI.h"
#include "nsServiceManagerUtils.h"
#include "nsIXPConnect.h"

namespace mozilla {
namespace places {

////////////////////////////////////////////////////////////////////////////////
//// PlaceInfo

PlaceInfo::PlaceInfo(PRInt64 aId,
                     const nsCString& aGUID,
                     already_AddRefed<nsIURI> aURI,
                     const nsString& aTitle,
                     PRInt64 aFrecency,
                     const VisitsArray& aVisits)
: mId(aId)
, mGUID(aGUID)
, mURI(aURI)
, mTitle(aTitle)
, mFrecency(aFrecency)
, mVisits(aVisits)
{
  NS_PRECONDITION(mURI, "Must provide a non-null uri!");
}

////////////////////////////////////////////////////////////////////////////////
//// mozIPlaceInfo

NS_IMETHODIMP
PlaceInfo::GetPlaceId(PRInt64* _placeId)
{
  *_placeId = mId;
  return NS_OK;
}

NS_IMETHODIMP
PlaceInfo::GetGuid(nsACString& _guid)
{
  _guid = mGUID;
  return NS_OK;
}

NS_IMETHODIMP
PlaceInfo::GetUri(nsIURI** _uri)
{
  NS_ADDREF(*_uri = mURI);
  return NS_OK;
}

NS_IMETHODIMP
PlaceInfo::GetTitle(nsAString& _title)
{
  _title = mTitle;
  return NS_OK;
}

NS_IMETHODIMP
PlaceInfo::GetFrecency(PRInt64* _frecency)
{
  *_frecency = mFrecency;
  return NS_OK;
}

NS_IMETHODIMP
PlaceInfo::GetVisits(JSContext* aContext,
                     jsval* _visits)
{
  // TODO bug 625913 when we use this in situations that have more than one
  // visit here, we will likely want to make this cache the value.
  JSObject* visits = JS_NewArrayObject(aContext, 0, NULL);
  NS_ENSURE_TRUE(visits, NS_ERROR_OUT_OF_MEMORY);

  JSObject* global = JS_GetGlobalForScopeChain(aContext);
  NS_ENSURE_TRUE(global, NS_ERROR_UNEXPECTED);

  static NS_DEFINE_CID(kXPConnectCID, NS_XPCONNECT_CID);
  nsresult rv;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(kXPConnectCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  for (VisitsArray::size_type idx = 0; idx < mVisits.Length(); idx++) {
    nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
    rv = xpc->WrapNative(aContext, global, mVisits[idx],
                         NS_GET_IID(mozIVisitInfo),
                         getter_AddRefs(wrapper));
    NS_ENSURE_SUCCESS(rv, rv);

    JSObject* jsobj;
    rv = wrapper->GetJSObject(&jsobj);
    NS_ENSURE_SUCCESS(rv, rv);
    jsval wrappedVisit = OBJECT_TO_JSVAL(jsobj);

    JSBool rc = JS_SetElement(aContext, visits, idx, &wrappedVisit);
    NS_ENSURE_TRUE(rc, NS_ERROR_UNEXPECTED);
  }

  *_visits = OBJECT_TO_JSVAL(visits);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsISupports

NS_IMPL_ISUPPORTS1(
  PlaceInfo
, mozIPlaceInfo
)

} // namespace places
} // namespace mozilla
