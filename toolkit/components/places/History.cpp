/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "nsXULAppAPI.h"

#include "History.h"
#include "nsNavHistory.h"
#include "nsNavBookmarks.h"
#include "nsAnnotationService.h"
#include "Helpers.h"
#include "PlaceInfo.h"
#include "VisitInfo.h"
#include "nsPlacesMacros.h"

#include "mozilla/storage.h"
#include "mozilla/dom/Link.h"
#include "nsDocShellCID.h"
#include "mozilla/Services.h"
#include "nsThreadUtils.h"
#include "nsNetUtil.h"
#include "nsIFileURL.h"
#include "nsIXPConnect.h"
#include "mozilla/Unused.h"
#include "nsContentUtils.h" // for nsAutoScriptBlocker
#include "nsJSUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsPrintfCString.h"
#include "nsTHashtable.h"
#include "jsapi.h"
#include "mozilla/dom/Element.h"

// Initial size for the cache holding visited status observers.
#define VISIT_OBSERVERS_INITIAL_CACHE_LENGTH 64

// Initial length for the visits removal hash.
#define VISITS_REMOVAL_INITIAL_HASH_LENGTH 64

using namespace mozilla::dom;
using namespace mozilla::ipc;
using mozilla::Unused;

namespace mozilla {
namespace places {

////////////////////////////////////////////////////////////////////////////////
//// Global Defines

#define URI_VISITED "visited"
#define URI_NOT_VISITED "not visited"
#define URI_VISITED_RESOLUTION_TOPIC "visited-status-resolution"
// Observer event fired after a visit has been registered in the DB.
#define URI_VISIT_SAVED "uri-visit-saved"

#define DESTINATIONFILEURI_ANNO \
        NS_LITERAL_CSTRING("downloads/destinationFileURI")

////////////////////////////////////////////////////////////////////////////////
//// VisitData

struct VisitData {
  VisitData()
  : placeId(0)
  , visitId(0)
  , hidden(true)
  , shouldUpdateHidden(true)
  , typed(false)
  , transitionType(UINT32_MAX)
  , visitTime(0)
  , frecency(-1)
  , lastVisitId(0)
  , lastVisitTime(0)
  , visitCount(0)
  , referrerVisitId(0)
  , titleChanged(false)
  , shouldUpdateFrecency(true)
  , redirect(false)
  {
    guid.SetIsVoid(true);
    title.SetIsVoid(true);
  }

  explicit VisitData(nsIURI* aURI,
                     nsIURI* aReferrer = nullptr)
  : placeId(0)
  , visitId(0)
  , hidden(true)
  , shouldUpdateHidden(true)
  , typed(false)
  , transitionType(UINT32_MAX)
  , visitTime(0)
  , frecency(-1)
  , lastVisitId(0)
  , lastVisitTime(0)
  , visitCount(0)
  , referrerVisitId(0)
  , titleChanged(false)
  , shouldUpdateFrecency(true)
  , redirect(false)
  {
    MOZ_ASSERT(aURI);
    if (aURI) {
      (void)aURI->GetSpec(spec);
      (void)GetReversedHostname(aURI, revHost);
    }
    if (aReferrer) {
      (void)aReferrer->GetSpec(referrerSpec);
    }
    guid.SetIsVoid(true);
    title.SetIsVoid(true);
  }

  /**
   * Sets the transition type of the visit, as well as if it was typed.
   *
   * @param aTransitionType
   *        The transition type constant to set.  Must be one of the
   *        TRANSITION_ constants on nsINavHistoryService.
   */
  void SetTransitionType(uint32_t aTransitionType)
  {
    typed = aTransitionType == nsINavHistoryService::TRANSITION_TYPED;
    transitionType = aTransitionType;
  }

  int64_t placeId;
  nsCString guid;
  int64_t visitId;
  nsCString spec;
  nsString revHost;
  bool hidden;
  bool shouldUpdateHidden;
  bool typed;
  uint32_t transitionType;
  PRTime visitTime;
  int32_t frecency;
  int64_t lastVisitId;
  PRTime lastVisitTime;
  uint32_t visitCount;

  /**
   * Stores the title.  If this is empty (IsEmpty() returns true), then the
   * title should be removed from the Place.  If the title is void (IsVoid()
   * returns true), then no title has been set on this object, and titleChanged
   * should remain false.
   */
  nsString title;

  nsCString referrerSpec;
  int64_t referrerVisitId;

  // TODO bug 626836 hook up hidden and typed change tracking too!
  bool titleChanged;

  // Indicates whether frecency should be updated for this visit.
  bool shouldUpdateFrecency;

  // Whether this is a redirect source.
  bool redirect;
};

////////////////////////////////////////////////////////////////////////////////
//// nsVisitData

class nsVisitData : public nsIVisitData
{
public:
  explicit nsVisitData(nsIURI* aURI,
                       int64_t aVisitId,
                       PRTime aTime,
                       int64_t aReferrerVisitId,
                       int32_t aTransitionType,
                       const nsACString& aGuid,
                       bool aHidden,
                       uint32_t aVisitCount,
                       uint32_t aTyped,
                       const nsAString& aLastKnownTitle)
    : mURI(aURI)
    , mVisitId(aVisitId)
    , mTime(aTime)
    , mReferrerVisitId(aReferrerVisitId)
    , mTransitionType(aTransitionType)
    , mGuid(aGuid)
    , mHidden(aHidden)
    , mVisitCount(aVisitCount)
    , mTyped(aTyped)
    , mLastKnownTitle(aLastKnownTitle)
  {
    MOZ_ASSERT(NS_IsMainThread(),
               "nsVisitData should only be constructed on the main thread.");
  }

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetUri(nsIURI** aUri) override
  {
    NS_ENSURE_ARG_POINTER(aUri);
    *aUri = mURI;
    NS_IF_ADDREF(*aUri);
    return NS_OK;
  }

  NS_IMETHOD GetVisitId(int64_t* aVisitId) override
  {
    *aVisitId = mVisitId;
    return NS_OK;
  }

  NS_IMETHOD GetTime(PRTime* aTime) override
  {
    *aTime = mTime;
    return NS_OK;
  }

  NS_IMETHOD GetReferrerId(int64_t* aReferrerVisitId) override
  {
    *aReferrerVisitId = mReferrerVisitId;
    return NS_OK;
  }

  NS_IMETHOD GetTransitionType(uint32_t* aTransitionType) override
  {
    *aTransitionType = mTransitionType;
    return NS_OK;
  }

  NS_IMETHOD GetGuid(nsACString& aGuid) override
  {
    aGuid.Assign(mGuid);
    return NS_OK;
  }

  NS_IMETHOD GetHidden(bool* aHidden) override
  {
    *aHidden = mHidden;
    return NS_OK;
  }

  NS_IMETHOD GetVisitCount(uint32_t* aVisitCount) override
  {
    *aVisitCount = mVisitCount;
    return NS_OK;
  }

  NS_IMETHOD GetTyped(uint32_t* aTyped) override
  {
    *aTyped = mTyped;
    return NS_OK;
  }

  NS_IMETHOD GetLastKnownTitle(nsAString& aLastKnownTitle) override
  {
    aLastKnownTitle.Assign(mLastKnownTitle);
    return NS_OK;
  }

private:
  virtual ~nsVisitData() {
    MOZ_ASSERT(NS_IsMainThread(),
               "nsVisitData should only be destructed on the main thread.");
  };

  nsCOMPtr<nsIURI> mURI;
  int64_t mVisitId;
  PRTime mTime;
  int64_t mReferrerVisitId;
  uint32_t mTransitionType;
  nsCString mGuid;
  bool mHidden;
  uint32_t mVisitCount;
  uint32_t mTyped;
  nsString mLastKnownTitle;
};

NS_IMPL_ISUPPORTS(nsVisitData, nsIVisitData)

////////////////////////////////////////////////////////////////////////////////
//// RemoveVisitsFilter

/**
 * Used to store visit filters for RemoveVisits.
 */
struct RemoveVisitsFilter {
  RemoveVisitsFilter()
  : transitionType(UINT32_MAX)
  {
  }

  uint32_t transitionType;
};

////////////////////////////////////////////////////////////////////////////////
//// PlaceHashKey

class PlaceHashKey : public nsCStringHashKey
{
public:
  explicit PlaceHashKey(const nsACString& aSpec)
    : nsCStringHashKey(&aSpec)
    , mVisitCount(0)
    , mBookmarked(false)
#ifdef DEBUG
    , mIsInitialized(false)
#endif
  {
  }

  explicit PlaceHashKey(const nsACString* aSpec)
    : nsCStringHashKey(aSpec)
    , mVisitCount(0)
    , mBookmarked(false)
#ifdef DEBUG
    , mIsInitialized(false)
#endif
  {
  }

  PlaceHashKey(const PlaceHashKey& aOther)
    : nsCStringHashKey(&aOther.GetKey())
  {
    MOZ_ASSERT(false, "Do not call me!");
  }

  void SetProperties(uint32_t aVisitCount, bool aBookmarked)
  {
    mVisitCount = aVisitCount;
    mBookmarked = aBookmarked;
#ifdef DEBUG
    mIsInitialized = true;
#endif
  }

  uint32_t VisitCount() const
  {
#ifdef DEBUG
    MOZ_ASSERT(mIsInitialized, "PlaceHashKey::mVisitCount not set");
#endif
    return mVisitCount;
  }

  bool IsBookmarked() const
  {
#ifdef DEBUG
    MOZ_ASSERT(mIsInitialized, "PlaceHashKey::mBookmarked not set");
#endif
    return mBookmarked;
  }

  // Array of VisitData objects.
  nsTArray<VisitData> mVisits;
private:
  // Visit count for this place.
  uint32_t mVisitCount;
  // Whether this place is bookmarked.
  bool mBookmarked;
#ifdef DEBUG
  // Whether previous attributes are set.
  bool mIsInitialized;
#endif
};

////////////////////////////////////////////////////////////////////////////////
//// Anonymous Helpers

namespace {

/**
 * Convert the given js value to a js array.
 *
 * @param [in] aValue
 *        the JS value to convert.
 * @param [in] aCtx
 *        The JSContext for aValue.
 * @param [out] _array
 *        the JS array.
 * @param [out] _arrayLength
 *        _array's length.
 */
nsresult
GetJSArrayFromJSValue(JS::Handle<JS::Value> aValue,
                      JSContext* aCtx,
                      JS::MutableHandle<JSObject*> _array,
                      uint32_t* _arrayLength) {
  if (aValue.isObjectOrNull()) {
    JS::Rooted<JSObject*> val(aCtx, aValue.toObjectOrNull());
    bool isArray;
    if (!JS_IsArrayObject(aCtx, val, &isArray)) {
      return NS_ERROR_UNEXPECTED;
    }
    if (isArray) {
      _array.set(val);
      (void)JS_GetArrayLength(aCtx, _array, _arrayLength);
      NS_ENSURE_ARG(*_arrayLength > 0);
      return NS_OK;
    }
  }

  // Build a temporary array to store this one item so the code below can
  // just loop.
  *_arrayLength = 1;
  _array.set(JS_NewArrayObject(aCtx, 0));
  NS_ENSURE_TRUE(_array, NS_ERROR_OUT_OF_MEMORY);

  bool rc = JS_DefineElement(aCtx, _array, 0, aValue, 0);
  NS_ENSURE_TRUE(rc, NS_ERROR_UNEXPECTED);
  return NS_OK;
}

/**
 * Attemps to convert a given js value to a nsIURI object.
 * @param aCtx
 *        The JSContext for aValue.
 * @param aValue
 *        The JS value to convert.
 * @return the nsIURI object, or null if aValue is not a nsIURI object.
 */
already_AddRefed<nsIURI>
GetJSValueAsURI(JSContext* aCtx,
                const JS::Value& aValue) {
  if (!aValue.isPrimitive()) {
    nsCOMPtr<nsIXPConnect> xpc = mozilla::services::GetXPConnect();

    nsCOMPtr<nsIXPConnectWrappedNative> wrappedObj;
    nsresult rv = xpc->GetWrappedNativeOfJSObject(aCtx, aValue.toObjectOrNull(),
                                                  getter_AddRefs(wrappedObj));
    NS_ENSURE_SUCCESS(rv, nullptr);
    nsCOMPtr<nsIURI> uri = do_QueryWrappedNative(wrappedObj);
    return uri.forget();
  }
  return nullptr;
}

/**
 * Obtains an nsIURI from the "uri" property of a JSObject.
 *
 * @param aCtx
 *        The JSContext for aObject.
 * @param aObject
 *        The JSObject to get the URI from.
 * @param aProperty
 *        The name of the property to get the URI from.
 * @return the URI if it exists.
 */
already_AddRefed<nsIURI>
GetURIFromJSObject(JSContext* aCtx,
                   JS::Handle<JSObject *> aObject,
                   const char* aProperty)
{
  JS::Rooted<JS::Value> uriVal(aCtx);
  bool rc = JS_GetProperty(aCtx, aObject, aProperty, &uriVal);
  NS_ENSURE_TRUE(rc, nullptr);
  return GetJSValueAsURI(aCtx, uriVal);
}

/**
 * Attemps to convert a JS value to a string.
 * @param aCtx
 *        The JSContext for aObject.
 * @param aValue
 *        The JS value to convert.
 * @param _string
 *        The string to populate with the value, or set it to void.
 */
void
GetJSValueAsString(JSContext* aCtx,
                   const JS::Value& aValue,
                   nsString& _string) {
  if (aValue.isUndefined() ||
      !(aValue.isNull() || aValue.isString())) {
    _string.SetIsVoid(true);
    return;
  }

  // |null| in JS maps to the empty string.
  if (aValue.isNull()) {
    _string.Truncate();
    return;
  }

  if (!AssignJSString(aCtx, _string, aValue.toString())) {
    _string.SetIsVoid(true);
  }
}

/**
 * Obtains the specified property of a JSObject.
 *
 * @param aCtx
 *        The JSContext for aObject.
 * @param aObject
 *        The JSObject to get the string from.
 * @param aProperty
 *        The property to get the value from.
 * @param _string
 *        The string to populate with the value, or set it to void.
 */
void
GetStringFromJSObject(JSContext* aCtx,
                      JS::Handle<JSObject *> aObject,
                      const char* aProperty,
                      nsString& _string)
{
  JS::Rooted<JS::Value> val(aCtx);
  bool rc = JS_GetProperty(aCtx, aObject, aProperty, &val);
  if (!rc) {
    _string.SetIsVoid(true);
    return;
  }
  else {
    GetJSValueAsString(aCtx, val, _string);
  }
}

/**
 * Obtains the specified property of a JSObject.
 *
 * @param aCtx
 *        The JSContext for aObject.
 * @param aObject
 *        The JSObject to get the int from.
 * @param aProperty
 *        The property to get the value from.
 * @param _int
 *        The integer to populate with the value on success.
 */
template <typename IntType>
nsresult
GetIntFromJSObject(JSContext* aCtx,
                   JS::Handle<JSObject *> aObject,
                   const char* aProperty,
                   IntType* _int)
{
  JS::Rooted<JS::Value> value(aCtx);
  bool rc = JS_GetProperty(aCtx, aObject, aProperty, &value);
  NS_ENSURE_TRUE(rc, NS_ERROR_UNEXPECTED);
  if (value.isUndefined()) {
    return NS_ERROR_INVALID_ARG;
  }
  NS_ENSURE_ARG(value.isPrimitive());
  NS_ENSURE_ARG(value.isNumber());

  double num;
  rc = JS::ToNumber(aCtx, value, &num);
  NS_ENSURE_TRUE(rc, NS_ERROR_UNEXPECTED);
  NS_ENSURE_ARG(IntType(num) == num);

  *_int = IntType(num);
  return NS_OK;
}

/**
 * Obtains the specified property of a JSObject.
 *
 * @pre aArray must be an Array object.
 *
 * @param aCtx
 *        The JSContext for aArray.
 * @param aArray
 *        The JSObject to get the object from.
 * @param aIndex
 *        The index to get the object from.
 * @param objOut
 *        Set to the JSObject pointer on success.
 */
nsresult
GetJSObjectFromArray(JSContext* aCtx,
                     JS::Handle<JSObject*> aArray,
                     uint32_t aIndex,
                     JS::MutableHandle<JSObject*> objOut)
{
  JS::Rooted<JS::Value> value(aCtx);
  bool rc = JS_GetElement(aCtx, aArray, aIndex, &value);
  NS_ENSURE_TRUE(rc, NS_ERROR_UNEXPECTED);
  NS_ENSURE_ARG(!value.isPrimitive());
  objOut.set(&value.toObject());
  return NS_OK;
}

class VisitedQuery final : public AsyncStatementCallback,
                           public mozIStorageCompletionCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  static nsresult Start(nsIURI* aURI,
                        mozIVisitedStatusCallback* aCallback=nullptr)
  {
    MOZ_ASSERT(aURI, "Null URI");

    // If we are a content process, always remote the request to the
    // parent process.
    if (XRE_IsContentProcess()) {
      URIParams uri;
      SerializeURI(aURI, uri);

      mozilla::dom::ContentChild* cpc =
        mozilla::dom::ContentChild::GetSingleton();
      NS_ASSERTION(cpc, "Content Protocol is NULL!");
      (void)cpc->SendStartVisitedQuery(uri);
      return NS_OK;
    }

    nsMainThreadPtrHandle<mozIVisitedStatusCallback>
      callback(new nsMainThreadPtrHolder<mozIVisitedStatusCallback>(
        "mozIVisitedStatusCallback", aCallback));

    nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
    NS_ENSURE_STATE(navHistory);
    if (navHistory->hasEmbedVisit(aURI)) {
      RefPtr<VisitedQuery> cb = new VisitedQuery(aURI, callback, true);
      NS_ENSURE_TRUE(cb, NS_ERROR_OUT_OF_MEMORY);
      // As per IHistory contract, we must notify asynchronously.
      NS_DispatchToMainThread(
        NewRunnableMethod("places::VisitedQuery::NotifyVisitedStatus",
                          cb,
                          &VisitedQuery::NotifyVisitedStatus));

      return NS_OK;
    }

    History* history = History::GetService();
    NS_ENSURE_STATE(history);
    RefPtr<VisitedQuery> cb = new VisitedQuery(aURI, callback);
    NS_ENSURE_TRUE(cb, NS_ERROR_OUT_OF_MEMORY);
    nsresult rv = history->GetIsVisitedStatement(cb);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  // Note: the return value matters here.  We call into this method, it's not
  // just xpcom boilerplate.
  NS_IMETHOD Complete(nsresult aResult, nsISupports* aStatement) override
  {
    NS_ENSURE_SUCCESS(aResult, aResult);
    nsCOMPtr<mozIStorageAsyncStatement> stmt = do_QueryInterface(aStatement);
    NS_ENSURE_STATE(stmt);
    // Bind by index for performance.
    nsresult rv = URIBinder::Bind(stmt, 0, mURI);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<mozIStoragePendingStatement> handle;
    return stmt->ExecuteAsync(this, getter_AddRefs(handle));
  }

  NS_IMETHOD HandleResult(mozIStorageResultSet* aResults) override
  {
    // If this method is called, we've gotten results, which means we have a
    // visit.
    mIsVisited = true;
    return NS_OK;
  }

  NS_IMETHOD HandleError(mozIStorageError* aError) override
  {
    // mIsVisited is already set to false, and that's the assumption we will
    // make if an error occurred.
    return NS_OK;
  }

  NS_IMETHOD HandleCompletion(uint16_t aReason) override
  {
    if (aReason != mozIStorageStatementCallback::REASON_FINISHED) {
      return NS_OK;
    }

    nsresult rv = NotifyVisitedStatus();
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  nsresult NotifyVisitedStatus()
  {
    // If an external handling callback is provided, just notify through it.
    if (!!mCallback) {
      mCallback->IsVisited(mURI, mIsVisited);
      return NS_OK;
    }

    if (mIsVisited) {
      History* history = History::GetService();
      NS_ENSURE_STATE(history);
      history->NotifyVisited(mURI);
      AutoTArray<URIParams, 1> uris;
      URIParams uri;
      SerializeURI(mURI, uri);
      uris.AppendElement(Move(uri));
      history->NotifyVisitedParent(uris);
    }

    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (observerService) {
      nsAutoString status;
      if (mIsVisited) {
        status.AssignLiteral(URI_VISITED);
      }
      else {
        status.AssignLiteral(URI_NOT_VISITED);
      }
      (void)observerService->NotifyObservers(mURI,
                                             URI_VISITED_RESOLUTION_TOPIC,
                                             status.get());
    }

    return NS_OK;
  }

private:
  explicit VisitedQuery(nsIURI* aURI,
                        const nsMainThreadPtrHandle<mozIVisitedStatusCallback>& aCallback,
                        bool aIsVisited=false)
  : mURI(aURI)
  , mCallback(aCallback)
  , mIsVisited(aIsVisited)
  {
  }

  ~VisitedQuery()
  {
  }

  nsCOMPtr<nsIURI> mURI;
  nsMainThreadPtrHandle<mozIVisitedStatusCallback> mCallback;
  bool mIsVisited;
};

NS_IMPL_ISUPPORTS_INHERITED(
  VisitedQuery
, AsyncStatementCallback
, mozIStorageCompletionCallback
)

/**
 * Notifies observers about a visit or an array of visits.
 */
class NotifyManyVisitsObservers : public Runnable
{
public:
  explicit NotifyManyVisitsObservers(const VisitData& aPlace)
    : Runnable("places::NotifyManyVisitsObservers")
    , mPlace(aPlace)
    , mHistory(History::GetService())
  {
  }

  explicit NotifyManyVisitsObservers(nsTArray<VisitData>& aPlaces)
    : Runnable("places::NotifyManyVisitsObservers")
    , mHistory(History::GetService())
  {
    aPlaces.SwapElements(mPlaces);
  }

  nsresult NotifyVisit(nsNavHistory* aNavHistory,
                       nsCOMPtr<nsIObserverService>& aObsService,
                       PRTime aNow,
                       nsIURI* aURI,
                       const VisitData& aPlace) {
    if (aObsService) {
      DebugOnly<nsresult> rv =
        aObsService->NotifyObservers(aURI, URI_VISIT_SAVED, nullptr);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Could not notify observers");
    }

    if (aNow - aPlace.visitTime < RECENTLY_VISITED_URIS_MAX_AGE) {
      mHistory->AppendToRecentlyVisitedURIs(aURI);
    }
    mHistory->NotifyVisited(aURI);

    if (aPlace.titleChanged) {
      aNavHistory->NotifyTitleChange(aURI, aPlace.title, aPlace.guid);
    }

    return NS_OK;
  }

  void AddPlaceForNotify(const VisitData& aPlace,
                         nsIURI* aURI,
                         nsCOMArray<nsIVisitData>& aPlaces) {
    if (aPlace.transitionType != nsINavHistoryService::TRANSITION_EMBED) {
      nsCOMPtr<nsIVisitData> notifyPlace = new nsVisitData(
        aURI, aPlace.visitId, aPlace.visitTime,
        aPlace.referrerVisitId, aPlace.transitionType,
        aPlace.guid, aPlace.hidden,
        aPlace.visitCount + 1, // Add current visit.
        static_cast<uint32_t>(aPlace.typed),
        aPlace.title);
      aPlaces.AppendElement(notifyPlace.forget());
    }
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");

    // We are in the main thread, no need to lock.
    if (mHistory->IsShuttingDown()) {
      // If we are shutting down, we cannot notify the observers.
      return NS_OK;
    }

    nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
    if (!navHistory) {
      NS_WARNING("Trying to notify visits observers but cannot get the history service!");
      return NS_OK;
    }

    nsCOMPtr<nsIObserverService> obsService =
      mozilla::services::GetObserverService();

    nsCOMArray<nsIVisitData> places;
    nsCOMArray<nsIURI> uris;
    if (mPlaces.Length() > 0) {
      for (uint32_t i = 0; i < mPlaces.Length(); ++i) {
        nsCOMPtr<nsIURI> uri;
        MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(uri), mPlaces[i].spec));
        if (!uri) {
          return NS_ERROR_UNEXPECTED;
        }
        AddPlaceForNotify(mPlaces[i], uri, places);
        uris.AppendElement(uri.forget());
      }
    } else {
      nsCOMPtr<nsIURI> uri;
      MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(uri), mPlace.spec));
      if (!uri) {
        return NS_ERROR_UNEXPECTED;
      }
      AddPlaceForNotify(mPlace, uri, places);
      uris.AppendElement(uri.forget());
    }
    if (places.Length() > 0) {
      navHistory->NotifyOnVisits(places.Elements(), places.Length());
    }

    PRTime now = PR_Now();
    if (mPlaces.Length() > 0) {
      InfallibleTArray<URIParams> serializableUris(mPlaces.Length());
      for (uint32_t i = 0; i < mPlaces.Length(); ++i) {
        nsresult rv = NotifyVisit(navHistory, obsService, now, uris[i], mPlaces[i]);
        NS_ENSURE_SUCCESS(rv, rv);

        URIParams serializedUri;
        SerializeURI(uris[i], serializedUri);
        serializableUris.AppendElement(Move(serializedUri));
      }
      mHistory->NotifyVisitedParent(serializableUris);
    } else {
      AutoTArray<URIParams, 1> serializableUris;
      nsresult rv = NotifyVisit(navHistory, obsService, now, uris[0], mPlace);
      NS_ENSURE_SUCCESS(rv, rv);

      URIParams serializedUri;
      SerializeURI(uris[0], serializedUri);
      serializableUris.AppendElement(Move(serializedUri));
      mHistory->NotifyVisitedParent(serializableUris);
    }

    return NS_OK;
  }
private:
  nsTArray<VisitData> mPlaces;
  VisitData mPlace;
  RefPtr<History> mHistory;
};

/**
 * Notifies observers about a pages title changing.
 */
class NotifyTitleObservers : public Runnable
{
public:
  /**
   * Notifies observers on the main thread.
   *
   * @param aSpec
   *        The spec of the URI to notify about.
   * @param aTitle
   *        The new title to notify about.
   */
  NotifyTitleObservers(const nsCString& aSpec,
                       const nsString& aTitle,
                       const nsCString& aGUID)
    : Runnable("places::NotifyTitleObservers")
    , mSpec(aSpec)
    , mTitle(aTitle)
    , mGUID(aGUID)
  {
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");

    nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(navHistory, NS_ERROR_OUT_OF_MEMORY);
    nsCOMPtr<nsIURI> uri;
    MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(uri), mSpec));
    if (!uri) {
      return NS_ERROR_UNEXPECTED;
    }

    navHistory->NotifyTitleChange(uri, mTitle, mGUID);

    return NS_OK;
  }
private:
  const nsCString mSpec;
  const nsString mTitle;
  const nsCString mGUID;
};

/**
 * Helper class for methods which notify their callers through the
 * mozIVisitInfoCallback interface.
 */
class NotifyPlaceInfoCallback : public Runnable
{
public:
  NotifyPlaceInfoCallback(
    const nsMainThreadPtrHandle<mozIVisitInfoCallback>& aCallback,
    const VisitData& aPlace,
    bool aIsSingleVisit,
    nsresult aResult)
    : Runnable("places::NotifyPlaceInfoCallback")
    , mCallback(aCallback)
    , mPlace(aPlace)
    , mResult(aResult)
    , mIsSingleVisit(aIsSingleVisit)
  {
    MOZ_ASSERT(aCallback, "Must pass a non-null callback!");
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");

    bool hasValidURIs = true;
    nsCOMPtr<nsIURI> referrerURI;
    if (!mPlace.referrerSpec.IsEmpty()) {
      MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(referrerURI), mPlace.referrerSpec));
      hasValidURIs = !!referrerURI;
    }

    nsCOMPtr<nsIURI> uri;
    MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(uri), mPlace.spec));
    hasValidURIs = hasValidURIs && !!uri;

    nsCOMPtr<mozIPlaceInfo> place;
    if (mIsSingleVisit) {
      nsCOMPtr<mozIVisitInfo> visit =
        new VisitInfo(mPlace.visitId, mPlace.visitTime, mPlace.transitionType,
                      referrerURI.forget());
      PlaceInfo::VisitsArray visits;
      (void)visits.AppendElement(visit);

      // The frecency isn't exposed because it may not reflect the updated value
      // in the case of InsertVisitedURIs.
      place =
        new PlaceInfo(mPlace.placeId, mPlace.guid, uri.forget(), mPlace.title,
                      -1, visits);
    }
    else {
      // Same as above.
      place =
        new PlaceInfo(mPlace.placeId, mPlace.guid, uri.forget(), mPlace.title,
                      -1);
    }

    if (NS_SUCCEEDED(mResult) && hasValidURIs) {
      (void)mCallback->HandleResult(place);
    } else {
      (void)mCallback->HandleError(mResult, place);
    }

    return NS_OK;
  }

private:
  nsMainThreadPtrHandle<mozIVisitInfoCallback> mCallback;
  VisitData mPlace;
  const nsresult mResult;
  bool mIsSingleVisit;
};

/**
 * Notifies a callback object when the operation is complete.
 */
class NotifyCompletion : public Runnable
{
public:
  explicit NotifyCompletion(
    const nsMainThreadPtrHandle<mozIVisitInfoCallback>& aCallback,
    uint32_t aUpdatedCount = 0)
    : Runnable("places::NotifyCompletion")
    , mCallback(aCallback)
    , mUpdatedCount(aUpdatedCount)
  {
    MOZ_ASSERT(aCallback, "Must pass a non-null callback!");
  }

  NS_IMETHOD Run() override
  {
    if (NS_IsMainThread()) {
      (void)mCallback->HandleCompletion(mUpdatedCount);
    }
    else {
      (void)NS_DispatchToMainThread(this);
    }
    return NS_OK;
  }

private:
  nsMainThreadPtrHandle<mozIVisitInfoCallback> mCallback;
  uint32_t mUpdatedCount;
};

/**
 * Checks to see if we can add aURI to history, and dispatches an error to
 * aCallback (if provided) if we cannot.
 *
 * @param aURI
 *        The URI to check.
 * @param [optional] aGUID
 *        The guid of the URI to check.  This is passed back to the callback.
 * @param [optional] aCallback
 *        The callback to notify if the URI cannot be added to history.
 * @return true if the URI can be added to history, false otherwise.
 */
bool
CanAddURI(nsIURI* aURI,
          const nsCString& aGUID = EmptyCString(),
          mozIVisitInfoCallback* aCallback = nullptr)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(navHistory, false);

  bool canAdd;
  nsresult rv = navHistory->CanAddURI(aURI, &canAdd);
  if (NS_SUCCEEDED(rv) && canAdd) {
    return true;
  };

  // We cannot add the URI.  Notify the callback, if we were given one.
  if (aCallback) {
    VisitData place(aURI);
    place.guid = aGUID;
    nsMainThreadPtrHandle<mozIVisitInfoCallback>
      callback(new nsMainThreadPtrHolder<mozIVisitInfoCallback>(
        "mozIVisitInfoCallback", aCallback));
    nsCOMPtr<nsIRunnable> event =
      new NotifyPlaceInfoCallback(callback, place, true, NS_ERROR_INVALID_ARG);
    (void)NS_DispatchToMainThread(event);
  }

  return false;
}

class NotifyManyFrecenciesChanged final : public Runnable
{
public:
  NotifyManyFrecenciesChanged()
    : Runnable("places::NotifyManyFrecenciesChanged")
  {
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");
    nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
    NS_ENSURE_STATE(navHistory);
    navHistory->NotifyManyFrecenciesChanged();
    return NS_OK;
  }
};


/**
 * Adds a visit to the database.
 */
class InsertVisitedURIs final: public Runnable
{
public:
  /**
   * Adds a visit to the database asynchronously.
   *
   * @param aConnection
   *        The database connection to use for these operations.
   * @param aPlaces
   *        The locations to record visits.
   * @param [optional] aCallback
   *        The callback to notify about the visit.
   * @param [optional] aGroupNotifications
   *        Whether to group any observer notifications rather than
   *        sending them out individually.
   */
  static nsresult Start(mozIStorageConnection* aConnection,
                        nsTArray<VisitData>& aPlaces,
                        mozIVisitInfoCallback* aCallback = nullptr,
                        bool aGroupNotifications = false,
                        uint32_t aInitialUpdatedCount = 0)
  {
    MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");
    MOZ_ASSERT(aPlaces.Length() > 0, "Must pass a non-empty array!");

    // Make sure nsNavHistory service is up before proceeding:
    nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
    MOZ_ASSERT(navHistory, "Could not get nsNavHistory?!");
    if (!navHistory) {
      return NS_ERROR_FAILURE;
    }

    nsMainThreadPtrHandle<mozIVisitInfoCallback>
      callback(new nsMainThreadPtrHolder<mozIVisitInfoCallback>(
        "mozIVisitInfoCallback", aCallback));
    bool ignoreErrors = false, ignoreResults = false;
    if (aCallback) {
      // We ignore errors from either of these methods in case old JS consumers
      // don't implement them (in which case they will get error/result
      // notifications as normal).
      Unused << aCallback->GetIgnoreErrors(&ignoreErrors);
      Unused << aCallback->GetIgnoreResults(&ignoreResults);
    }
    RefPtr<InsertVisitedURIs> event =
      new InsertVisitedURIs(aConnection, aPlaces, callback, aGroupNotifications,
                            ignoreErrors, ignoreResults, aInitialUpdatedCount);

    // Get the target thread, and then start the work!
    nsCOMPtr<nsIEventTarget> target = do_GetInterface(aConnection);
    NS_ENSURE_TRUE(target, NS_ERROR_UNEXPECTED);
    nsresult rv = target->Dispatch(event, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(!NS_IsMainThread(), "This should not be called on the main thread");

    // The inner run method may bail out at any point, so we ensure we do
    // whatever we can and then notify the main thread we're done.
    nsresult rv = InnerRun();

    if (mSuccessfulUpdatedCount > 0 && mGroupNotifications) {
      NS_DispatchToMainThread(new NotifyManyFrecenciesChanged());
    }
    if (!!mCallback) {
      NS_DispatchToMainThread(new NotifyCompletion(mCallback, mSuccessfulUpdatedCount));
    }
    return rv;
  }

  nsresult InnerRun() {
    // Prevent the main thread from shutting down while this is running.
    MutexAutoLock lockedScope(mHistory->GetShutdownMutex());
    if (mHistory->IsShuttingDown()) {
      // If we were already shutting down, we cannot insert the URIs.
      return NS_OK;
    }

    mozStorageTransaction transaction(mDBConn, false,
                                      mozIStorageConnection::TRANSACTION_IMMEDIATE);

    const VisitData* lastFetchedPlace = nullptr;
    uint32_t lastFetchedVisitCount = 0;
    bool shouldChunkNotifications = mPlaces.Length() > NOTIFY_VISITS_CHUNK_SIZE;
    InfallibleTArray<VisitData> notificationChunk;
    if (shouldChunkNotifications) {
      notificationChunk.SetCapacity(NOTIFY_VISITS_CHUNK_SIZE);
    }
    for (nsTArray<VisitData>::size_type i = 0; i < mPlaces.Length(); i++) {
      VisitData& place = mPlaces.ElementAt(i);

      // Fetching from the database can overwrite this information, so save it
      // apart.
      bool typed = place.typed;
      bool hidden = place.hidden;

      // We can avoid a database lookup if it's the same place as the last
      // visit we added.
      bool known = lastFetchedPlace && lastFetchedPlace->spec.Equals(place.spec);
      if (!known) {
        nsresult rv = mHistory->FetchPageInfo(place, &known);
        if (NS_FAILED(rv)) {
          if (!!mCallback && !mIgnoreErrors) {
            nsCOMPtr<nsIRunnable> event =
              new NotifyPlaceInfoCallback(mCallback, place, true, rv);
            return NS_DispatchToMainThread(event);
          }
          return NS_OK;
        }
        lastFetchedPlace = &mPlaces.ElementAt(i);
        lastFetchedVisitCount = lastFetchedPlace->visitCount;
      } else {
        // Copy over the data from the already known place.
        place.placeId = lastFetchedPlace->placeId;
        place.guid = lastFetchedPlace->guid;
        place.lastVisitId = lastFetchedPlace->visitId;
        place.lastVisitTime = lastFetchedPlace->visitTime;
        if (!place.title.IsVoid()) {
          place.titleChanged = !lastFetchedPlace->title.Equals(place.title);
        }
        place.frecency = lastFetchedPlace->frecency;
        // Add one visit for the previous loop.
        place.visitCount = ++lastFetchedVisitCount;
      }

      // If any transition is typed, ensure the page is marked as typed.
      if (typed != lastFetchedPlace->typed) {
        place.typed = true;
      }

      // If any transition is visible, ensure the page is marked as visible.
      if (hidden != lastFetchedPlace->hidden) {
        place.hidden = false;
      }

      // If this is a new page, or the existing page was already visible,
      // there's no need to try to unhide it.
      if (!known || !lastFetchedPlace->hidden) {
        place.shouldUpdateHidden = false;
      }

      FetchReferrerInfo(place);

      nsresult rv = DoDatabaseInserts(known, place);
      if (!!mCallback) {
        // Check if consumers wanted to be notified about success/failure,
        // depending on whether this action succeeded or not.
        if ((NS_SUCCEEDED(rv) && !mIgnoreResults) ||
            (NS_FAILED(rv) && !mIgnoreErrors)) {
          nsCOMPtr<nsIRunnable> event =
            new NotifyPlaceInfoCallback(mCallback, place, true, rv);
          nsresult rv2 = NS_DispatchToMainThread(event);
          NS_ENSURE_SUCCESS(rv2, rv2);
        }
      }
      NS_ENSURE_SUCCESS(rv, rv);

      if (shouldChunkNotifications) {
        int32_t numRemaining = mPlaces.Length() - (i + 1);
        notificationChunk.AppendElement(place);
        if (notificationChunk.Length() == NOTIFY_VISITS_CHUNK_SIZE ||
            numRemaining == 0) {
          // This will SwapElements on notificationChunk with an empty nsTArray
          nsCOMPtr<nsIRunnable> event = new NotifyManyVisitsObservers(notificationChunk);
          rv = NS_DispatchToMainThread(event);
          NS_ENSURE_SUCCESS(rv, rv);

          int32_t nextCapacity = std::min(NOTIFY_VISITS_CHUNK_SIZE, numRemaining);
          notificationChunk.SetCapacity(nextCapacity);
        }
      }

      // If we get here, we must have been successful adding/updating this
      // visit/place, so update the count:
      mSuccessfulUpdatedCount++;
    }

    {
      // Trigger an update for all the hosts of the places we inserted
      nsAutoCString query("DELETE FROM moz_updatehostsinsert_temp");
      nsCOMPtr<mozIStorageStatement> stmt = mHistory->GetStatement(query);
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scoper(stmt);
      nsresult rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsresult rv = transaction.Commit();
    NS_ENSURE_SUCCESS(rv, rv);

    // If we don't need to chunk the notifications, just notify using the
    // original mPlaces array.
    if (!shouldChunkNotifications) {
      nsCOMPtr<nsIRunnable> event = new NotifyManyVisitsObservers(mPlaces);
      rv = NS_DispatchToMainThread(event);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
  }
private:
  InsertVisitedURIs(
    mozIStorageConnection* aConnection,
    nsTArray<VisitData>& aPlaces,
    const nsMainThreadPtrHandle<mozIVisitInfoCallback>& aCallback,
    bool aGroupNotifications,
    bool aIgnoreErrors,
    bool aIgnoreResults,
    uint32_t aInitialUpdatedCount)
    : Runnable("places::InsertVisitedURIs")
    , mDBConn(aConnection)
    , mCallback(aCallback)
    , mGroupNotifications(aGroupNotifications)
    , mIgnoreErrors(aIgnoreErrors)
    , mIgnoreResults(aIgnoreResults)
    , mSuccessfulUpdatedCount(aInitialUpdatedCount)
    , mHistory(History::GetService())
  {
    MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");

    mPlaces.SwapElements(aPlaces);

#ifdef DEBUG
    for (nsTArray<VisitData>::size_type i = 0; i < mPlaces.Length(); i++) {
      nsCOMPtr<nsIURI> uri;
      MOZ_ASSERT(NS_SUCCEEDED(NS_NewURI(getter_AddRefs(uri), mPlaces[i].spec)));
      MOZ_ASSERT(CanAddURI(uri),
                 "Passed a VisitData with a URI we cannot add to history!");
    }
#endif
  }

  /**
   * Inserts or updates the entry in moz_places for this visit, adds the visit,
   * and updates the frecency of the place.
   *
   * @param aKnown
   *        True if we already have an entry for this place in moz_places, false
   *        otherwise.
   * @param aPlace
   *        The place we are adding a visit for.
   */
  nsresult DoDatabaseInserts(bool aKnown,
                             VisitData& aPlace)
  {
    MOZ_ASSERT(!NS_IsMainThread(), "This should not be called on the main thread");

    // If the page was in moz_places, we need to update the entry.
    nsresult rv;
    if (aKnown) {
      rv = mHistory->UpdatePlace(aPlace);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    // Otherwise, the page was not in moz_places, so now we have to add it.
    else {
      rv = mHistory->InsertPlace(aPlace, !mGroupNotifications);
      NS_ENSURE_SUCCESS(rv, rv);
      aPlace.placeId = nsNavHistory::sLastInsertedPlaceId;
    }
    MOZ_ASSERT(aPlace.placeId > 0);

    rv = AddVisit(aPlace);
    NS_ENSURE_SUCCESS(rv, rv);

    // TODO (bug 623969) we shouldn't update this after each visit, but
    // rather only for each unique place to save disk I/O.

    // Don't update frecency if the page should not appear in autocomplete.
    if (aPlace.shouldUpdateFrecency) {
      rv = UpdateFrecency(aPlace);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
  }

  /**
   * Fetches information about a referrer for aPlace if it was a recent
   * visit or not.
   *
   * @param aPlace
   *        The VisitData for the visit we will eventually add.
   *
   */
  void FetchReferrerInfo(VisitData& aPlace)
  {
    if (aPlace.referrerSpec.IsEmpty()) {
      return;
    }

    VisitData referrer;
    referrer.spec = aPlace.referrerSpec;
    // If the referrer is the same as the page, we don't need to fetch it.
    if (aPlace.referrerSpec.Equals(aPlace.spec)) {
      referrer = aPlace;
      // The page last visit id is also the referrer visit id.
      aPlace.referrerVisitId = aPlace.lastVisitId;
    } else {
      bool exists = false;
      if (NS_SUCCEEDED(mHistory->FetchPageInfo(referrer, &exists)) && exists) {
        // Copy the referrer last visit id.
        aPlace.referrerVisitId = referrer.lastVisitId;
      }
    }

    // Check if the page has effectively been visited recently, otherwise
    // discard the referrer info.
    if (!aPlace.referrerVisitId || !referrer.lastVisitTime ||
        aPlace.visitTime - referrer.lastVisitTime > RECENT_EVENT_THRESHOLD) {
      // We will not be using the referrer data.
      aPlace.referrerSpec.Truncate();
      aPlace.referrerVisitId = 0;
    }
  }

  /**
   * Adds a visit for _place and updates it with the right visit id.
   *
   * @param _place
   *        The VisitData for the place we need to know visit information about.
   */
  nsresult AddVisit(VisitData& _place)
  {
    MOZ_ASSERT(_place.placeId > 0);

    nsresult rv;
    nsCOMPtr<mozIStorageStatement> stmt;
    stmt = mHistory->GetStatement(
      "INSERT INTO moz_historyvisits "
        "(from_visit, place_id, visit_date, visit_type, session) "
      "VALUES (:from_visit, :page_id, :visit_date, :visit_type, 0) "
    );
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);

    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), _place.placeId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("from_visit"),
                               _place.referrerVisitId);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("visit_date"),
                               _place.visitTime);
    NS_ENSURE_SUCCESS(rv, rv);
    uint32_t transitionType = _place.transitionType;
    MOZ_ASSERT(transitionType >= nsINavHistoryService::TRANSITION_LINK &&
               transitionType <= nsINavHistoryService::TRANSITION_RELOAD,
                 "Invalid transition type!");
    rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("visit_type"),
                               transitionType);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    _place.visitId = nsNavHistory::sLastInsertedVisitId;
    MOZ_ASSERT(_place.visitId > 0);

    return NS_OK;
  }

  /**
   * Updates the frecency, and possibly the hidden-ness of aPlace.
   *
   * @param aPlace
   *        The VisitData for the place we want to update.
   */
  nsresult UpdateFrecency(const VisitData& aPlace)
  {
    MOZ_ASSERT(aPlace.shouldUpdateFrecency);
    MOZ_ASSERT(aPlace.placeId > 0);

    nsresult rv;
    { // First, set our frecency to the proper value.
      nsCOMPtr<mozIStorageStatement> stmt;
      if (!mGroupNotifications) {
        // If we're notifying for individual frecency updates, use
        // the notify_frecency sql function which will call us back.
        stmt = mHistory->GetStatement(
          "UPDATE moz_places "
          "SET frecency = NOTIFY_FRECENCY("
            "CALCULATE_FRECENCY(:page_id, :redirect), "
            "url, guid, hidden, last_visit_date"
          ") "
          "WHERE id = :page_id"
        );
      } else {
        // otherwise, just update the frecency without notifying.
        stmt = mHistory->GetStatement(
          "UPDATE moz_places "
          "SET frecency = CALCULATE_FRECENCY(:page_id, :redirect) "
          "WHERE id = :page_id"
        );
      }
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scoper(stmt);

      rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), aPlace.placeId);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("redirect"), aPlace.redirect);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (!aPlace.hidden && aPlace.shouldUpdateHidden) {
      // Mark the page as not hidden if the frecency is now nonzero.
      nsCOMPtr<mozIStorageStatement> stmt;
      stmt = mHistory->GetStatement(
        "UPDATE moz_places "
        "SET hidden = 0 "
        "WHERE id = :page_id AND frecency <> 0"
      );
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scoper(stmt);

      rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), aPlace.placeId);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
  }

  mozIStorageConnection* mDBConn;

  nsTArray<VisitData> mPlaces;

  nsMainThreadPtrHandle<mozIVisitInfoCallback> mCallback;

  bool mGroupNotifications;

  bool mIgnoreErrors;

  bool mIgnoreResults;

  uint32_t mSuccessfulUpdatedCount;

  /**
   * Strong reference to the History object because we do not want it to
   * disappear out from under us.
   */
  RefPtr<History> mHistory;
};

/**
 * Sets the page title for a page in moz_places (if necessary).
 */
class SetPageTitle : public Runnable
{
public:
  /**
   * Sets a pages title in the database asynchronously.
   *
   * @param aConnection
   *        The database connection to use for this operation.
   * @param aURI
   *        The URI to set the page title on.
   * @param aTitle
   *        The title to set for the page, if the page exists.
   */
  static nsresult Start(mozIStorageConnection* aConnection,
                        nsIURI* aURI,
                        const nsAString& aTitle)
  {
    MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");
    MOZ_ASSERT(aURI, "Must pass a non-null URI object!");

    nsCString spec;
    nsresult rv = aURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);

    RefPtr<SetPageTitle> event = new SetPageTitle(spec, aTitle);

    // Get the target thread, and then start the work!
    nsCOMPtr<nsIEventTarget> target = do_GetInterface(aConnection);
    NS_ENSURE_TRUE(target, NS_ERROR_UNEXPECTED);
    rv = target->Dispatch(event, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(!NS_IsMainThread(), "This should not be called on the main thread");

    // First, see if the page exists in the database (we'll need its id later).
    bool exists;
    nsresult rv = mHistory->FetchPageInfo(mPlace, &exists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!exists || !mPlace.titleChanged) {
      // We have no record of this page, or we have no title change, so there
      // is no need to do any further work.
      return NS_OK;
    }

    MOZ_ASSERT(mPlace.placeId > 0,
               "We somehow have an invalid place id here!");

    // Now we can update our database record.
    nsCOMPtr<mozIStorageStatement> stmt =
      mHistory->GetStatement(
        "UPDATE moz_places "
        "SET title = :page_title "
        "WHERE id = :page_id "
      );
    NS_ENSURE_STATE(stmt);

    {
      mozStorageStatementScoper scoper(stmt);
      rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), mPlace.placeId);
      NS_ENSURE_SUCCESS(rv, rv);
      // Empty strings should clear the title, just like
      // nsNavHistory::SetPageTitle.
      if (mPlace.title.IsEmpty()) {
        rv = stmt->BindNullByName(NS_LITERAL_CSTRING("page_title"));
      }
      else {
        rv = stmt->BindStringByName(NS_LITERAL_CSTRING("page_title"),
                                    StringHead(mPlace.title, TITLE_LENGTH_MAX));
      }
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsCOMPtr<nsIRunnable> event =
      new NotifyTitleObservers(mPlace.spec, mPlace.title, mPlace.guid);
    rv = NS_DispatchToMainThread(event);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

private:
  SetPageTitle(const nsCString& aSpec, const nsAString& aTitle)
    : Runnable("places::SetPageTitle")
    , mHistory(History::GetService())
  {
    mPlace.spec = aSpec;
    mPlace.title = aTitle;
  }

  VisitData mPlace;

  /**
   * Strong reference to the History object because we do not want it to
   * disappear out from under us.
   */
  RefPtr<History> mHistory;
};

/**
 * Adds download-specific annotations to a download page.
 */
class SetDownloadAnnotations final : public mozIVisitInfoCallback
{
public:
  NS_DECL_ISUPPORTS

  explicit SetDownloadAnnotations(nsIURI* aDestination)
  : mDestination(aDestination)
  , mHistory(History::GetService())
  {
    MOZ_ASSERT(mDestination);
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD GetIgnoreResults(bool *aIgnoreResults) override
  {
    *aIgnoreResults = false;
    return NS_OK;
  }

  NS_IMETHOD GetIgnoreErrors(bool *aIgnoreErrors) override
  {
    *aIgnoreErrors = false;
    return NS_OK;
  }

  NS_IMETHOD HandleError(nsresult aResultCode, mozIPlaceInfo *aPlaceInfo) override
  {
    // Just don't add the annotations in case the visit isn't added.
    return NS_OK;
  }

  NS_IMETHOD HandleResult(mozIPlaceInfo *aPlaceInfo) override
  {
    // Exit silently if the download destination is not a local file.
    nsCOMPtr<nsIFileURL> destinationFileURL = do_QueryInterface(mDestination);
    if (!destinationFileURL) {
      return NS_OK;
    }

    nsCOMPtr<nsIURI> source;
    nsresult rv = aPlaceInfo->GetUri(getter_AddRefs(source));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString destinationURISpec;
    rv = destinationFileURL->GetSpec(destinationURISpec);
    NS_ENSURE_SUCCESS(rv, rv);

    // Use annotations for storing the additional download metadata.
    nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
    NS_ENSURE_TRUE(annosvc, NS_ERROR_OUT_OF_MEMORY);

    rv = annosvc->SetPageAnnotationString(
      source,
      DESTINATIONFILEURI_ANNO,
      NS_ConvertUTF8toUTF16(destinationURISpec),
      0,
      nsIAnnotationService::EXPIRE_WITH_HISTORY
    );
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString title;
    rv = aPlaceInfo->GetTitle(title);
    NS_ENSURE_SUCCESS(rv, rv);

    // In case we are downloading a file that does not correspond to a web
    // page for which the title is present, we populate the otherwise empty
    // history title with the name of the destination file, to allow it to be
    // visible and searchable in history results.
    if (title.IsEmpty()) {
      nsCOMPtr<nsIFile> destinationFile;
      rv = destinationFileURL->GetFile(getter_AddRefs(destinationFile));
      NS_ENSURE_SUCCESS(rv, rv);

      nsAutoString destinationFileName;
      rv = destinationFile->GetLeafName(destinationFileName);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = mHistory->SetURITitle(source, destinationFileName);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
  }

  NS_IMETHOD HandleCompletion(uint32_t aUpdatedCount) override
  {
    return NS_OK;
  }

private:
  ~SetDownloadAnnotations() {}

  nsCOMPtr<nsIURI> mDestination;

  /**
   * Strong reference to the History object because we do not want it to
   * disappear out from under us.
   */
  RefPtr<History> mHistory;
};
NS_IMPL_ISUPPORTS(
  SetDownloadAnnotations,
  mozIVisitInfoCallback
)

/**
 * Notify removed visits to observers.
 */
class NotifyRemoveVisits : public Runnable
{
public:
  explicit NotifyRemoveVisits(nsTHashtable<PlaceHashKey>& aPlaces)
    : Runnable("places::NotifyRemoveVisits")
    , mPlaces(VISITS_REMOVAL_INITIAL_HASH_LENGTH)
    , mHistory(History::GetService())
  {
    MOZ_ASSERT(!NS_IsMainThread(),
               "This should not be called on the main thread");
    for (auto iter = aPlaces.Iter(); !iter.Done(); iter.Next()) {
      PlaceHashKey* entry = iter.Get();
      PlaceHashKey* copy = mPlaces.PutEntry(entry->GetKey());
      copy->SetProperties(entry->VisitCount(), entry->IsBookmarked());
      entry->mVisits.SwapElements(copy->mVisits);
    }
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");

    // We are in the main thread, no need to lock.
    if (mHistory->IsShuttingDown()) {
      // If we are shutting down, we cannot notify the observers.
      return NS_OK;
    }

    nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
    if (!navHistory) {
      NS_WARNING("Cannot notify without the history service!");
      return NS_OK;
    }

    // Wrap all notifications in a batch, so the view can handle changes in a
    // more performant way, by initiating a refresh after a limited number of
    // single changes.
    (void)navHistory->BeginUpdateBatch();
    for (auto iter = mPlaces.Iter(); !iter.Done(); iter.Next()) {
      PlaceHashKey* entry = iter.Get();
      const nsTArray<VisitData>& visits = entry->mVisits;
      nsCOMPtr<nsIURI> uri;
      MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(uri), visits[0].spec));
      // Notify an expiration only if we have a valid uri, otherwise
      // the observer couldn't gather any useful data from the notification.
      // This should be false only if there's a bug in the code preceding us.
      if (uri) {
        bool removingPage = visits.Length() == entry->VisitCount() &&
                            !entry->IsBookmarked();

        // FindRemovableVisits only sets the transition type on the VisitData
        // objects it collects if the visits were filtered by transition type.
        // RemoveVisitsFilter currently only supports filtering by transition
        // type, so FindRemovableVisits will either find all visits, or all
        // visits of a given type. Therefore, if transitionType is set on this
        // visit, we pass the transition type to NotifyOnPageExpired which in
        // turns passes it to OnDeleteVisits to indicate that all visits of a
        // given type were removed.
        uint32_t transition = visits[0].transitionType < UINT32_MAX
                            ? visits[0].transitionType
                            : 0;
        navHistory->NotifyOnPageExpired(uri, visits[0].visitTime, removingPage,
                                        visits[0].guid,
                                        nsINavHistoryObserver::REASON_DELETED,
                                        transition);
      }
    }
    (void)navHistory->EndUpdateBatch();

    return NS_OK;
  }

private:
  nsTHashtable<PlaceHashKey> mPlaces;

  /**
   * Strong reference to the History object because we do not want it to
   * disappear out from under us.
   */
  RefPtr<History> mHistory;
};

/**
 * Remove visits from history.
 */
class RemoveVisits : public Runnable
{
public:
  /**
   * Asynchronously removes visits from history.
   *
   * @param aConnection
   *        The database connection to use for these operations.
   * @param aFilter
   *        Filter to remove visits.
   */
  static nsresult Start(mozIStorageConnection* aConnection,
                        RemoveVisitsFilter& aFilter)
  {
    MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");

    RefPtr<RemoveVisits> event = new RemoveVisits(aConnection, aFilter);

    // Get the target thread, and then start the work!
    nsCOMPtr<nsIEventTarget> target = do_GetInterface(aConnection);
    NS_ENSURE_TRUE(target, NS_ERROR_UNEXPECTED);
    nsresult rv = target->Dispatch(event, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(!NS_IsMainThread(),
               "This should not be called on the main thread");

    // Prevent the main thread from shutting down while this is running.
    MutexAutoLock lockedScope(mHistory->GetShutdownMutex());
    if (mHistory->IsShuttingDown()) {
      // If we were already shutting down, we cannot remove the visits.
      return NS_OK;
    }

    // Find all the visits relative to the current filters and whether their
    // pages will be removed or not.
    nsTHashtable<PlaceHashKey> places(VISITS_REMOVAL_INITIAL_HASH_LENGTH);
    nsresult rv = FindRemovableVisits(places);
    NS_ENSURE_SUCCESS(rv, rv);

    if (places.Count() == 0)
      return NS_OK;

    mozStorageTransaction transaction(mDBConn, false,
                                      mozIStorageConnection::TRANSACTION_IMMEDIATE);

    rv = RemoveVisitsFromDatabase();
    NS_ENSURE_SUCCESS(rv, rv);
    rv = RemovePagesFromDatabase(places);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = transaction.Commit();
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIRunnable> event = new NotifyRemoveVisits(places);
    rv = NS_DispatchToMainThread(event);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

private:
  RemoveVisits(mozIStorageConnection* aConnection, RemoveVisitsFilter& aFilter)
    : Runnable("places::RemoveVisits")
    , mDBConn(aConnection)
    , mHasTransitionType(false)
    , mHistory(History::GetService())
  {
    MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");

    // Build query conditions.
    nsTArray<nsCString> conditions;
    // TODO: add support for binding params when adding further stuff here.
    if (aFilter.transitionType < UINT32_MAX) {
      conditions.AppendElement(nsPrintfCString("visit_type = %d", aFilter.transitionType));
      mHasTransitionType = true;
    }
    if (conditions.Length() > 0) {
      mWhereClause.AppendLiteral (" WHERE ");
      for (uint32_t i = 0; i < conditions.Length(); ++i) {
        if (i > 0)
          mWhereClause.AppendLiteral(" AND ");
        mWhereClause.Append(conditions[i]);
      }
    }
  }

  /**
   * Find the list of entries that may be removed from `moz_places`.
   *
   * Calling this method makes sense only if we are not clearing the entire history.
   */
  nsresult
  FindRemovableVisits(nsTHashtable<PlaceHashKey>& aPlaces)
  {
    MOZ_ASSERT(!NS_IsMainThread(),
               "This should not be called on the main thread");

    nsCString query("SELECT h.id, url, guid, visit_date, visit_type, "
                    "(SELECT count(*) FROM moz_historyvisits WHERE place_id = h.id) as full_visit_count, "
                    "EXISTS(SELECT 1 FROM moz_bookmarks WHERE fk = h.id) as bookmarked "
                    "FROM moz_historyvisits "
                    "JOIN moz_places h ON place_id = h.id");
    query.Append(mWhereClause);

    nsCOMPtr<mozIStorageStatement> stmt = mHistory->GetStatement(query);
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);

    bool hasResult;
    nsresult rv;
    while (NS_SUCCEEDED((rv = stmt->ExecuteStep(&hasResult))) && hasResult) {
      VisitData visit;
      rv = stmt->GetInt64(0, &visit.placeId);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->GetUTF8String(1, visit.spec);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->GetUTF8String(2, visit.guid);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->GetInt64(3, &visit.visitTime);
      NS_ENSURE_SUCCESS(rv, rv);
      if (mHasTransitionType) {
        int32_t transition;
        rv = stmt->GetInt32(4, &transition);
        NS_ENSURE_SUCCESS(rv, rv);
        visit.transitionType = static_cast<uint32_t>(transition);
      }
      int32_t visitCount, bookmarked;
      rv = stmt->GetInt32(5, &visitCount);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->GetInt32(6, &bookmarked);
      NS_ENSURE_SUCCESS(rv, rv);

      PlaceHashKey* entry = aPlaces.GetEntry(visit.spec);
      if (!entry) {
        entry = aPlaces.PutEntry(visit.spec);
      }
      entry->SetProperties(static_cast<uint32_t>(visitCount), static_cast<bool>(bookmarked));
      entry->mVisits.AppendElement(visit);
    }
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  nsresult
  RemoveVisitsFromDatabase()
  {
    MOZ_ASSERT(!NS_IsMainThread(),
               "This should not be called on the main thread");

    nsCString query("DELETE FROM moz_historyvisits");
    query.Append(mWhereClause);

    nsCOMPtr<mozIStorageStatement> stmt = mHistory->GetStatement(query);
    NS_ENSURE_STATE(stmt);
    mozStorageStatementScoper scoper(stmt);
    nsresult rv = stmt->Execute();
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  nsresult
  RemovePagesFromDatabase(nsTHashtable<PlaceHashKey>& aPlaces)
  {
    MOZ_ASSERT(!NS_IsMainThread(),
               "This should not be called on the main thread");

    nsCString placeIdsToRemove;
    for (auto iter = aPlaces.Iter(); !iter.Done(); iter.Next()) {
      PlaceHashKey* entry = iter.Get();
      const nsTArray<VisitData>& visits = entry->mVisits;
      // Only orphan ids should be listed.
      if (visits.Length() == entry->VisitCount() && !entry->IsBookmarked()) {
        if (!placeIdsToRemove.IsEmpty())
          placeIdsToRemove.Append(',');
        placeIdsToRemove.AppendInt(visits[0].placeId);
      }
    }

#ifdef DEBUG
    {
      // Ensure that we are not removing any problematic entry.
      nsCString query("SELECT id FROM moz_places h WHERE id IN (");
      query.Append(placeIdsToRemove);
      query.AppendLiteral(") AND ("
          "EXISTS(SELECT 1 FROM moz_bookmarks WHERE fk = h.id) OR "
          "EXISTS(SELECT 1 FROM moz_historyvisits WHERE place_id = h.id) OR "
          "SUBSTR(h.url, 1, 6) = 'place:' "
        ")");
      nsCOMPtr<mozIStorageStatement> stmt = mHistory->GetStatement(query);
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scoper(stmt);
      bool hasResult;
      MOZ_ASSERT(NS_SUCCEEDED(stmt->ExecuteStep(&hasResult)) && !hasResult,
                 "Trying to remove a non-oprhan place from the database");
    }
#endif

    {
      nsCString query("DELETE FROM moz_places "
                      "WHERE id IN (");
      query.Append(placeIdsToRemove);
      query.Append(')');

      nsCOMPtr<mozIStorageStatement> stmt = mHistory->GetStatement(query);
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scoper(stmt);
      nsresult rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    {
      // Hosts accumulated during the places delete are updated through a trigger
      // (see nsPlacesTriggers.h).
      nsAutoCString query("DELETE FROM moz_updatehostsdelete_temp");
      nsCOMPtr<mozIStorageStatement> stmt = mHistory->GetStatement(query);
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scoper(stmt);
      nsresult rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
  }

  mozIStorageConnection* mDBConn;
  bool mHasTransitionType;
  nsCString mWhereClause;

  /**
   * Strong reference to the History object because we do not want it to
   * disappear out from under us.
   */
  RefPtr<History> mHistory;
};

/**
 * Stores an embed visit, and notifies observers.
 *
 * @param aPlace
 *        The VisitData of the visit to store as an embed visit.
 * @param [optional] aCallback
 *        The mozIVisitInfoCallback to notify, if provided.
 */
void
StoreAndNotifyEmbedVisit(VisitData& aPlace,
                         mozIVisitInfoCallback* aCallback = nullptr)
{
  MOZ_ASSERT(aPlace.transitionType == nsINavHistoryService::TRANSITION_EMBED,
             "Must only pass TRANSITION_EMBED visits to this!");
  MOZ_ASSERT(NS_IsMainThread(), "Must be called on the main thread!");

  nsCOMPtr<nsIURI> uri;
  MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(uri), aPlace.spec));

  nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
  if (!navHistory || !uri) {
    return;
  }

  navHistory->registerEmbedVisit(uri, aPlace.visitTime);

  if (!!aCallback) {
    nsMainThreadPtrHandle<mozIVisitInfoCallback>
      callback(new nsMainThreadPtrHolder<mozIVisitInfoCallback>(
        "mozIVisitInfoCallback", aCallback));
    bool ignoreResults = false;
    Unused << aCallback->GetIgnoreResults(&ignoreResults);
    if (!ignoreResults) {
      nsCOMPtr<nsIRunnable> event =
        new NotifyPlaceInfoCallback(callback, aPlace, true, NS_OK);
      (void)NS_DispatchToMainThread(event);
    }
  }

  nsCOMPtr<nsIRunnable> event = new NotifyManyVisitsObservers(aPlace);
  (void)NS_DispatchToMainThread(event);
}

} // namespace

////////////////////////////////////////////////////////////////////////////////
//// History

History* History::gService = nullptr;

History::History()
  : mShuttingDown(false)
  , mShutdownMutex("History::mShutdownMutex")
  , mObservers(VISIT_OBSERVERS_INITIAL_CACHE_LENGTH)
  , mRecentlyVisitedURIs(RECENTLY_VISITED_URIS_SIZE)
{
  NS_ASSERTION(!gService, "Ruh-roh!  This service has already been created!");
  gService = this;

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  NS_WARNING_ASSERTION(os, "Observer service was not found!");
  if (os) {
    (void)os->AddObserver(this, TOPIC_PLACES_SHUTDOWN, false);
  }
}

History::~History()
{
  UnregisterWeakMemoryReporter(this);

  MOZ_ASSERT(gService == this);
  gService = nullptr;
}

void
History::InitMemoryReporter()
{
  RegisterWeakMemoryReporter(this);
}

// Helper function which performs the checking required to fetch the document
// object for the given link. May return null if the link does not have an owner
// document.
static nsIDocument*
GetLinkDocument(Link* aLink)
{
  // NOTE: Theoretically GetElement should never return nullptr, but it does
  // in GTests because they use a mock_Link which returns null from this
  // method.
  Element* element = aLink->GetElement();
  return element ? element->OwnerDoc() : nullptr;
}

void
History::NotifyVisitedParent(const nsTArray<URIParams>& aURIs)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  nsTArray<ContentParent*> cplist;
  ContentParent::GetAll(cplist);

  if (!cplist.IsEmpty()) {
    for (uint32_t i = 0; i < cplist.Length(); ++i) {
      Unused << cplist[i]->SendNotifyVisited(aURIs);
    }
  }
}

NS_IMETHODIMP
History::NotifyVisited(nsIURI* aURI)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aURI);
  // NOTE: This can be run within the SystemGroup, and thus cannot directly
  // interact with webpages.

  nsAutoScriptBlocker scriptBlocker;

  // If we have no observers for this URI, we have nothing to notify about.
  KeyClass* key = mObservers.GetEntry(aURI);
  if (!key) {
    return NS_OK;
  }
  key->mVisited = true;

  // If we have a key, it should have at least one observer.
  MOZ_ASSERT(!key->array.IsEmpty());

  // Dispatch an event to each document which has a Link observing this URL.
  // These will fire asynchronously in the correct DocGroup.
  {
    nsTArray<nsIDocument*> seen; // Don't dispatch duplicate runnables.
    ObserverArray::BackwardIterator iter(key->array);
    while (iter.HasMore()) {
      Link* link = iter.GetNext();
      nsIDocument* doc = GetLinkDocument(link);
      if (seen.Contains(doc)) {
        continue;
      }
      seen.AppendElement(doc);
      DispatchNotifyVisited(aURI, doc);
    }
  }

  return NS_OK;
}

void
History::NotifyVisitedForDocument(nsIURI* aURI, nsIDocument* aDocument)
{
  MOZ_ASSERT(NS_IsMainThread());
  // Make sure that nothing invalidates our observer array while we're walking
  // over it.
  nsAutoScriptBlocker scriptBlocker;

  // If we have no observers for this URI, we have nothing to notify about.
  KeyClass* key = mObservers.GetEntry(aURI);
  if (!key) {
    return;
  }

  {
    // Update status of each Link node. We iterate over the array backwards so
    // we can remove the items as we encounter them.
    ObserverArray::BackwardIterator iter(key->array);
    while (iter.HasMore()) {
      Link* link = iter.GetNext();
      nsIDocument* doc = GetLinkDocument(link);
      if (doc == aDocument) {
        link->SetLinkState(eLinkState_Visited);
        iter.Remove();
      }

      // Verify that the observers hash doesn't mutate while looping through
      // the links associated with this URI.
      MOZ_ASSERT(key == mObservers.GetEntry(aURI),
                 "The URIs hash mutated!");
    }
  }

  // If we don't have any links left, we can remove the array.
  if (key->array.IsEmpty()) {
    mObservers.RemoveEntry(key);
  }
}

void
History::DispatchNotifyVisited(nsIURI* aURI, nsIDocument* aDocument)
{
  // Capture strong references to the arguments to capture in the closure.
  nsCOMPtr<nsIDocument> doc = aDocument;
  nsCOMPtr<nsIURI> uri = aURI;

  // Create and dispatch the runnable to call NotifyVisitedForDocument.
  nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction("History::DispatchNotifyVisited", [uri, doc] {
    nsCOMPtr<IHistory> history = services::GetHistoryService();
    static_cast<History*>(history.get())->NotifyVisitedForDocument(uri, doc);
  });

  if (doc) {
    doc->Dispatch(TaskCategory::Other, runnable.forget());
  } else {
    NS_DispatchToMainThread(runnable.forget());
  }
}

class ConcurrentStatementsHolder final : public mozIStorageCompletionCallback {
public:
  NS_DECL_ISUPPORTS

  explicit ConcurrentStatementsHolder(mozIStorageConnection* aDBConn)
  : mShutdownWasInvoked(false)
  {
    DebugOnly<nsresult> rv = aDBConn->AsyncClone(true, this);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  NS_IMETHOD Complete(nsresult aStatus, nsISupports* aConnection) override {
    if (NS_FAILED(aStatus)) {
      return NS_OK;
    }
    mReadOnlyDBConn = do_QueryInterface(aConnection);
    // It's possible Shutdown was invoked before we were handed back the
    // cloned connection handle.
    if (mShutdownWasInvoked) {
      Shutdown();
      return NS_OK;
    }

    // Now we can create our cached statements.

    if (!mIsVisitedStatement) {
      (void)mReadOnlyDBConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
        "SELECT 1 FROM moz_places h "
        "WHERE url_hash = hash(?1) AND url = ?1 AND last_visit_date NOTNULL "
      ),  getter_AddRefs(mIsVisitedStatement));
      MOZ_ASSERT(mIsVisitedStatement);
      nsresult result = mIsVisitedStatement ? NS_OK : NS_ERROR_NOT_AVAILABLE;
      for (int32_t i = 0; i < mIsVisitedCallbacks.Count(); ++i) {
        DebugOnly<nsresult> rv;
        rv = mIsVisitedCallbacks[i]->Complete(result, mIsVisitedStatement);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
      }
      mIsVisitedCallbacks.Clear();
    }

    return NS_OK;
  }

  void GetIsVisitedStatement(mozIStorageCompletionCallback* aCallback)
  {
    if (mIsVisitedStatement) {
      DebugOnly<nsresult> rv;
      rv = aCallback->Complete(NS_OK, mIsVisitedStatement);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    } else {
      DebugOnly<bool> added = mIsVisitedCallbacks.AppendObject(aCallback);
      MOZ_ASSERT(added);
    }
  }

  void Shutdown() {
    mShutdownWasInvoked = true;
    if (mReadOnlyDBConn) {
      mIsVisitedCallbacks.Clear();
      DebugOnly<nsresult> rv;
      if (mIsVisitedStatement) {
        rv = mIsVisitedStatement->Finalize();
        MOZ_ASSERT(NS_SUCCEEDED(rv));
      }
      rv = mReadOnlyDBConn->AsyncClose(nullptr);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      mReadOnlyDBConn = nullptr;
    }
  }

private:
  ~ConcurrentStatementsHolder()
  {
  }

  nsCOMPtr<mozIStorageAsyncConnection> mReadOnlyDBConn;
  nsCOMPtr<mozIStorageAsyncStatement> mIsVisitedStatement;
  nsCOMArray<mozIStorageCompletionCallback> mIsVisitedCallbacks;
  bool mShutdownWasInvoked;
};

NS_IMPL_ISUPPORTS(
  ConcurrentStatementsHolder
, mozIStorageCompletionCallback
)

nsresult
History::GetIsVisitedStatement(mozIStorageCompletionCallback* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mShuttingDown)
    return NS_ERROR_NOT_AVAILABLE;

  if (!mConcurrentStatementsHolder) {
    mozIStorageConnection* dbConn = GetDBConn();
    NS_ENSURE_STATE(dbConn);
    mConcurrentStatementsHolder = new ConcurrentStatementsHolder(dbConn);
  }
  mConcurrentStatementsHolder->GetIsVisitedStatement(aCallback);
  return NS_OK;
}

nsresult
History::InsertPlace(VisitData& aPlace, bool aShouldNotifyFrecencyChanged)
{
  MOZ_ASSERT(aPlace.placeId == 0, "should not have a valid place id!");
  MOZ_ASSERT(!aPlace.shouldUpdateHidden, "We should not need to update hidden");
  MOZ_ASSERT(!NS_IsMainThread(), "must be called off of the main thread!");

  nsCOMPtr<mozIStorageStatement> stmt = GetStatement(
      "INSERT INTO moz_places "
        "(url, url_hash, title, rev_host, hidden, typed, frecency, guid) "
      "VALUES (:url, hash(:url), :title, :rev_host, :hidden, :typed, :frecency, :guid) "
    );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindStringByName(NS_LITERAL_CSTRING("rev_host"),
                                       aPlace.revHost);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("url"), aPlace.spec);
  NS_ENSURE_SUCCESS(rv, rv);
  nsString title = aPlace.title;
  // Empty strings should have no title, just like nsNavHistory::SetPageTitle.
  if (title.IsEmpty()) {
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("title"));
  }
  else {
    title.Assign(StringHead(aPlace.title, TITLE_LENGTH_MAX));
    rv = stmt->BindStringByName(NS_LITERAL_CSTRING("title"), title);
  }
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("typed"), aPlace.typed);
  NS_ENSURE_SUCCESS(rv, rv);
  // When inserting a page for a first visit that should not appear in
  // autocomplete, for example an error page, use a zero frecency.
  int32_t frecency = aPlace.shouldUpdateFrecency ? aPlace.frecency : 0;
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("frecency"), frecency);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("hidden"), aPlace.hidden);
  NS_ENSURE_SUCCESS(rv, rv);
  if (aPlace.guid.IsVoid()) {
    rv = GenerateGUID(aPlace.guid);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("guid"), aPlace.guid);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  // Post an onFrecencyChanged observer notification.
  if (aShouldNotifyFrecencyChanged) {
    const nsNavHistory* navHistory = nsNavHistory::GetConstHistoryService();
    NS_ENSURE_STATE(navHistory);
    navHistory->DispatchFrecencyChangedNotification(aPlace.spec, frecency,
                                                    aPlace.guid,
                                                    aPlace.hidden,
                                                    aPlace.visitTime);
  }

  return NS_OK;
}

nsresult
History::UpdatePlace(const VisitData& aPlace)
{
  MOZ_ASSERT(!NS_IsMainThread(), "must be called off of the main thread!");
  MOZ_ASSERT(aPlace.placeId > 0, "must have a valid place id!");
  MOZ_ASSERT(!aPlace.guid.IsVoid(), "must have a guid!");

  nsCOMPtr<mozIStorageStatement> stmt;
  bool titleIsVoid = aPlace.title.IsVoid();
  if (titleIsVoid) {
    // Don't change the title.
    stmt = GetStatement(
      "UPDATE moz_places "
      "SET hidden = :hidden, "
          "typed = :typed, "
          "guid = :guid "
      "WHERE id = :page_id "
    );
  } else {
    stmt = GetStatement(
      "UPDATE moz_places "
      "SET title = :title, "
          "hidden = :hidden, "
          "typed = :typed, "
          "guid = :guid "
      "WHERE id = :page_id "
    );
  }
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv;
  if (!titleIsVoid) {
    // An empty string clears the title.
    if (aPlace.title.IsEmpty()) {
      rv = stmt->BindNullByName(NS_LITERAL_CSTRING("title"));
    } else {
      rv = stmt->BindStringByName(NS_LITERAL_CSTRING("title"),
                                  StringHead(aPlace.title, TITLE_LENGTH_MAX));
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("typed"), aPlace.typed);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("hidden"), aPlace.hidden);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("guid"), aPlace.guid);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"),
                             aPlace.placeId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
History::FetchPageInfo(VisitData& _place, bool* _exists)
{
  MOZ_ASSERT(!_place.spec.IsEmpty() || !_place.guid.IsEmpty(), "must have either a non-empty spec or guid!");
  MOZ_ASSERT(!NS_IsMainThread(), "must be called off of the main thread!");

  nsresult rv;

  // URI takes precedence.
  nsCOMPtr<mozIStorageStatement> stmt;
  bool selectByURI = !_place.spec.IsEmpty();
  if (selectByURI) {
    stmt = GetStatement(
      "SELECT guid, id, title, hidden, typed, frecency, visit_count, last_visit_date, "
      "(SELECT id FROM moz_historyvisits "
       "WHERE place_id = h.id AND visit_date = h.last_visit_date) AS last_visit_id "
      "FROM moz_places h "
      "WHERE url_hash = hash(:page_url) AND url = :page_url "
    );
    NS_ENSURE_STATE(stmt);

    rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), _place.spec);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    stmt = GetStatement(
      "SELECT url, id, title, hidden, typed, frecency, visit_count, last_visit_date, "
      "(SELECT id FROM moz_historyvisits "
       "WHERE place_id = h.id AND visit_date = h.last_visit_date) AS last_visit_id "
      "FROM moz_places h "
      "WHERE guid = :guid "
    );
    NS_ENSURE_STATE(stmt);

    rv = stmt->BindUTF8StringByName(NS_LITERAL_CSTRING("guid"), _place.guid);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mozStorageStatementScoper scoper(stmt);

  rv = stmt->ExecuteStep(_exists);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!*_exists) {
    return NS_OK;
  }

  if (selectByURI) {
    if (_place.guid.IsEmpty()) {
      rv = stmt->GetUTF8String(0, _place.guid);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  else {
    nsAutoCString spec;
    rv = stmt->GetUTF8String(0, spec);
    NS_ENSURE_SUCCESS(rv, rv);
    _place.spec = spec;
  }

  rv = stmt->GetInt64(1, &_place.placeId);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString title;
  rv = stmt->GetString(2, title);
  NS_ENSURE_SUCCESS(rv, rv);

  // If the title we were given was void, that means we did not bother to set
  // it to anything.  As a result, ignore the fact that we may have changed the
  // title (because we don't want to, that would be empty), and set the title
  // to what is currently stored in the datbase.
  if (_place.title.IsVoid()) {
    _place.title = title;
  }
  // Otherwise, just indicate if the title has changed.
  else {
    _place.titleChanged = !(_place.title.Equals(title)) &&
                          !(_place.title.IsEmpty() && title.IsVoid());
  }

  int32_t hidden;
  rv = stmt->GetInt32(3, &hidden);
  NS_ENSURE_SUCCESS(rv, rv);
  _place.hidden = !!hidden;

  int32_t typed;
  rv = stmt->GetInt32(4, &typed);
  NS_ENSURE_SUCCESS(rv, rv);
  _place.typed = !!typed;

  rv = stmt->GetInt32(5, &_place.frecency);
  NS_ENSURE_SUCCESS(rv, rv);
  int32_t visitCount;
  rv = stmt->GetInt32(6, &visitCount);
  NS_ENSURE_SUCCESS(rv, rv);
  _place.visitCount = visitCount;
  rv = stmt->GetInt64(7, &_place.lastVisitTime);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->GetInt64(8, &_place.lastVisitId);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

MOZ_DEFINE_MALLOC_SIZE_OF(HistoryMallocSizeOf)

NS_IMETHODIMP
History::CollectReports(nsIHandleReportCallback* aHandleReport,
                        nsISupports* aData, bool aAnonymize)
{
  MOZ_COLLECT_REPORT(
    "explicit/history-links-hashtable", KIND_HEAP, UNITS_BYTES,
    SizeOfIncludingThis(HistoryMallocSizeOf),
    "Memory used by the hashtable that records changes to the visited state "
    "of links.");

  return NS_OK;
}

size_t
History::SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOfThis)
{
  return aMallocSizeOfThis(this) +
         mObservers.SizeOfExcludingThis(aMallocSizeOfThis);
}

/* static */
History*
History::GetService()
{
  if (gService) {
    return gService;
  }

  nsCOMPtr<IHistory> service(do_GetService(NS_IHISTORY_CONTRACTID));
  if (service) {
    NS_ASSERTION(gService, "Our constructor was not run?!");
  }

  return gService;
}

/* static */
already_AddRefed<History>
History::GetSingleton()
{
  if (!gService) {
    RefPtr<History> svc = new History();
    MOZ_ASSERT(gService == svc.get());
    svc->InitMemoryReporter();
    return svc.forget();
  }

  return do_AddRef(gService);
}

mozIStorageConnection*
History::GetDBConn()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mShuttingDown)
    return nullptr;
  if (!mDB) {
    mDB = Database::GetDatabase();
    NS_ENSURE_TRUE(mDB, nullptr);
    // This must happen on the main-thread, so when we try to use the connection
    // later it's initialized.
    mDB->EnsureConnection();
    NS_ENSURE_TRUE(mDB, nullptr);
  }
  return mDB->MainConn();
}

const mozIStorageConnection*
History::GetConstDBConn()
{
  MOZ_ASSERT(mDB || mShuttingDown);
  if (mShuttingDown || !mDB) {
    return nullptr;
  }
  return mDB->MainConn();
}

void
History::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Prevent other threads from scheduling uses of the DB while we mark
  // ourselves as shutting down.
  MutexAutoLock lockedScope(mShutdownMutex);
  MOZ_ASSERT(!mShuttingDown && "Shutdown was called more than once!");

  mShuttingDown = true;

  if (mConcurrentStatementsHolder) {
    mConcurrentStatementsHolder->Shutdown();
  }
}

void
History::AppendToRecentlyVisitedURIs(nsIURI* aURI) {
  // Add a new entry, if necessary.
  RecentURIKey* entry = mRecentlyVisitedURIs.GetEntry(aURI);
  if (!entry) {
    entry = mRecentlyVisitedURIs.PutEntry(aURI);
  }
  if (entry) {
    entry->time = PR_Now();
  }

  // Remove entries older than RECENTLY_VISITED_URIS_MAX_AGE.
  for (auto iter = mRecentlyVisitedURIs.Iter(); !iter.Done(); iter.Next()) {
    RecentURIKey* entry = iter.Get();
    if ((PR_Now() - entry->time) > RECENTLY_VISITED_URIS_MAX_AGE) {
      iter.Remove();
    }
  }
}

inline bool
History::IsRecentlyVisitedURI(nsIURI* aURI) {
  RecentURIKey* entry = mRecentlyVisitedURIs.GetEntry(aURI);
  // Check if the entry exists and is younger than RECENTLY_VISITED_URIS_MAX_AGE.
  return entry && (PR_Now() - entry->time) < RECENTLY_VISITED_URIS_MAX_AGE;
}

////////////////////////////////////////////////////////////////////////////////
//// IHistory

NS_IMETHODIMP
History::VisitURI(nsIURI* aURI,
                  nsIURI* aLastVisitedURI,
                  uint32_t aFlags)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aURI);

  if (mShuttingDown) {
    return NS_OK;
  }

  if (XRE_IsContentProcess()) {
    URIParams uri;
    SerializeURI(aURI, uri);

    OptionalURIParams lastVisitedURI;
    SerializeURI(aLastVisitedURI, lastVisitedURI);

    mozilla::dom::ContentChild* cpc =
      mozilla::dom::ContentChild::GetSingleton();
    NS_ASSERTION(cpc, "Content Protocol is NULL!");
    (void)cpc->SendVisitURI(uri, lastVisitedURI, aFlags);
    return NS_OK;
  }

  nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(navHistory, NS_ERROR_OUT_OF_MEMORY);

  // Silently return if URI is something we shouldn't add to DB.
  bool canAdd;
  nsresult rv = navHistory->CanAddURI(aURI, &canAdd);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!canAdd) {
    return NS_OK;
  }

  // Do not save a reloaded uri if we have visited the same URI recently.
  bool reload = false;
  if (aLastVisitedURI) {
    rv = aURI->Equals(aLastVisitedURI, &reload);
    NS_ENSURE_SUCCESS(rv, rv);
    if (reload && IsRecentlyVisitedURI(aURI)) {
      // Regardless we must update the stored visit time.
      AppendToRecentlyVisitedURIs(aURI);
      return NS_OK;
    }
  }

  nsTArray<VisitData> placeArray(1);
  NS_ENSURE_TRUE(placeArray.AppendElement(VisitData(aURI, aLastVisitedURI)),
                 NS_ERROR_OUT_OF_MEMORY);
  VisitData& place = placeArray.ElementAt(0);
  NS_ENSURE_FALSE(place.spec.IsEmpty(), NS_ERROR_INVALID_ARG);

  place.visitTime = PR_Now();

  // Assigns a type to the edge in the visit linked list. Each type will be
  // considered differently when weighting the frecency of a location.
  uint32_t recentFlags = navHistory->GetRecentFlags(aURI);
  bool isFollowedLink = recentFlags & nsNavHistory::RECENT_ACTIVATED;

  // Embed visits should never be added to the database, and the same is valid
  // for redirects across frames.
  // For the above reasoning non-toplevel transitions are handled at first.
  // if the visit is toplevel or a non-toplevel followed link, then it can be
  // handled as usual and stored on disk.

  uint32_t transitionType = nsINavHistoryService::TRANSITION_LINK;

  if (!(aFlags & IHistory::TOP_LEVEL) && !isFollowedLink) {
    // A frame redirected to a new site without user interaction.
    transitionType = nsINavHistoryService::TRANSITION_EMBED;
  }
  else if (aFlags & IHistory::REDIRECT_TEMPORARY) {
    transitionType = nsINavHistoryService::TRANSITION_REDIRECT_TEMPORARY;
  }
  else if (aFlags & IHistory::REDIRECT_PERMANENT) {
    transitionType = nsINavHistoryService::TRANSITION_REDIRECT_PERMANENT;
  }
  else if (reload) {
    transitionType = nsINavHistoryService::TRANSITION_RELOAD;
  }
  else if ((recentFlags & nsNavHistory::RECENT_TYPED) &&
           !(aFlags & IHistory::UNRECOVERABLE_ERROR)) {
    // Don't mark error pages as typed, even if they were actually typed by
    // the user.  This is useful to limit their score in autocomplete.
    transitionType = nsINavHistoryService::TRANSITION_TYPED;
  }
  else if (recentFlags & nsNavHistory::RECENT_BOOKMARKED) {
    transitionType = nsINavHistoryService::TRANSITION_BOOKMARK;
  }
  else if (!(aFlags & IHistory::TOP_LEVEL) && isFollowedLink) {
    // User activated a link in a frame.
    transitionType = nsINavHistoryService::TRANSITION_FRAMED_LINK;
  }

  place.SetTransitionType(transitionType);
  place.redirect = aFlags & IHistory::REDIRECT_SOURCE;
  place.hidden = GetHiddenState(place.redirect, place.transitionType);

  // Error pages should never be autocompleted.
  if (aFlags & IHistory::UNRECOVERABLE_ERROR) {
    place.shouldUpdateFrecency = false;
  }

  // EMBED visits are session-persistent and should not go through the database.
  // They exist only to keep track of isVisited status during the session.
  if (place.transitionType == nsINavHistoryService::TRANSITION_EMBED) {
    StoreAndNotifyEmbedVisit(place);
  }
  else {
    mozIStorageConnection* dbConn = GetDBConn();
    NS_ENSURE_STATE(dbConn);

    rv = InsertVisitedURIs::Start(dbConn, placeArray);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Finally, notify that we've been visited.
  nsCOMPtr<nsIObserverService> obsService =
    mozilla::services::GetObserverService();
  if (obsService) {
    obsService->NotifyObservers(aURI, NS_LINK_VISITED_EVENT_TOPIC, nullptr);
  }

  return NS_OK;
}

NS_IMETHODIMP
History::RegisterVisitedCallback(nsIURI* aURI,
                                 Link* aLink)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ASSERTION(aURI, "Must pass a non-null URI!");
  if (XRE_IsContentProcess()) {
    MOZ_ASSERT(aLink, "Must pass a non-null Link!");
  }

  // Obtain our array of observers for this URI.
#ifdef DEBUG
  bool keyAlreadyExists = !!mObservers.GetEntry(aURI);
#endif
  KeyClass* key = mObservers.PutEntry(aURI);
  NS_ENSURE_TRUE(key, NS_ERROR_OUT_OF_MEMORY);
  ObserverArray& observers = key->array;

  if (observers.IsEmpty()) {
    NS_ASSERTION(!keyAlreadyExists,
                 "An empty key was kept around in our hashtable!");

    // We are the first Link node to ask about this URI, or there are no pending
    // Links wanting to know about this URI.  Therefore, we should query the
    // database now.
    nsresult rv = VisitedQuery::Start(aURI);

    // In IPC builds, we are passed a nullptr Link from
    // ContentParent::RecvStartVisitedQuery.  Since we won't be adding a
    // nullptr entry to our list of observers, and the code after this point
    // assumes that aLink is non-nullptr, we will need to return now.
    if (NS_FAILED(rv) || !aLink) {
      // Remove our array from the hashtable so we don't keep it around.
      MOZ_ASSERT(key == mObservers.GetEntry(aURI), "The URIs hash mutated!");
      // In some case calling RemoveEntry on the key obtained by PutEntry
      // crashes for currently unknown reasons.  Our suspect is that something
      // between PutEntry and this call causes a nested loop that either removes
      // the entry or reallocs the hash.
      // TODO (Bug 1412647): we must figure the root cause for these issues and
      // remove this stop-gap crash fix.
      key = mObservers.GetEntry(aURI);
      if (key) {
        mObservers.RemoveEntry(key);
      }
      return rv;
    }
  }
  // In IPC builds, we are passed a nullptr Link from
  // ContentParent::RecvStartVisitedQuery.  All of our code after this point
  // assumes aLink is non-nullptr, so we have to return now.
  else if (!aLink) {
    NS_ASSERTION(XRE_IsParentProcess(),
                 "We should only ever get a null Link in the default process!");
    return NS_OK;
  }

  // Sanity check that Links are not registered more than once for a given URI.
  // This will not catch a case where it is registered for two different URIs.
  NS_ASSERTION(!observers.Contains(aLink),
               "Already tracking this Link object!");

  // Start tracking our Link.
  if (!observers.AppendElement(aLink)) {
    // Curses - unregister and return failure.
    (void)UnregisterVisitedCallback(aURI, aLink);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // If this link has already been visited, we cannot synchronously mark
  // ourselves as visited, so instead we fire a runnable into our docgroup,
  // which will handle it for us.
  if (key->mVisited) {
    DispatchNotifyVisited(aURI, GetLinkDocument(aLink));
  }

  return NS_OK;
}

NS_IMETHODIMP
History::UnregisterVisitedCallback(nsIURI* aURI,
                                   Link* aLink)
{
  MOZ_ASSERT(NS_IsMainThread());
  // TODO: aURI is sometimes null - see bug 548685
  NS_ASSERTION(aURI, "Must pass a non-null URI!");
  NS_ASSERTION(aLink, "Must pass a non-null Link object!");

  // Get the array, and remove the item from it.
  KeyClass* key = mObservers.GetEntry(aURI);
  if (!key) {
    NS_ERROR("Trying to unregister for a URI that wasn't registered!");
    return NS_ERROR_UNEXPECTED;
  }
  ObserverArray& observers = key->array;
  if (!observers.RemoveElement(aLink)) {
    NS_ERROR("Trying to unregister a node that wasn't registered!");
    return NS_ERROR_UNEXPECTED;
  }

  // If the array is now empty, we should remove it from the hashtable.
  if (observers.IsEmpty()) {
    MOZ_ASSERT(key == mObservers.GetEntry(aURI), "The URIs hash mutated!");
    mObservers.RemoveEntry(key);
  }

  return NS_OK;
}

NS_IMETHODIMP
History::SetURITitle(nsIURI* aURI, const nsAString& aTitle)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aURI);

  if (mShuttingDown) {
    return NS_OK;
  }

  if (XRE_IsContentProcess()) {
    URIParams uri;
    SerializeURI(aURI, uri);

    mozilla::dom::ContentChild * cpc =
      mozilla::dom::ContentChild::GetSingleton();
    NS_ASSERTION(cpc, "Content Protocol is NULL!");
    (void)cpc->SendSetURITitle(uri, PromiseFlatString(aTitle));
    return NS_OK;
  }

  nsNavHistory* navHistory = nsNavHistory::GetHistoryService();

  // At first, it seems like nav history should always be available here, no
  // matter what.
  //
  // nsNavHistory fails to register as a service if there is no profile in
  // place (for instance, if user is choosing a profile).
  //
  // Maybe the correct thing to do is to not register this service if no
  // profile has been selected?
  //
  NS_ENSURE_TRUE(navHistory, NS_ERROR_FAILURE);

  bool canAdd;
  nsresult rv = navHistory->CanAddURI(aURI, &canAdd);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!canAdd) {
    return NS_OK;
  }

  // Embed visits don't have a database entry, thus don't set a title on them.
  if (navHistory->hasEmbedVisit(aURI)) {
    return NS_OK;
  }

  mozIStorageConnection* dbConn = GetDBConn();
  NS_ENSURE_STATE(dbConn);

  rv = SetPageTitle::Start(dbConn, aURI, aTitle);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIDownloadHistory

NS_IMETHODIMP
History::AddDownload(nsIURI* aSource, nsIURI* aReferrer,
                     PRTime aStartTime, nsIURI* aDestination)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aSource);

  if (mShuttingDown) {
    return NS_OK;
  }

  if (XRE_IsContentProcess()) {
    NS_ERROR("Cannot add downloads to history from content process!");
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(navHistory, NS_ERROR_OUT_OF_MEMORY);

  // Silently return if URI is something we shouldn't add to DB.
  bool canAdd;
  nsresult rv = navHistory->CanAddURI(aSource, &canAdd);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!canAdd) {
    return NS_OK;
  }

  nsTArray<VisitData> placeArray(1);
  NS_ENSURE_TRUE(placeArray.AppendElement(VisitData(aSource, aReferrer)),
                 NS_ERROR_OUT_OF_MEMORY);
  VisitData& place = placeArray.ElementAt(0);
  NS_ENSURE_FALSE(place.spec.IsEmpty(), NS_ERROR_INVALID_ARG);

  place.visitTime = aStartTime;
  place.SetTransitionType(nsINavHistoryService::TRANSITION_DOWNLOAD);
  place.hidden = false;

  mozIStorageConnection* dbConn = GetDBConn();
  NS_ENSURE_STATE(dbConn);

  nsMainThreadPtrHandle<mozIVisitInfoCallback> callback;
  if (aDestination) {
    callback = new nsMainThreadPtrHolder<mozIVisitInfoCallback>(
      "mozIVisitInfoCallback", new SetDownloadAnnotations(aDestination));
  }

  rv = InsertVisitedURIs::Start(dbConn, placeArray, callback);
  NS_ENSURE_SUCCESS(rv, rv);

  // Finally, notify that we've been visited.
  nsCOMPtr<nsIObserverService> obsService =
    mozilla::services::GetObserverService();
  if (obsService) {
    obsService->NotifyObservers(aSource, NS_LINK_VISITED_EVENT_TOPIC, nullptr);
  }

  return NS_OK;
}

NS_IMETHODIMP
History::RemoveAllDownloads()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mShuttingDown) {
    return NS_OK;
  }

  if (XRE_IsContentProcess()) {
    NS_ERROR("Cannot remove downloads to history from content process!");
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Ensure navHistory is initialized.
  nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(navHistory, NS_ERROR_OUT_OF_MEMORY);
  mozIStorageConnection* dbConn = GetDBConn();
  NS_ENSURE_STATE(dbConn);

  RemoveVisitsFilter filter;
  filter.transitionType = nsINavHistoryService::TRANSITION_DOWNLOAD;

  nsresult rv = RemoveVisits::Start(dbConn, filter);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// mozIAsyncHistory

NS_IMETHODIMP
History::UpdatePlaces(JS::Handle<JS::Value> aPlaceInfos,
                      mozIVisitInfoCallback* aCallback,
                      bool aGroupNotifications,
                      JSContext* aCtx)
{
  NS_ENSURE_TRUE(NS_IsMainThread(), NS_ERROR_UNEXPECTED);
  NS_ENSURE_TRUE(!aPlaceInfos.isPrimitive(), NS_ERROR_INVALID_ARG);

  uint32_t infosLength;
  JS::Rooted<JSObject*> infos(aCtx);
  nsresult rv = GetJSArrayFromJSValue(aPlaceInfos, aCtx, &infos, &infosLength);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t initialUpdatedCount = 0;

  nsTArray<VisitData> visitData;
  for (uint32_t i = 0; i < infosLength; i++) {
    JS::Rooted<JSObject*> info(aCtx);
    nsresult rv = GetJSObjectFromArray(aCtx, infos, i, &info);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIURI> uri = GetURIFromJSObject(aCtx, info, "uri");
    nsCString guid;
    {
      nsString fatGUID;
      GetStringFromJSObject(aCtx, info, "guid", fatGUID);
      if (fatGUID.IsVoid()) {
        guid.SetIsVoid(true);
      }
      else {
        guid = NS_ConvertUTF16toUTF8(fatGUID);
      }
    }

    // Make sure that any uri we are given can be added to history, and if not,
    // skip it (CanAddURI will notify our callback for us).
    if (uri && !CanAddURI(uri, guid, aCallback)) {
      continue;
    }

    // We must have at least one of uri or guid.
    NS_ENSURE_ARG(uri || !guid.IsVoid());

    // If we were given a guid, make sure it is valid.
    bool isValidGUID = IsValidGUID(guid);
    NS_ENSURE_ARG(guid.IsVoid() || isValidGUID);

    nsString title;
    GetStringFromJSObject(aCtx, info, "title", title);

    JS::Rooted<JSObject*> visits(aCtx, nullptr);
    {
      JS::Rooted<JS::Value> visitsVal(aCtx);
      bool rc = JS_GetProperty(aCtx, info, "visits", &visitsVal);
      NS_ENSURE_TRUE(rc, NS_ERROR_UNEXPECTED);
      if (!visitsVal.isPrimitive()) {
        visits = visitsVal.toObjectOrNull();
        bool isArray;
        if (!JS_IsArrayObject(aCtx, visits, &isArray)) {
          return NS_ERROR_UNEXPECTED;
        }
        if (!isArray) {
          return NS_ERROR_INVALID_ARG;
        }
      }
    }
    NS_ENSURE_ARG(visits);

    uint32_t visitsLength = 0;
    if (visits) {
      (void)JS_GetArrayLength(aCtx, visits, &visitsLength);
    }
    NS_ENSURE_ARG(visitsLength > 0);

    // Check each visit, and build our array of VisitData objects.
    visitData.SetCapacity(visitData.Length() + visitsLength);
    for (uint32_t j = 0; j < visitsLength; j++) {
      JS::Rooted<JSObject*> visit(aCtx);
      rv = GetJSObjectFromArray(aCtx, visits, j, &visit);
      NS_ENSURE_SUCCESS(rv, rv);

      VisitData& data = *visitData.AppendElement(VisitData(uri));
      if (!title.IsEmpty()) {
        data.title = title;
      } else if (!title.IsVoid()) {
        // Setting data.title to an empty string wouldn't make it non-void.
        data.title.SetIsVoid(false);
      }
      data.guid = guid;

      // We must have a date and a transaction type!
      rv = GetIntFromJSObject(aCtx, visit, "visitDate", &data.visitTime);
      NS_ENSURE_SUCCESS(rv, rv);
      // visitDate should be in microseconds. It's easy to do the wrong thing
      // and pass milliseconds to updatePlaces, so we lazily check for that.
      // While it's not easily distinguishable, since both are integers, we can
      // check if the value is very far in the past, and assume it's probably
      // a mistake.
      if (data.visitTime < (PR_Now() / 1000)) {
#ifdef DEBUG
        nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
        Unused << xpc->DebugDumpJSStack(false, false, false);
        MOZ_CRASH("invalid time format passed to updatePlaces");
#endif
        return NS_ERROR_INVALID_ARG;
      }
      uint32_t transitionType = 0;
      rv = GetIntFromJSObject(aCtx, visit, "transitionType", &transitionType);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_ARG_RANGE(transitionType,
                          nsINavHistoryService::TRANSITION_LINK,
                          nsINavHistoryService::TRANSITION_RELOAD);
      data.SetTransitionType(transitionType);
      data.hidden = GetHiddenState(false, transitionType);

      // If the visit is an embed visit, we do not actually add it to the
      // database.
      if (transitionType == nsINavHistoryService::TRANSITION_EMBED) {
        StoreAndNotifyEmbedVisit(data, aCallback);
        visitData.RemoveLastElement();
        initialUpdatedCount++;
        continue;
      }

      // The referrer is optional.
      nsCOMPtr<nsIURI> referrer = GetURIFromJSObject(aCtx, visit,
                                                     "referrerURI");
      if (referrer) {
        (void)referrer->GetSpec(data.referrerSpec);
      }
    }
  }

  mozIStorageConnection* dbConn = GetDBConn();
  NS_ENSURE_STATE(dbConn);

  nsMainThreadPtrHandle<mozIVisitInfoCallback>
    callback(new nsMainThreadPtrHolder<mozIVisitInfoCallback>(
      "mozIVisitInfoCallback", aCallback));

  // It is possible that all of the visits we were passed were dissallowed by
  // CanAddURI, which isn't an error.  If we have no visits to add, however,
  // we should not call InsertVisitedURIs::Start.
  if (visitData.Length()) {
    nsresult rv = InsertVisitedURIs::Start(dbConn, visitData,
                                           callback, aGroupNotifications,
                                           initialUpdatedCount);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (aCallback) {
    // Be sure to notify that all of our operations are complete.  This
    // is dispatched to the background thread first and redirected to the
    // main thread from there to make sure that all database notifications
    // and all embed or canAddURI notifications have finished.

    // Note: if we're inserting anything, it's the responsibility of
    // InsertVisitedURIs to call the completion callback, as here we won't
    // know how yet many items we will successfully insert/update.
    nsCOMPtr<nsIEventTarget> backgroundThread = do_GetInterface(dbConn);
    NS_ENSURE_TRUE(backgroundThread, NS_ERROR_UNEXPECTED);
    nsCOMPtr<nsIRunnable> event = new NotifyCompletion(callback,
                                                       initialUpdatedCount);
    return backgroundThread->Dispatch(event, NS_DISPATCH_NORMAL);
  }

  return NS_OK;
}

NS_IMETHODIMP
History::IsURIVisited(nsIURI* aURI,
                      mozIVisitedStatusCallback* aCallback)
{
  NS_ENSURE_STATE(NS_IsMainThread());
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG(aCallback);

  nsresult rv = VisitedQuery::Start(aURI, aCallback);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
History::Observe(nsISupports* aSubject, const char* aTopic,
                 const char16_t* aData)
{
  if (strcmp(aTopic, TOPIC_PLACES_SHUTDOWN) == 0) {
    Shutdown();

    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      (void)os->RemoveObserver(this, TOPIC_PLACES_SHUTDOWN);
    }
  }

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// nsISupports

NS_IMPL_ISUPPORTS(
  History
, IHistory
, nsIDownloadHistory
, mozIAsyncHistory
, nsIObserver
, nsIMemoryReporter
)

} // namespace places
} // namespace mozilla
