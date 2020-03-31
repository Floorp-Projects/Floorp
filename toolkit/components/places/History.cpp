/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/BrowserChild.h"
#include "nsXULAppAPI.h"

#include "History.h"
#include "nsNavHistory.h"
#include "nsNavBookmarks.h"
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
#include "nsIWidget.h"
#include "nsIXPConnect.h"
#include "nsIXULRuntime.h"
#include "mozilla/Unused.h"
#include "nsContentUtils.h"  // for nsAutoScriptBlocker
#include "nsJSUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsPrintfCString.h"
#include "nsTHashtable.h"
#include "jsapi.h"
#include "js/Array.h"  // JS::GetArrayLength, JS::IsArrayObject, JS::NewArrayObject
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/dom/ContentProcessMessageManager.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/PlacesObservers.h"
#include "mozilla/dom/PlacesVisit.h"
#include "mozilla/dom/ScriptSettings.h"

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
      : placeId(0),
        visitId(0),
        hidden(true),
        shouldUpdateHidden(true),
        typed(false),
        transitionType(UINT32_MAX),
        visitTime(0),
        frecency(-1),
        lastVisitId(0),
        lastVisitTime(0),
        visitCount(0),
        referrerVisitId(0),
        titleChanged(false),
        shouldUpdateFrecency(true),
        useFrecencyRedirectBonus(false) {
    guid.SetIsVoid(true);
    title.SetIsVoid(true);
  }

  explicit VisitData(nsIURI* aURI, nsIURI* aReferrer = nullptr)
      : placeId(0),
        visitId(0),
        hidden(true),
        shouldUpdateHidden(true),
        typed(false),
        transitionType(UINT32_MAX),
        visitTime(0),
        frecency(-1),
        lastVisitId(0),
        lastVisitTime(0),
        visitCount(0),
        referrerVisitId(0),
        titleChanged(false),
        shouldUpdateFrecency(true),
        useFrecencyRedirectBonus(false) {
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
  void SetTransitionType(uint32_t aTransitionType) {
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

  // Whether to override the visit type bonus with a redirect bonus when
  // calculating frecency on the most recent visit.
  bool useFrecencyRedirectBonus;
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
nsresult GetJSArrayFromJSValue(JS::Handle<JS::Value> aValue, JSContext* aCtx,
                               JS::MutableHandle<JSObject*> _array,
                               uint32_t* _arrayLength) {
  if (aValue.isObjectOrNull()) {
    JS::Rooted<JSObject*> val(aCtx, aValue.toObjectOrNull());
    bool isArray;
    if (!JS::IsArrayObject(aCtx, val, &isArray)) {
      return NS_ERROR_UNEXPECTED;
    }
    if (isArray) {
      _array.set(val);
      (void)JS::GetArrayLength(aCtx, _array, _arrayLength);
      NS_ENSURE_ARG(*_arrayLength > 0);
      return NS_OK;
    }
  }

  // Build a temporary array to store this one item so the code below can
  // just loop.
  *_arrayLength = 1;
  _array.set(JS::NewArrayObject(aCtx, 0));
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
already_AddRefed<nsIURI> GetJSValueAsURI(JSContext* aCtx,
                                         const JS::Value& aValue) {
  if (!aValue.isPrimitive()) {
    nsCOMPtr<nsIXPConnect> xpc = nsIXPConnect::XPConnect();

    nsCOMPtr<nsIXPConnectWrappedNative> wrappedObj;
    JS::Rooted<JSObject*> obj(aCtx, aValue.toObjectOrNull());
    nsresult rv =
        xpc->GetWrappedNativeOfJSObject(aCtx, obj, getter_AddRefs(wrappedObj));
    NS_ENSURE_SUCCESS(rv, nullptr);
    nsCOMPtr<nsIURI> uri = do_QueryInterface(wrappedObj->Native());
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
already_AddRefed<nsIURI> GetURIFromJSObject(JSContext* aCtx,
                                            JS::Handle<JSObject*> aObject,
                                            const char* aProperty) {
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
void GetJSValueAsString(JSContext* aCtx, const JS::Value& aValue,
                        nsString& _string) {
  if (aValue.isUndefined() || !(aValue.isNull() || aValue.isString())) {
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
void GetStringFromJSObject(JSContext* aCtx, JS::Handle<JSObject*> aObject,
                           const char* aProperty, nsString& _string) {
  JS::Rooted<JS::Value> val(aCtx);
  bool rc = JS_GetProperty(aCtx, aObject, aProperty, &val);
  if (!rc) {
    _string.SetIsVoid(true);
    return;
  } else {
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
nsresult GetIntFromJSObject(JSContext* aCtx, JS::Handle<JSObject*> aObject,
                            const char* aProperty, IntType* _int) {
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
nsresult GetJSObjectFromArray(JSContext* aCtx, JS::Handle<JSObject*> aArray,
                              uint32_t aIndex,
                              JS::MutableHandle<JSObject*> objOut) {
  JS::Rooted<JS::Value> value(aCtx);
  bool rc = JS_GetElement(aCtx, aArray, aIndex, &value);
  NS_ENSURE_TRUE(rc, NS_ERROR_UNEXPECTED);
  NS_ENSURE_ARG(!value.isPrimitive());
  objOut.set(&value.toObject());
  return NS_OK;
}

}  // namespace

class VisitedQuery final : public AsyncStatementCallback {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  static nsresult Start(nsIURI* aURI,
                        mozIVisitedStatusCallback* aCallback = nullptr) {
    MOZ_ASSERT(aURI, "Null URI");
    MOZ_ASSERT(XRE_IsParentProcess());

    nsMainThreadPtrHandle<mozIVisitedStatusCallback> callback(
        new nsMainThreadPtrHolder<mozIVisitedStatusCallback>(
            "mozIVisitedStatusCallback", aCallback));

    History* history = History::GetService();
    NS_ENSURE_STATE(history);
    RefPtr<VisitedQuery> query = new VisitedQuery(aURI, callback);
    return history->QueueVisitedStatement(std::move(query));
  }

  void Execute(mozIStorageAsyncStatement& aStatement) {
    // Bind by index for performance.
    nsresult rv = URIBinder::Bind(&aStatement, 0, mURI);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    nsCOMPtr<mozIStoragePendingStatement> handle;
    rv = aStatement.ExecuteAsync(this, getter_AddRefs(handle));
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  NS_IMETHOD HandleResult(mozIStorageResultSet* aResults) override {
    // If this method is called, we've gotten results, which means we have a
    // visit.
    mIsVisited = true;
    return NS_OK;
  }

  NS_IMETHOD HandleError(mozIStorageError* aError) override {
    // mIsVisited is already set to false, and that's the assumption we will
    // make if an error occurred.
    return NS_OK;
  }

  NS_IMETHOD HandleCompletion(uint16_t aReason) override {
    if (aReason == mozIStorageStatementCallback::REASON_FINISHED) {
      NotifyVisitedStatus();
    }
    return NS_OK;
  }

  void NotifyVisitedStatus() {
    // If an external handling callback is provided, just notify through it.
    if (mCallback) {
      mCallback->IsVisited(mURI, mIsVisited);
      return;
    }

    if (mIsVisited || StaticPrefs::layout_css_notify_of_unvisited()) {
      History* history = History::GetService();
      if (!history) {
        return;
      }
      auto status = mIsVisited ? IHistory::VisitedStatus::Visited
                               : IHistory::VisitedStatus::Unvisited;
      history->NotifyVisited(mURI, status);
      if (BrowserTabsRemoteAutostart()) {
        AutoTArray<VisitedQueryResult, 1> results;
        VisitedQueryResult& result = *results.AppendElement();
        result.visited() = mIsVisited;
        result.uri() = mURI;
        history->NotifyVisitedParent(results);
      }
    }

    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService) {
      static const char16_t visited[] = u"" URI_VISITED;
      static const char16_t notVisited[] = u"" URI_NOT_VISITED;
      const char16_t* status = mIsVisited ? visited : notVisited;
      (void)observerService->NotifyObservers(mURI, URI_VISITED_RESOLUTION_TOPIC,
                                             status);
    }
  }

 private:
  explicit VisitedQuery(
      nsIURI* aURI,
      const nsMainThreadPtrHandle<mozIVisitedStatusCallback>& aCallback)
      : mURI(aURI), mCallback(aCallback), mIsVisited(false) {}

  ~VisitedQuery() = default;

  nsCOMPtr<nsIURI> mURI;
  nsMainThreadPtrHandle<mozIVisitedStatusCallback> mCallback;
  bool mIsVisited;
};

NS_IMPL_ISUPPORTS_INHERITED0(VisitedQuery, AsyncStatementCallback)

/**
 * Notifies observers about a visit or an array of visits.
 */
class NotifyManyVisitsObservers : public Runnable {
 public:
  explicit NotifyManyVisitsObservers(const VisitData& aPlace)
      : Runnable("places::NotifyManyVisitsObservers"),
        mPlace(aPlace),
        mHistory(History::GetService()) {}

  explicit NotifyManyVisitsObservers(nsTArray<VisitData>& aPlaces)
      : Runnable("places::NotifyManyVisitsObservers"),
        mHistory(History::GetService()) {
    aPlaces.SwapElements(mPlaces);
  }

  nsresult NotifyVisit(nsNavHistory* aNavHistory,
                       nsCOMPtr<nsIObserverService>& aObsService, PRTime aNow,
                       nsIURI* aURI, const VisitData& aPlace) {
    if (aObsService) {
      DebugOnly<nsresult> rv =
          aObsService->NotifyObservers(aURI, URI_VISIT_SAVED, nullptr);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Could not notify observers");
    }

    if (aNow - aPlace.visitTime < RECENTLY_VISITED_URIS_MAX_AGE) {
      mHistory->AppendToRecentlyVisitedURIs(aURI, aPlace.hidden);
    }
    mHistory->NotifyVisited(aURI, IHistory::VisitedStatus::Visited);

    if (aPlace.titleChanged) {
      aNavHistory->NotifyTitleChange(aURI, aPlace.title, aPlace.guid);
    }

    aNavHistory->UpdateDaysOfHistory(aPlace.visitTime);

    return NS_OK;
  }

  void AddPlaceForNotify(const VisitData& aPlace, nsIURI* aURI,
                         Sequence<OwningNonNull<PlacesEvent>>& aEvents) {
    if (aPlace.transitionType != nsINavHistoryService::TRANSITION_EMBED) {
      RefPtr<PlacesVisit> vd = new PlacesVisit();
      vd->mVisitId = aPlace.visitId;
      vd->mUrl.Assign(NS_ConvertUTF8toUTF16(aPlace.spec));
      vd->mVisitTime = aPlace.visitTime / 1000;
      vd->mReferringVisitId = aPlace.referrerVisitId;
      vd->mTransitionType = aPlace.transitionType;
      vd->mPageGuid.Assign(aPlace.guid);
      vd->mHidden = aPlace.hidden;
      vd->mVisitCount = aPlace.visitCount + 1;  // Add current visit
      vd->mTypedCount = static_cast<uint32_t>(aPlace.typed);
      vd->mLastKnownTitle.Assign(aPlace.title);
      bool success = !!aEvents.AppendElement(vd.forget(), fallible);
      MOZ_RELEASE_ASSERT(success);
    }
  }

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY until Runnable::Run is marked
  // MOZ_CAN_RUN_SCRIPT.  See bug 1535398.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");

    // We are in the main thread, no need to lock.
    if (mHistory->IsShuttingDown()) {
      // If we are shutting down, we cannot notify the observers.
      return NS_OK;
    }

    nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
    if (!navHistory) {
      NS_WARNING(
          "Trying to notify visits observers but cannot get the history "
          "service!");
      return NS_OK;
    }

    nsCOMPtr<nsIObserverService> obsService =
        mozilla::services::GetObserverService();

    Sequence<OwningNonNull<PlacesEvent>> events;
    nsCOMArray<nsIURI> uris;
    if (mPlaces.Length() > 0) {
      for (uint32_t i = 0; i < mPlaces.Length(); ++i) {
        nsCOMPtr<nsIURI> uri;
        MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(uri), mPlaces[i].spec));
        if (!uri) {
          return NS_ERROR_UNEXPECTED;
        }
        AddPlaceForNotify(mPlaces[i], uri, events);
        uris.AppendElement(uri.forget());
      }
    } else {
      nsCOMPtr<nsIURI> uri;
      MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(uri), mPlace.spec));
      if (!uri) {
        return NS_ERROR_UNEXPECTED;
      }
      AddPlaceForNotify(mPlace, uri, events);
      uris.AppendElement(uri.forget());
    }

    if (events.Length() > 0) {
      PlacesObservers::NotifyListeners(events);
    }

    PRTime now = PR_Now();
    if (!mPlaces.IsEmpty()) {
      nsTArray<VisitedQueryResult> results(mPlaces.Length());
      for (uint32_t i = 0; i < mPlaces.Length(); ++i) {
        nsresult rv =
            NotifyVisit(navHistory, obsService, now, uris[i], mPlaces[i]);
        NS_ENSURE_SUCCESS(rv, rv);

        if (BrowserTabsRemoteAutostart()) {
          VisitedQueryResult& result = *results.AppendElement();
          result.uri() = uris[i];
          result.visited() = true;
        }
      }
      mHistory->NotifyVisitedParent(results);
    } else {
      AutoTArray<VisitedQueryResult, 1> results;
      nsresult rv = NotifyVisit(navHistory, obsService, now, uris[0], mPlace);
      NS_ENSURE_SUCCESS(rv, rv);

      if (BrowserTabsRemoteAutostart()) {
        VisitedQueryResult& result = *results.AppendElement();
        result.uri() = uris[0];
        result.visited() = true;
        mHistory->NotifyVisitedParent(results);
      }
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
class NotifyTitleObservers : public Runnable {
 public:
  /**
   * Notifies observers on the main thread.
   *
   * @param aSpec
   *        The spec of the URI to notify about.
   * @param aTitle
   *        The new title to notify about.
   */
  NotifyTitleObservers(const nsCString& aSpec, const nsString& aTitle,
                       const nsCString& aGUID)
      : Runnable("places::NotifyTitleObservers"),
        mSpec(aSpec),
        mTitle(aTitle),
        mGUID(aGUID) {}

  NS_IMETHOD Run() override {
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
class NotifyPlaceInfoCallback : public Runnable {
 public:
  NotifyPlaceInfoCallback(
      const nsMainThreadPtrHandle<mozIVisitInfoCallback>& aCallback,
      const VisitData& aPlace, bool aIsSingleVisit, nsresult aResult)
      : Runnable("places::NotifyPlaceInfoCallback"),
        mCallback(aCallback),
        mPlace(aPlace),
        mResult(aResult),
        mIsSingleVisit(aIsSingleVisit) {
    MOZ_ASSERT(aCallback, "Must pass a non-null callback!");
  }

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");

    bool hasValidURIs = true;
    nsCOMPtr<nsIURI> referrerURI;
    if (!mPlace.referrerSpec.IsEmpty()) {
      MOZ_ALWAYS_SUCCEEDS(
          NS_NewURI(getter_AddRefs(referrerURI), mPlace.referrerSpec));
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
      place = new PlaceInfo(mPlace.placeId, mPlace.guid, uri.forget(),
                            mPlace.title, -1, visits);
    } else {
      // Same as above.
      place = new PlaceInfo(mPlace.placeId, mPlace.guid, uri.forget(),
                            mPlace.title, -1);
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
class NotifyCompletion : public Runnable {
 public:
  explicit NotifyCompletion(
      const nsMainThreadPtrHandle<mozIVisitInfoCallback>& aCallback,
      uint32_t aUpdatedCount = 0)
      : Runnable("places::NotifyCompletion"),
        mCallback(aCallback),
        mUpdatedCount(aUpdatedCount) {
    MOZ_ASSERT(aCallback, "Must pass a non-null callback!");
  }

  NS_IMETHOD Run() override {
    if (NS_IsMainThread()) {
      (void)mCallback->HandleCompletion(mUpdatedCount);
    } else {
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
bool CanAddURI(nsIURI* aURI, const nsCString& aGUID = EmptyCString(),
               mozIVisitInfoCallback* aCallback = nullptr) {
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
    nsMainThreadPtrHandle<mozIVisitInfoCallback> callback(
        new nsMainThreadPtrHolder<mozIVisitInfoCallback>(
            "mozIVisitInfoCallback", aCallback));
    nsCOMPtr<nsIRunnable> event = new NotifyPlaceInfoCallback(
        callback, place, true, NS_ERROR_INVALID_ARG);
    (void)NS_DispatchToMainThread(event);
  }

  return false;
}

class NotifyManyFrecenciesChanged final : public Runnable {
 public:
  NotifyManyFrecenciesChanged()
      : Runnable("places::NotifyManyFrecenciesChanged") {}

  NS_IMETHOD Run() override {
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
class InsertVisitedURIs final : public Runnable {
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
                        uint32_t aInitialUpdatedCount = 0) {
    MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");
    MOZ_ASSERT(aPlaces.Length() > 0, "Must pass a non-empty array!");

    // Make sure nsNavHistory service is up before proceeding:
    nsNavHistory* navHistory = nsNavHistory::GetHistoryService();
    MOZ_ASSERT(navHistory, "Could not get nsNavHistory?!");
    if (!navHistory) {
      return NS_ERROR_FAILURE;
    }

    nsMainThreadPtrHandle<mozIVisitInfoCallback> callback(
        new nsMainThreadPtrHolder<mozIVisitInfoCallback>(
            "mozIVisitInfoCallback", aCallback));
    bool ignoreErrors = false, ignoreResults = false;
    if (aCallback) {
      // We ignore errors from either of these methods in case old JS consumers
      // don't implement them (in which case they will get error/result
      // notifications as normal).
      Unused << aCallback->GetIgnoreErrors(&ignoreErrors);
      Unused << aCallback->GetIgnoreResults(&ignoreResults);
    }
    RefPtr<InsertVisitedURIs> event = new InsertVisitedURIs(
        aConnection, aPlaces, callback, aGroupNotifications, ignoreErrors,
        ignoreResults, aInitialUpdatedCount);

    // Get the target thread, and then start the work!
    nsCOMPtr<nsIEventTarget> target = do_GetInterface(aConnection);
    NS_ENSURE_TRUE(target, NS_ERROR_UNEXPECTED);
    nsresult rv = target->Dispatch(event, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  NS_IMETHOD Run() override {
    MOZ_ASSERT(!NS_IsMainThread(),
               "This should not be called on the main thread");

    // The inner run method may bail out at any point, so we ensure we do
    // whatever we can and then notify the main thread we're done.
    nsresult rv = InnerRun();

    if (mSuccessfulUpdatedCount > 0 && mGroupNotifications) {
      NS_DispatchToMainThread(new NotifyManyFrecenciesChanged());
    }
    if (!!mCallback) {
      NS_DispatchToMainThread(
          new NotifyCompletion(mCallback, mSuccessfulUpdatedCount));
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

    mozStorageTransaction transaction(
        mDBConn, false, mozIStorageConnection::TRANSACTION_IMMEDIATE);

    const VisitData* lastFetchedPlace = nullptr;
    uint32_t lastFetchedVisitCount = 0;
    bool shouldChunkNotifications = mPlaces.Length() > NOTIFY_VISITS_CHUNK_SIZE;
    nsTArray<VisitData> notificationChunk;
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
      bool known =
          lastFetchedPlace && lastFetchedPlace->spec.Equals(place.spec);
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
          nsCOMPtr<nsIRunnable> event =
              new NotifyManyVisitsObservers(notificationChunk);
          rv = NS_DispatchToMainThread(event);
          NS_ENSURE_SUCCESS(rv, rv);

          int32_t nextCapacity =
              std::min(NOTIFY_VISITS_CHUNK_SIZE, numRemaining);
          notificationChunk.SetCapacity(nextCapacity);
        }
      }

      // If we get here, we must have been successful adding/updating this
      // visit/place, so update the count:
      mSuccessfulUpdatedCount++;
    }

    {
      // Trigger insertions for all the new origins of the places we inserted.
      nsAutoCString query("DELETE FROM moz_updateoriginsinsert_temp");
      nsCOMPtr<mozIStorageStatement> stmt = mHistory->GetStatement(query);
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scoper(stmt);
      nsresult rv = stmt->Execute();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    {
      // Trigger frecency updates for all those origins.
      nsAutoCString query("DELETE FROM moz_updateoriginsupdate_temp");
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
      mozIStorageConnection* aConnection, nsTArray<VisitData>& aPlaces,
      const nsMainThreadPtrHandle<mozIVisitInfoCallback>& aCallback,
      bool aGroupNotifications, bool aIgnoreErrors, bool aIgnoreResults,
      uint32_t aInitialUpdatedCount)
      : Runnable("places::InsertVisitedURIs"),
        mDBConn(aConnection),
        mCallback(aCallback),
        mGroupNotifications(aGroupNotifications),
        mIgnoreErrors(aIgnoreErrors),
        mIgnoreResults(aIgnoreResults),
        mSuccessfulUpdatedCount(aInitialUpdatedCount),
        mHistory(History::GetService()) {
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
  nsresult DoDatabaseInserts(bool aKnown, VisitData& aPlace) {
    MOZ_ASSERT(!NS_IsMainThread(),
               "This should not be called on the main thread");

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
  void FetchReferrerInfo(VisitData& aPlace) {
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
  nsresult AddVisit(VisitData& _place) {
    MOZ_ASSERT(_place.placeId > 0);

    nsresult rv;
    nsCOMPtr<mozIStorageStatement> stmt;
    stmt = mHistory->GetStatement(
        "INSERT INTO moz_historyvisits "
        "(from_visit, place_id, visit_date, visit_type, session) "
        "VALUES (:from_visit, :page_id, :visit_date, :visit_type, 0) ");
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
    rv =
        stmt->BindInt32ByName(NS_LITERAL_CSTRING("visit_type"), transitionType);
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
  nsresult UpdateFrecency(const VisitData& aPlace) {
    MOZ_ASSERT(aPlace.shouldUpdateFrecency);
    MOZ_ASSERT(aPlace.placeId > 0);

    nsresult rv;
    {  // First, set our frecency to the proper value.
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
            "WHERE id = :page_id");
      } else {
        // otherwise, just update the frecency without notifying.
        stmt = mHistory->GetStatement(
            "UPDATE moz_places "
            "SET frecency = CALCULATE_FRECENCY(:page_id, :redirect) "
            "WHERE id = :page_id");
      }
      NS_ENSURE_STATE(stmt);
      mozStorageStatementScoper scoper(stmt);

      rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), aPlace.placeId);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("redirect"),
                                 aPlace.useFrecencyRedirectBonus);
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
          "WHERE id = :page_id AND frecency <> 0");
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
class SetPageTitle : public Runnable {
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
  static nsresult Start(mozIStorageConnection* aConnection, nsIURI* aURI,
                        const nsAString& aTitle) {
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

  NS_IMETHOD Run() override {
    MOZ_ASSERT(!NS_IsMainThread(),
               "This should not be called on the main thread");

    // First, see if the page exists in the database (we'll need its id later).
    bool exists;
    nsresult rv = mHistory->FetchPageInfo(mPlace, &exists);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!exists || !mPlace.titleChanged) {
      // We have no record of this page, or we have no title change, so there
      // is no need to do any further work.
      return NS_OK;
    }

    MOZ_ASSERT(mPlace.placeId > 0, "We somehow have an invalid place id here!");

    // Now we can update our database record.
    nsCOMPtr<mozIStorageStatement> stmt = mHistory->GetStatement(
        "UPDATE moz_places "
        "SET title = :page_title "
        "WHERE id = :page_id ");
    NS_ENSURE_STATE(stmt);

    {
      mozStorageStatementScoper scoper(stmt);
      rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), mPlace.placeId);
      NS_ENSURE_SUCCESS(rv, rv);
      // Empty strings should clear the title, just like
      // nsNavHistory::SetPageTitle.
      if (mPlace.title.IsEmpty()) {
        rv = stmt->BindNullByName(NS_LITERAL_CSTRING("page_title"));
      } else {
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
      : Runnable("places::SetPageTitle"), mHistory(History::GetService()) {
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
 * Stores an embed visit, and notifies observers.
 *
 * @param aPlace
 *        The VisitData of the visit to store as an embed visit.
 * @param [optional] aCallback
 *        The mozIVisitInfoCallback to notify, if provided.
 *
 * FIXME(emilio, bug 1595484): We should get rid of EMBED visits completely.
 */
void NotifyEmbedVisit(VisitData& aPlace,
                      mozIVisitInfoCallback* aCallback = nullptr) {
  MOZ_ASSERT(aPlace.transitionType == nsINavHistoryService::TRANSITION_EMBED,
             "Must only pass TRANSITION_EMBED visits to this!");
  MOZ_ASSERT(NS_IsMainThread(), "Must be called on the main thread!");

  nsCOMPtr<nsIURI> uri;
  MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(uri), aPlace.spec));

  if (!uri) {
    return;
  }

  if (!!aCallback) {
    nsMainThreadPtrHandle<mozIVisitInfoCallback> callback(
        new nsMainThreadPtrHolder<mozIVisitInfoCallback>(
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

////////////////////////////////////////////////////////////////////////////////
//// History

History* History::gService = nullptr;

History::History()
    : mShuttingDown(false),
      mShutdownMutex("History::mShutdownMutex"),
      mRecentlyVisitedURIs(RECENTLY_VISITED_URIS_SIZE) {
  NS_ASSERTION(!gService, "Ruh-roh!  This service has already been created!");
  if (XRE_IsParentProcess()) {
    nsCOMPtr<nsIProperties> dirsvc = services::GetDirectoryService();
    bool haveProfile = false;
    MOZ_RELEASE_ASSERT(
        dirsvc &&
            NS_SUCCEEDED(
                dirsvc->Has(NS_APP_USER_PROFILE_50_DIR, &haveProfile)) &&
            haveProfile,
        "Can't construct history service if there is no profile.");
  }
  gService = this;

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  NS_WARNING_ASSERTION(os, "Observer service was not found!");
  if (os) {
    (void)os->AddObserver(this, TOPIC_PLACES_SHUTDOWN, false);
  }
}

History::~History() {
  UnregisterWeakMemoryReporter(this);

  MOZ_ASSERT(gService == this);
  gService = nullptr;
}

void History::InitMemoryReporter() { RegisterWeakMemoryReporter(this); }

void History::NotifyVisitedParent(const nsTArray<VisitedQueryResult>& aURIs) {
  MOZ_ASSERT(XRE_IsParentProcess());
  nsTArray<ContentParent*> cplist;
  ContentParent::GetAll(cplist);

  for (auto* cp : cplist) {
    Unused << cp->SendNotifyVisited(aURIs);
  }
}

class ConcurrentStatementsHolder final : public mozIStorageCompletionCallback {
 public:
  NS_DECL_ISUPPORTS

  explicit ConcurrentStatementsHolder(mozIStorageConnection* aDBConn)
      : mShutdownWasInvoked(false) {
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
      (void)mReadOnlyDBConn->CreateAsyncStatement(
          NS_LITERAL_CSTRING("SELECT 1 FROM moz_places h "
                             "WHERE url_hash = hash(?1) AND url = ?1 AND "
                             "last_visit_date NOTNULL "),
          getter_AddRefs(mIsVisitedStatement));
      MOZ_ASSERT(mIsVisitedStatement);
      auto queries = std::move(mVisitedQueries);
      if (mIsVisitedStatement) {
        for (auto& query : queries) {
          query->Execute(*mIsVisitedStatement);
        }
      }
    }

    return NS_OK;
  }

  void QueueVisitedStatement(RefPtr<VisitedQuery> aCallback) {
    if (mIsVisitedStatement) {
      aCallback->Execute(*mIsVisitedStatement);
    } else {
      mVisitedQueries.AppendElement(std::move(aCallback));
    }
  }

  void Shutdown() {
    mShutdownWasInvoked = true;
    if (mReadOnlyDBConn) {
      mVisitedQueries.Clear();
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
  ~ConcurrentStatementsHolder() = default;

  nsCOMPtr<mozIStorageAsyncConnection> mReadOnlyDBConn;
  nsCOMPtr<mozIStorageAsyncStatement> mIsVisitedStatement;
  nsTArray<RefPtr<VisitedQuery>> mVisitedQueries;
  bool mShutdownWasInvoked;
};

NS_IMPL_ISUPPORTS(ConcurrentStatementsHolder, mozIStorageCompletionCallback)

nsresult History::QueueVisitedStatement(RefPtr<VisitedQuery> aQuery) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mShuttingDown) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!mConcurrentStatementsHolder) {
    mozIStorageConnection* dbConn = GetDBConn();
    NS_ENSURE_STATE(dbConn);
    mConcurrentStatementsHolder = new ConcurrentStatementsHolder(dbConn);
  }
  mConcurrentStatementsHolder->QueueVisitedStatement(std::move(aQuery));
  return NS_OK;
}

nsresult History::InsertPlace(VisitData& aPlace,
                              bool aShouldNotifyFrecencyChanged) {
  MOZ_ASSERT(aPlace.placeId == 0, "should not have a valid place id!");
  MOZ_ASSERT(!aPlace.shouldUpdateHidden, "We should not need to update hidden");
  MOZ_ASSERT(!NS_IsMainThread(), "must be called off of the main thread!");

  nsCOMPtr<mozIStorageStatement> stmt = GetStatement(
      "INSERT INTO moz_places "
      "(url, url_hash, title, rev_host, hidden, typed, frecency, guid) "
      "VALUES (:url, hash(:url), :title, :rev_host, :hidden, :typed, "
      ":frecency, :guid) ");
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsresult rv =
      stmt->BindStringByName(NS_LITERAL_CSTRING("rev_host"), aPlace.revHost);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("url"), aPlace.spec);
  NS_ENSURE_SUCCESS(rv, rv);
  nsString title = aPlace.title;
  // Empty strings should have no title, just like nsNavHistory::SetPageTitle.
  if (title.IsEmpty()) {
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("title"));
  } else {
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
    navHistory->DispatchFrecencyChangedNotification(
        aPlace.spec, frecency, aPlace.guid, aPlace.hidden, aPlace.visitTime);
  }

  return NS_OK;
}

nsresult History::UpdatePlace(const VisitData& aPlace) {
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
        "WHERE id = :page_id ");
  } else {
    stmt = GetStatement(
        "UPDATE moz_places "
        "SET title = :title, "
        "hidden = :hidden, "
        "typed = :typed, "
        "guid = :guid "
        "WHERE id = :page_id ");
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
  rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("page_id"), aPlace.placeId);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult History::FetchPageInfo(VisitData& _place, bool* _exists) {
  MOZ_ASSERT(!_place.spec.IsEmpty() || !_place.guid.IsEmpty(),
             "must have either a non-empty spec or guid!");
  MOZ_ASSERT(!NS_IsMainThread(), "must be called off of the main thread!");

  nsresult rv;

  // URI takes precedence.
  nsCOMPtr<mozIStorageStatement> stmt;
  bool selectByURI = !_place.spec.IsEmpty();
  if (selectByURI) {
    stmt = GetStatement(
        "SELECT guid, id, title, hidden, typed, frecency, visit_count, "
        "last_visit_date, "
        "(SELECT id FROM moz_historyvisits "
        "WHERE place_id = h.id AND visit_date = h.last_visit_date) AS "
        "last_visit_id "
        "FROM moz_places h "
        "WHERE url_hash = hash(:page_url) AND url = :page_url ");
    NS_ENSURE_STATE(stmt);

    rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), _place.spec);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    stmt = GetStatement(
        "SELECT url, id, title, hidden, typed, frecency, visit_count, "
        "last_visit_date, "
        "(SELECT id FROM moz_historyvisits "
        "WHERE place_id = h.id AND visit_date = h.last_visit_date) AS "
        "last_visit_id "
        "FROM moz_places h "
        "WHERE guid = :guid ");
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
  } else {
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
                        nsISupports* aData, bool aAnonymize) {
  MOZ_COLLECT_REPORT(
      "explicit/history-links-hashtable", KIND_HEAP, UNITS_BYTES,
      SizeOfIncludingThis(HistoryMallocSizeOf),
      "Memory used by the hashtable that records changes to the visited state "
      "of links.");

  return NS_OK;
}

size_t History::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) {
  size_t size = aMallocSizeOf(this);
  size += mTrackedURIs.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (const auto& entry : mTrackedURIs) {
    size += entry.GetData().SizeOfExcludingThis(aMallocSizeOf);
  }
  return size;
}

/* static */
History* History::GetService() {
  if (gService) {
    return gService;
  }

  nsCOMPtr<IHistory> service = services::GetHistoryService();
  if (service) {
    NS_ASSERTION(gService, "Our constructor was not run?!");
  }

  return gService;
}

/* static */
already_AddRefed<History> History::GetSingleton() {
  if (!gService) {
    RefPtr<History> svc = new History();
    MOZ_ASSERT(gService == svc.get());
    svc->InitMemoryReporter();
    return svc.forget();
  }

  return do_AddRef(gService);
}

mozIStorageConnection* History::GetDBConn() {
  MOZ_ASSERT(NS_IsMainThread());
  if (mShuttingDown) return nullptr;
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

const mozIStorageConnection* History::GetConstDBConn() {
  MOZ_ASSERT(mDB || mShuttingDown);
  if (mShuttingDown || !mDB) {
    return nullptr;
  }
  return mDB->MainConn();
}

void History::Shutdown() {
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

void History::AppendToRecentlyVisitedURIs(nsIURI* aURI, bool aHidden) {
  PRTime now = PR_Now();

  {
    RecentURIVisit& visit =
        mRecentlyVisitedURIs.LookupForAdd(aURI).OrInsert([] {
          return RecentURIVisit{0, false};
        });

    visit.mTime = now;
    visit.mHidden = aHidden;
  }

  // Remove entries older than RECENTLY_VISITED_URIS_MAX_AGE.
  for (auto iter = mRecentlyVisitedURIs.Iter(); !iter.Done(); iter.Next()) {
    if ((now - iter.Data().mTime) > RECENTLY_VISITED_URIS_MAX_AGE) {
      iter.Remove();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
//// IHistory

NS_IMETHODIMP
History::VisitURI(nsIWidget* aWidget, nsIURI* aURI, nsIURI* aLastVisitedURI,
                  uint32_t aFlags) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aURI);

  if (mShuttingDown) {
    return NS_OK;
  }

  if (XRE_IsContentProcess()) {
    NS_ENSURE_ARG(aWidget);
    BrowserChild* browserChild = aWidget->GetOwningBrowserChild();
    NS_ENSURE_TRUE(browserChild, NS_ERROR_FAILURE);
    (void)browserChild->SendVisitURI(aURI, aLastVisitedURI, aFlags);
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

  bool reload = false;
  if (aLastVisitedURI) {
    rv = aURI->Equals(aLastVisitedURI, &reload);
    NS_ENSURE_SUCCESS(rv, rv);
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
  } else if (aFlags & IHistory::REDIRECT_TEMPORARY) {
    transitionType = nsINavHistoryService::TRANSITION_REDIRECT_TEMPORARY;
  } else if (aFlags & IHistory::REDIRECT_PERMANENT) {
    transitionType = nsINavHistoryService::TRANSITION_REDIRECT_PERMANENT;
  } else if (reload) {
    transitionType = nsINavHistoryService::TRANSITION_RELOAD;
  } else if ((recentFlags & nsNavHistory::RECENT_TYPED) &&
             !(aFlags & IHistory::UNRECOVERABLE_ERROR)) {
    // Don't mark error pages as typed, even if they were actually typed by
    // the user.  This is useful to limit their score in autocomplete.
    transitionType = nsINavHistoryService::TRANSITION_TYPED;
  } else if (recentFlags & nsNavHistory::RECENT_BOOKMARKED) {
    transitionType = nsINavHistoryService::TRANSITION_BOOKMARK;
  } else if (!(aFlags & IHistory::TOP_LEVEL) && isFollowedLink) {
    // User activated a link in a frame.
    transitionType = nsINavHistoryService::TRANSITION_FRAMED_LINK;
  }

  place.SetTransitionType(transitionType);
  bool isRedirect = aFlags & IHistory::REDIRECT_SOURCE;
  if (isRedirect) {
    place.useFrecencyRedirectBonus =
        (aFlags & IHistory::REDIRECT_SOURCE_PERMANENT) ||
        transitionType != nsINavHistoryService::TRANSITION_TYPED;
  }
  place.hidden = GetHiddenState(isRedirect, place.transitionType);

  // Error pages should never be autocompleted.
  if (aFlags & IHistory::UNRECOVERABLE_ERROR) {
    place.shouldUpdateFrecency = false;
  }

  // Do not save a reloaded uri if we have visited the same URI recently.
  if (reload) {
    auto entry = mRecentlyVisitedURIs.Lookup(aURI);
    // Check if the entry exists and is younger than
    // RECENTLY_VISITED_URIS_MAX_AGE.
    if (entry &&
        (PR_Now() - entry.Data().mTime) < RECENTLY_VISITED_URIS_MAX_AGE) {
      bool wasHidden = entry.Data().mHidden;
      // Regardless of whether we store the visit or not, we must update the
      // stored visit time.
      AppendToRecentlyVisitedURIs(aURI, place.hidden);
      // We always want to store an unhidden visit, if the previous visits were
      // hidden, because otherwise the page may not appear in the history UI.
      // This can happen for example at a page redirecting to itself.
      if (!wasHidden || place.hidden) {
        // We can skip this visit.
        return NS_OK;
      }
    }
  }

  // EMBED visits should not go through the database.
  // They exist only to keep track of isVisited status during the session.
  if (place.transitionType == nsINavHistoryService::TRANSITION_EMBED) {
    NotifyEmbedVisit(place);
  } else {
    mozIStorageConnection* dbConn = GetDBConn();
    NS_ENSURE_STATE(dbConn);

    rv = InsertVisitedURIs::Start(dbConn, placeArray);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
History::SetURITitle(nsIURI* aURI, const nsAString& aTitle) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG(aURI);

  if (mShuttingDown) {
    return NS_OK;
  }

  if (XRE_IsContentProcess()) {
    auto* cpc = dom::ContentChild::GetSingleton();
    MOZ_ASSERT(cpc, "Content Protocol is NULL!");
    Unused << cpc->SendSetURITitle(aURI, PromiseFlatString(aTitle));
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

  mozIStorageConnection* dbConn = GetDBConn();
  NS_ENSURE_STATE(dbConn);

  return SetPageTitle::Start(dbConn, aURI, aTitle);
}

////////////////////////////////////////////////////////////////////////////////
//// mozIAsyncHistory

NS_IMETHODIMP
History::UpdatePlaces(JS::Handle<JS::Value> aPlaceInfos,
                      mozIVisitInfoCallback* aCallback,
                      bool aGroupNotifications, JSContext* aCtx) {
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
      } else {
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
        if (!JS::IsArrayObject(aCtx, visits, &isArray)) {
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
      (void)JS::GetArrayLength(aCtx, visits, &visitsLength);
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
        nsCOMPtr<nsIXPConnect> xpc = nsIXPConnect::XPConnect();
        Unused << xpc->DebugDumpJSStack(false, false, false);
        MOZ_CRASH("invalid time format passed to updatePlaces");
#endif
        return NS_ERROR_INVALID_ARG;
      }
      uint32_t transitionType = 0;
      rv = GetIntFromJSObject(aCtx, visit, "transitionType", &transitionType);
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_ARG_RANGE(transitionType, nsINavHistoryService::TRANSITION_LINK,
                          nsINavHistoryService::TRANSITION_RELOAD);
      data.SetTransitionType(transitionType);
      data.hidden = GetHiddenState(false, transitionType);

      // If the visit is an embed visit, we do not actually add it to the
      // database.
      if (transitionType == nsINavHistoryService::TRANSITION_EMBED) {
        NotifyEmbedVisit(data, aCallback);
        visitData.RemoveLastElement();
        initialUpdatedCount++;
        continue;
      }

      // The referrer is optional.
      nsCOMPtr<nsIURI> referrer =
          GetURIFromJSObject(aCtx, visit, "referrerURI");
      if (referrer) {
        (void)referrer->GetSpec(data.referrerSpec);
      }
    }
  }

  mozIStorageConnection* dbConn = GetDBConn();
  NS_ENSURE_STATE(dbConn);

  nsMainThreadPtrHandle<mozIVisitInfoCallback> callback(
      new nsMainThreadPtrHolder<mozIVisitInfoCallback>("mozIVisitInfoCallback",
                                                       aCallback));

  // It is possible that all of the visits we were passed were dissallowed by
  // CanAddURI, which isn't an error.  If we have no visits to add, however,
  // we should not call InsertVisitedURIs::Start.
  if (visitData.Length()) {
    nsresult rv = InsertVisitedURIs::Start(
        dbConn, visitData, callback, aGroupNotifications, initialUpdatedCount);
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
    nsCOMPtr<nsIRunnable> event =
        new NotifyCompletion(callback, initialUpdatedCount);
    return backgroundThread->Dispatch(event, NS_DISPATCH_NORMAL);
  }

  return NS_OK;
}

NS_IMETHODIMP
History::IsURIVisited(nsIURI* aURI, mozIVisitedStatusCallback* aCallback) {
  NS_ENSURE_STATE(NS_IsMainThread());
  NS_ENSURE_ARG(aURI);
  NS_ENSURE_ARG(aCallback);

  return VisitedQuery::Start(aURI, aCallback);
}

void History::StartPendingVisitedQueries(
    const PendingVisitedQueries& aQueries) {
  if (XRE_IsContentProcess()) {
    nsTArray<RefPtr<nsIURI>> uris(aQueries.Count());
    for (auto iter = aQueries.ConstIter(); !iter.Done(); iter.Next()) {
      uris.AppendElement(iter.Get()->GetKey());
    }
    auto* cpc = mozilla::dom::ContentChild::GetSingleton();
    MOZ_ASSERT(cpc, "Content Protocol is NULL!");
    Unused << cpc->SendStartVisitedQueries(uris);
  } else {
    // TODO(bug 1594368): We could do a single query, as long as we can
    // then notify each URI individually.
    for (auto iter = aQueries.ConstIter(); !iter.Done(); iter.Next()) {
      nsresult queryStatus = VisitedQuery::Start(iter.Get()->GetKey());
      Unused << NS_WARN_IF(NS_FAILED(queryStatus));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
History::Observe(nsISupports* aSubject, const char* aTopic,
                 const char16_t* aData) {
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

NS_IMPL_ISUPPORTS(History, IHistory, mozIAsyncHistory, nsIObserver,
                  nsIMemoryReporter)

}  // namespace places
}  // namespace mozilla
