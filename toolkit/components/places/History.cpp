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
#include "mozilla/unused.h"
#include "nsContentUtils.h" // for nsAutoScriptBlocker
#include "nsJSUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsPrintfCString.h"
#include "nsTHashtable.h"
#include "jsapi.h"

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
#define DESTINATIONFILENAME_ANNO \
        NS_LITERAL_CSTRING("downloads/destinationFileName")

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
};

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
    NS_PRECONDITION(aURI, "Null URI");

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
      callback(new nsMainThreadPtrHolder<mozIVisitedStatusCallback>(aCallback));

    nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
    NS_ENSURE_STATE(navHistory);
    if (navHistory->hasEmbedVisit(aURI)) {
      RefPtr<VisitedQuery> cb = new VisitedQuery(aURI, callback, true);
      NS_ENSURE_TRUE(cb, NS_ERROR_OUT_OF_MEMORY);
      // As per IHistory contract, we must notify asynchronously.
      NS_DispatchToMainThread(NewRunnableMethod(cb, &VisitedQuery::NotifyVisitedStatus));

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
 * Notifies observers about a visit.
 */
class NotifyVisitObservers : public Runnable
{
public:
  NotifyVisitObservers(VisitData& aPlace)
  : mPlace(aPlace)
  , mHistory(History::GetService())
  {
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");

    // We are in the main thread, no need to lock.
    if (mHistory->IsShuttingDown()) {
      // If we are shutting down, we cannot notify the observers.
      return NS_OK;
    }

    nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
    if (!navHistory) {
      NS_WARNING("Trying to notify about a visit but cannot get the history service!");
      return NS_OK;
    }

    nsCOMPtr<nsIURI> uri;
    MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(uri), mPlace.spec));
    if (!uri) {
      return NS_ERROR_UNEXPECTED;
    }

    // Notify the visit.  Note that TRANSITION_EMBED visits are never added
    // to the database, thus cannot be queried and we don't notify them.
    if (mPlace.transitionType != nsINavHistoryService::TRANSITION_EMBED) {
      navHistory->NotifyOnVisit(uri, mPlace.visitId, mPlace.visitTime,
                                mPlace.referrerVisitId, mPlace.transitionType,
                                mPlace.guid, mPlace.hidden,
                                mPlace.visitCount + 1, // Add current visit.
                                static_cast<uint32_t>(mPlace.typed));
    }

    nsCOMPtr<nsIObserverService> obsService =
      mozilla::services::GetObserverService();
    if (obsService) {
      DebugOnly<nsresult> rv =
        obsService->NotifyObservers(uri, URI_VISIT_SAVED, nullptr);
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Could not notify observers");
    }

    History* history = History::GetService();
    NS_ENSURE_STATE(history);
    history->AppendToRecentlyVisitedURIs(uri);
    history->NotifyVisited(uri);

    return NS_OK;
  }
private:
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
  : mSpec(aSpec)
  , mTitle(aTitle)
  , mGUID(aGUID)
  {
  }

  NS_IMETHOD Run()
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
  NotifyPlaceInfoCallback(const nsMainThreadPtrHandle<mozIVisitInfoCallback>& aCallback,
                          const VisitData& aPlace,
                          bool aIsSingleVisit,
                          nsresult aResult)
  : mCallback(aCallback)
  , mPlace(aPlace)
  , mResult(aResult)
  , mIsSingleVisit(aIsSingleVisit)
  {
    MOZ_ASSERT(aCallback, "Must pass a non-null callback!");
  }

  NS_IMETHOD Run()
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
  explicit NotifyCompletion(const nsMainThreadPtrHandle<mozIVisitInfoCallback>& aCallback)
  : mCallback(aCallback)
  {
    MOZ_ASSERT(aCallback, "Must pass a non-null callback!");
  }

  NS_IMETHOD Run()
  {
    if (NS_IsMainThread()) {
      (void)mCallback->HandleCompletion();
    }
    else {
      (void)NS_DispatchToMainThread(this);
    }
    return NS_OK;
  }

private:
  nsMainThreadPtrHandle<mozIVisitInfoCallback> mCallback;
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
      callback(new nsMainThreadPtrHolder<mozIVisitInfoCallback>(aCallback));
    nsCOMPtr<nsIRunnable> event =
      new NotifyPlaceInfoCallback(callback, place, true, NS_ERROR_INVALID_ARG);
    (void)NS_DispatchToMainThread(event);
  }

  return false;
}

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
   */
  static nsresult Start(mozIStorageConnection* aConnection,
                        nsTArray<VisitData>& aPlaces,
                        mozIVisitInfoCallback* aCallback = nullptr)
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
      callback(new nsMainThreadPtrHolder<mozIVisitInfoCallback>(aCallback));
    RefPtr<InsertVisitedURIs> event =
      new InsertVisitedURIs(aConnection, aPlaces, callback);

    // Get the target thread, and then start the work!
    nsCOMPtr<nsIEventTarget> target = do_GetInterface(aConnection);
    NS_ENSURE_TRUE(target, NS_ERROR_UNEXPECTED);
    nsresult rv = target->Dispatch(event, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread(), "This should not be called on the main thread");

    // Prevent the main thread from shutting down while this is running.
    MutexAutoLock lockedScope(mHistory->GetShutdownMutex());
    if (mHistory->IsShuttingDown()) {
      // If we were already shutting down, we cannot insert the URIs.
      return NS_OK;
    }

    mozStorageTransaction transaction(mDBConn, false,
                                      mozIStorageConnection::TRANSACTION_IMMEDIATE);

    VisitData* lastFetchedPlace = nullptr;
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
          if (!!mCallback) {
            nsCOMPtr<nsIRunnable> event =
              new NotifyPlaceInfoCallback(mCallback, place, true, rv);
            return NS_DispatchToMainThread(event);
          }
          return NS_OK;
        }
        lastFetchedPlace = &mPlaces.ElementAt(i);
      } else {
        // Copy over the data from the already known place.
        place.placeId = lastFetchedPlace->placeId;
        place.guid = lastFetchedPlace->guid;
        place.lastVisitId = lastFetchedPlace->visitId;
        place.lastVisitTime = lastFetchedPlace->visitTime;
        place.titleChanged = !lastFetchedPlace->title.Equals(place.title);
        place.frecency = lastFetchedPlace->frecency;
        // Add one visit for the previous loop.
        place.visitCount = ++(*lastFetchedPlace).visitCount;
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
        nsCOMPtr<nsIRunnable> event =
          new NotifyPlaceInfoCallback(mCallback, place, true, rv);
        nsresult rv2 = NS_DispatchToMainThread(event);
        NS_ENSURE_SUCCESS(rv2, rv2);
      }
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIRunnable> event = new NotifyVisitObservers(place);
      rv = NS_DispatchToMainThread(event);
      NS_ENSURE_SUCCESS(rv, rv);

      // Notify about title change if needed.
      if ((!known && !place.title.IsVoid()) || place.titleChanged) {
        event = new NotifyTitleObservers(place.spec, place.title, place.guid);
        rv = NS_DispatchToMainThread(event);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }

    nsresult rv = transaction.Commit();
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }
private:
  InsertVisitedURIs(mozIStorageConnection* aConnection,
                    nsTArray<VisitData>& aPlaces,
                    const nsMainThreadPtrHandle<mozIVisitInfoCallback>& aCallback)
  : mDBConn(aConnection)
  , mCallback(aCallback)
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
      rv = mHistory->InsertPlace(aPlace);
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
      stmt = mHistory->GetStatement(
        "UPDATE moz_places "
        "SET frecency = NOTIFY_FRECENCY("
          "CALCULATE_FRECENCY(:page_id), "
          "url, guid, hidden, last_visit_date"
        ") "
        "WHERE id = :page_id"
      );
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scoper(stmt);

      rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), aPlace.placeId);
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

  /**
   * Strong reference to the History object because we do not want it to
   * disappear out from under us.
   */
  RefPtr<History> mHistory;
};

class GetPlaceInfo final : public Runnable {
public:
  /**
   * Get the place info for a given place (by GUID or URI)  asynchronously.
   */
  static nsresult Start(mozIStorageConnection* aConnection,
                        VisitData& aPlace,
                        mozIVisitInfoCallback* aCallback) {
    MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");

    nsMainThreadPtrHandle<mozIVisitInfoCallback>
      callback(new nsMainThreadPtrHolder<mozIVisitInfoCallback>(aCallback));
    RefPtr<GetPlaceInfo> event = new GetPlaceInfo(aPlace, callback);

    // Get the target thread, and then start the work!
    nsCOMPtr<nsIEventTarget> target = do_GetInterface(aConnection);
    NS_ENSURE_TRUE(target, NS_ERROR_UNEXPECTED);
    nsresult rv = target->Dispatch(event, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(!NS_IsMainThread(), "This should not be called on the main thread");

    bool exists;
    nsresult rv = mHistory->FetchPageInfo(mPlace, &exists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!exists)
      rv = NS_ERROR_NOT_AVAILABLE;

    nsCOMPtr<nsIRunnable> event =
      new NotifyPlaceInfoCallback(mCallback, mPlace, false, rv);

    rv = NS_DispatchToMainThread(event);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }
private:
  GetPlaceInfo(VisitData& aPlace,
               const nsMainThreadPtrHandle<mozIVisitInfoCallback>& aCallback)
  : mPlace(aPlace)
  , mCallback(aCallback)
  , mHistory(History::GetService())
  {
    MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");
  }

  VisitData mPlace;
  nsMainThreadPtrHandle<mozIVisitInfoCallback> mCallback;
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

  NS_IMETHOD Run()
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
  SetPageTitle(const nsCString& aSpec,
               const nsAString& aTitle)
  : mHistory(History::GetService())
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

    nsCOMPtr<nsIFile> destinationFile;
    rv = destinationFileURL->GetFile(getter_AddRefs(destinationFile));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString destinationFileName;
    rv = destinationFile->GetLeafName(destinationFileName);
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

    rv = annosvc->SetPageAnnotationString(
      source,
      DESTINATIONFILENAME_ANNO,
      destinationFileName,
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
      rv = mHistory->SetURITitle(source, destinationFileName);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
  }

  NS_IMETHOD HandleCompletion() override
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
    : mPlaces(VISITS_REMOVAL_INITIAL_HASH_LENGTH)
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

  NS_IMETHOD Run()
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

  NS_IMETHOD Run()
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
  RemoveVisits(mozIStorageConnection* aConnection,
               RemoveVisitsFilter& aFilter)
  : mDBConn(aConnection)
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
      nsAutoCString query("DELETE FROM moz_updatehosts_temp");
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
      callback(new nsMainThreadPtrHolder<mozIVisitInfoCallback>(aCallback));
    nsCOMPtr<nsIRunnable> event =
      new NotifyPlaceInfoCallback(callback, aPlace, true, NS_OK);
    (void)NS_DispatchToMainThread(event);
  }

  nsCOMPtr<nsIRunnable> event = new NotifyVisitObservers(aPlace);
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
  NS_WARN_IF_FALSE(os, "Observer service was not found!");
  if (os) {
    (void)os->AddObserver(this, TOPIC_PLACES_SHUTDOWN, false);
  }
}

History::~History()
{
  UnregisterWeakMemoryReporter(this);

  gService = nullptr;

  NS_ASSERTION(mObservers.Count() == 0,
               "Not all Links were removed before we disappear!");
}

void
History::InitMemoryReporter()
{
  RegisterWeakMemoryReporter(this);
}

NS_IMETHODIMP
History::NotifyVisited(nsIURI* aURI)
{
  NS_ENSURE_ARG(aURI);

  nsAutoScriptBlocker scriptBlocker;

  if (XRE_IsParentProcess()) {
    nsTArray<ContentParent*> cplist;
    ContentParent::GetAll(cplist);

    if (!cplist.IsEmpty()) {
      URIParams uri;
      SerializeURI(aURI, uri);
      for (uint32_t i = 0; i < cplist.Length(); ++i) {
        Unused << cplist[i]->SendNotifyVisited(uri);
      }
    }
  }

  // If we have no observers for this URI, we have nothing to notify about.
  KeyClass* key = mObservers.GetEntry(aURI);
  if (!key) {
    return NS_OK;
  }

  // Update status of each Link node.
  {
    // RemoveEntry will destroy the array, this iterator should not survive it.
    ObserverArray::ForwardIterator iter(key->array);
    while (iter.HasMore()) {
      Link* link = iter.GetNext();
      link->SetLinkState(eLinkState_Visited);
      // Verify that the observers hash doesn't mutate while looping through
      // the links associated with this URI.
      MOZ_ASSERT(key == mObservers.GetEntry(aURI),
                 "The URIs hash mutated!");
    }
  }

  // All the registered nodes can now be removed for this URI.
  mObservers.RemoveEntry(key);
  return NS_OK;
}

class ConcurrentStatementsHolder final : public mozIStorageCompletionCallback {
public:
  NS_DECL_ISUPPORTS

  explicit ConcurrentStatementsHolder(mozIStorageConnection* aDBConn)
  {
    DebugOnly<nsresult> rv = aDBConn->AsyncClone(true, this);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  NS_IMETHOD Complete(nsresult aStatus, nsISupports* aConnection) override {
    if (NS_FAILED(aStatus))
      return NS_OK;
    mReadOnlyDBConn = do_QueryInterface(aConnection);

    // Now we can create our cached statements.

    if (!mIsVisitedStatement) {
      (void)mReadOnlyDBConn->CreateAsyncStatement(NS_LITERAL_CSTRING(
        "SELECT 1 FROM moz_places h "
        "WHERE url = ?1 AND last_visit_date NOTNULL "
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
    if (mReadOnlyDBConn) {
      mIsVisitedCallbacks.Clear();
      DebugOnly<nsresult> rv;
      if (mIsVisitedStatement) {
        rv = mIsVisitedStatement->Finalize();
        MOZ_ASSERT(NS_SUCCEEDED(rv));
      }
      rv = mReadOnlyDBConn->AsyncClose(nullptr);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

private:
  ~ConcurrentStatementsHolder()
  {
  }

  nsCOMPtr<mozIStorageAsyncConnection> mReadOnlyDBConn;
  nsCOMPtr<mozIStorageAsyncStatement> mIsVisitedStatement;
  nsCOMArray<mozIStorageCompletionCallback> mIsVisitedCallbacks;
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
History::InsertPlace(VisitData& aPlace)
{
  MOZ_ASSERT(aPlace.placeId == 0, "should not have a valid place id!");
  MOZ_ASSERT(!aPlace.shouldUpdateHidden, "We should not need to update hidden");
  MOZ_ASSERT(!NS_IsMainThread(), "must be called off of the main thread!");

  nsCOMPtr<mozIStorageStatement> stmt = GetStatement(
      "INSERT INTO moz_places "
        "(url, title, rev_host, hidden, typed, frecency, guid) "
      "VALUES (:url, :title, :rev_host, :hidden, :typed, :frecency, :guid) "
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
  const nsNavHistory* navHistory = nsNavHistory::GetConstHistoryService();
  NS_ENSURE_STATE(navHistory);
  navHistory->DispatchFrecencyChangedNotification(aPlace.spec, frecency,
                                                  aPlace.guid,
                                                  aPlace.hidden,
                                                  aPlace.visitTime);

  return NS_OK;
}

nsresult
History::UpdatePlace(const VisitData& aPlace)
{
  MOZ_ASSERT(!NS_IsMainThread(), "must be called off of the main thread!");
  MOZ_ASSERT(aPlace.placeId > 0, "must have a valid place id!");
  MOZ_ASSERT(!aPlace.guid.IsVoid(), "must have a guid!");

  nsCOMPtr<mozIStorageStatement> stmt = GetStatement(
      "UPDATE moz_places "
      "SET title = :title, "
          "hidden = :hidden, "
          "typed = :typed, "
          "guid = :guid "
      "WHERE id = :page_id "
    );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv;
  // Empty strings should clear the title, just like nsNavHistory::SetPageTitle.
  if (aPlace.title.IsEmpty()) {
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("title"));
  }
  else {
    rv = stmt->BindStringByName(NS_LITERAL_CSTRING("title"),
                                StringHead(aPlace.title, TITLE_LENGTH_MAX));
  }
  NS_ENSURE_SUCCESS(rv, rv);
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
      "WHERE url = :page_url "
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
    _place.titleChanged = !(_place.title.Equals(title) ||
                            (_place.title.IsEmpty() && title.IsVoid()));
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
  return MOZ_COLLECT_REPORT(
    "explicit/history-links-hashtable", KIND_HEAP, UNITS_BYTES,
    SizeOfIncludingThis(HistoryMallocSizeOf),
    "Memory used by the hashtable that records changes to the visited state "
    "of links.");
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
  MOZ_ASSERT(service, "Cannot obtain IHistory service!");
  NS_ASSERTION(gService, "Our constructor was not run?!");

  return gService;
}

/* static */
History*
History::GetSingleton()
{
  if (!gService) {
    gService = new History();
    NS_ENSURE_TRUE(gService, nullptr);
    gService->InitMemoryReporter();
  }

  NS_ADDREF(gService);
  return gService;
}

mozIStorageConnection*
History::GetDBConn()
{
  if (mShuttingDown)
    return nullptr;
  if (!mDB) {
    mDB = Database::GetDatabase();
    NS_ENSURE_TRUE(mDB, nullptr);
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
  place.hidden = GetHiddenState(aFlags & IHistory::REDIRECT_SOURCE,
                                transitionType);

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
  NS_ASSERTION(aURI, "Must pass a non-null URI!");
  if (XRE_IsContentProcess()) {
    NS_PRECONDITION(aLink, "Must pass a non-null Link!");
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
      mObservers.RemoveEntry(aURI);
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

  return NS_OK;
}

NS_IMETHODIMP
History::UnregisterVisitedCallback(nsIURI* aURI,
                                   Link* aLink)
{
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
    mObservers.RemoveEntry(aURI);
  }

  return NS_OK;
}

NS_IMETHODIMP
History::SetURITitle(nsIURI* aURI, const nsAString& aTitle)
{
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
    callback = new nsMainThreadPtrHolder<mozIVisitInfoCallback>(new SetDownloadAnnotations(aDestination));
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
History::GetPlacesInfo(JS::Handle<JS::Value> aPlaceIdentifiers,
                       mozIVisitInfoCallback* aCallback,
                       JSContext* aCtx)
{
  // Make sure nsNavHistory service is up before proceeding:
  nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
  MOZ_ASSERT(navHistory, "Could not get nsNavHistory?!");
  if (!navHistory) {
    return NS_ERROR_FAILURE;
  }

  uint32_t placesIndentifiersLength;
  JS::Rooted<JSObject*> placesIndentifiers(aCtx);
  nsresult rv = GetJSArrayFromJSValue(aPlaceIdentifiers, aCtx,
                                      &placesIndentifiers,
                                      &placesIndentifiersLength);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<VisitData> placesInfo;
  placesInfo.SetCapacity(placesIndentifiersLength);
  for (uint32_t i = 0; i < placesIndentifiersLength; i++) {
    JS::Rooted<JS::Value> placeIdentifier(aCtx);
    bool rc = JS_GetElement(aCtx, placesIndentifiers, i, &placeIdentifier);
    NS_ENSURE_TRUE(rc, NS_ERROR_UNEXPECTED);

    // GUID
    nsAutoString fatGUID;
    GetJSValueAsString(aCtx, placeIdentifier, fatGUID);
    if (!fatGUID.IsVoid()) {
      NS_ConvertUTF16toUTF8 guid(fatGUID);
      if (!IsValidGUID(guid))
        return NS_ERROR_INVALID_ARG;

      VisitData& placeInfo = *placesInfo.AppendElement(VisitData());
      placeInfo.guid = guid;
    }
    else {
      nsCOMPtr<nsIURI> uri = GetJSValueAsURI(aCtx, placeIdentifier);
      if (!uri)
        return NS_ERROR_INVALID_ARG; // neither a guid, nor a uri.
      placesInfo.AppendElement(VisitData(uri));
    }
  }

  mozIStorageConnection* dbConn = GetDBConn();
  NS_ENSURE_STATE(dbConn);

  for (nsTArray<VisitData>::size_type i = 0; i < placesInfo.Length(); i++) {
    nsresult rv = GetPlaceInfo::Start(dbConn, placesInfo.ElementAt(i), aCallback);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Be sure to notify that all of our operations are complete.  This
  // is dispatched to the background thread first and redirected to the
  // main thread from there to make sure that all database notifications
  // and all embed or canAddURI notifications have finished.
  if (aCallback) {
    nsMainThreadPtrHandle<mozIVisitInfoCallback>
      callback(new nsMainThreadPtrHolder<mozIVisitInfoCallback>(aCallback));
    nsCOMPtr<nsIEventTarget> backgroundThread = do_GetInterface(dbConn);
    NS_ENSURE_TRUE(backgroundThread, NS_ERROR_UNEXPECTED);
    nsCOMPtr<nsIRunnable> event = new NotifyCompletion(callback);
    return backgroundThread->Dispatch(event, NS_DISPATCH_NORMAL);
  }

  return NS_OK;
}

NS_IMETHODIMP
History::UpdatePlaces(JS::Handle<JS::Value> aPlaceInfos,
                      mozIVisitInfoCallback* aCallback,
                      JSContext* aCtx)
{
  NS_ENSURE_TRUE(NS_IsMainThread(), NS_ERROR_UNEXPECTED);
  NS_ENSURE_TRUE(!aPlaceInfos.isPrimitive(), NS_ERROR_INVALID_ARG);

  uint32_t infosLength;
  JS::Rooted<JSObject*> infos(aCtx);
  nsresult rv = GetJSArrayFromJSValue(aPlaceInfos, aCtx, &infos, &infosLength);
  NS_ENSURE_SUCCESS(rv, rv);

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
      data.title = title;
      data.guid = guid;

      // We must have a date and a transaction type!
      rv = GetIntFromJSObject(aCtx, visit, "visitDate", &data.visitTime);
      NS_ENSURE_SUCCESS(rv, rv);
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
        visitData.RemoveElementAt(visitData.Length() - 1);
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
    callback(new nsMainThreadPtrHolder<mozIVisitInfoCallback>(aCallback));

  // It is possible that all of the visits we were passed were dissallowed by
  // CanAddURI, which isn't an error.  If we have no visits to add, however,
  // we should not call InsertVisitedURIs::Start.
  if (visitData.Length()) {
    nsresult rv = InsertVisitedURIs::Start(dbConn, visitData, callback);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Be sure to notify that all of our operations are complete.  This
  // is dispatched to the background thread first and redirected to the
  // main thread from there to make sure that all database notifications
  // and all embed or canAddURI notifications have finished.
  if (aCallback) {
    nsCOMPtr<nsIEventTarget> backgroundThread = do_GetInterface(dbConn);
    NS_ENSURE_TRUE(backgroundThread, NS_ERROR_UNEXPECTED);
    nsCOMPtr<nsIRunnable> event = new NotifyCompletion(callback);
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
