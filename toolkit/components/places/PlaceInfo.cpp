/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PlaceInfo.h"
#include "VisitInfo.h"
#include "nsIURI.h"
#include "nsServiceManagerUtils.h"
#include "nsIXPConnect.h"
#include "jsapi.h"
#include "js/Array.h"  // JS::NewArrayObject

namespace mozilla {
namespace places {

////////////////////////////////////////////////////////////////////////////////
//// PlaceInfo

PlaceInfo::PlaceInfo(int64_t aId, const nsCString& aGUID,
                     already_AddRefed<nsIURI> aURI, const nsString& aTitle,
                     int64_t aFrecency)
    : mId(aId),
      mGUID(aGUID),
      mURI(aURI),
      mTitle(aTitle),
      mFrecency(aFrecency),
      mVisitsAvailable(false) {
  MOZ_ASSERT(mURI, "Must provide a non-null uri!");
}

PlaceInfo::PlaceInfo(int64_t aId, const nsCString& aGUID,
                     already_AddRefed<nsIURI> aURI, const nsString& aTitle,
                     int64_t aFrecency, const VisitsArray& aVisits)
    : mId(aId),
      mGUID(aGUID),
      mURI(aURI),
      mTitle(aTitle),
      mFrecency(aFrecency),
      mVisits(aVisits.Clone()),
      mVisitsAvailable(true) {
  MOZ_ASSERT(mURI, "Must provide a non-null uri!");
}

////////////////////////////////////////////////////////////////////////////////
//// mozIPlaceInfo

NS_IMETHODIMP
PlaceInfo::GetPlaceId(int64_t* _placeId) {
  *_placeId = mId;
  return NS_OK;
}

NS_IMETHODIMP
PlaceInfo::GetGuid(nsACString& _guid) {
  _guid = mGUID;
  return NS_OK;
}

NS_IMETHODIMP
PlaceInfo::GetUri(nsIURI** _uri) {
  NS_ADDREF(*_uri = mURI);
  return NS_OK;
}

NS_IMETHODIMP
PlaceInfo::GetTitle(nsAString& _title) {
  _title = mTitle;
  return NS_OK;
}

NS_IMETHODIMP
PlaceInfo::GetFrecency(int64_t* _frecency) {
  *_frecency = mFrecency;
  return NS_OK;
}

NS_IMETHODIMP
PlaceInfo::GetVisits(JSContext* aContext,
                     JS::MutableHandle<JS::Value> _visits) {
  // If the visits data was not provided, return null rather
  // than an empty array to distinguish this case from the case
  // of a place without any visit.
  if (!mVisitsAvailable) {
    _visits.setNull();
    return NS_OK;
  }

  // TODO bug 625913 when we use this in situations that have more than one
  // visit here, we will likely want to make this cache the value.
  JS::Rooted<JSObject*> visits(aContext, JS::NewArrayObject(aContext, 0));
  NS_ENSURE_TRUE(visits, NS_ERROR_OUT_OF_MEMORY);

  JS::Rooted<JSObject*> global(aContext, JS::CurrentGlobalOrNull(aContext));
  NS_ENSURE_TRUE(global, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIXPConnect> xpc = nsIXPConnect::XPConnect();

  for (VisitsArray::size_type idx = 0; idx < mVisits.Length(); idx++) {
    JS::RootedObject jsobj(aContext);
    nsresult rv = xpc->WrapNative(aContext, global, mVisits[idx],
                                  NS_GET_IID(mozIVisitInfo), jsobj.address());
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_STATE(jsobj);

    bool rc = JS_DefineElement(aContext, visits, idx, jsobj, JSPROP_ENUMERATE);
    NS_ENSURE_TRUE(rc, NS_ERROR_UNEXPECTED);
  }

  _visits.setObject(*visits);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsISupports

NS_IMPL_ISUPPORTS(PlaceInfo, mozIPlaceInfo)

}  // namespace places
}  // namespace mozilla
