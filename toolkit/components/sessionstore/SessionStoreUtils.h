/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStoreUtils_h
#define mozilla_dom_SessionStoreUtils_h

#include "mozilla/Attributes.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/SessionStoreUtilsBinding.h"
#include "SessionStoreData.h"
#include "SessionStoreRestoreData.h"

class nsIDocument;
class nsGlobalWindowInner;

namespace mozilla {
class ErrorResult;

namespace dom {

class CanonicalBrowsingContext;
class GlobalObject;
struct SSScrollPositionDict;
class SSCacheCopy;
class SSSetItemInfo;

namespace sessionstore {
class DocShellRestoreState;
class FormData;
class FormEntry;
class StorageEntry;
}  // namespace sessionstore

class SessionStoreUtils {
 public:
  MOZ_CAN_RUN_SCRIPT
  static void ForEachNonDynamicChildFrame(
      const GlobalObject& aGlobal, WindowProxyHolder& aWindow,
      SessionStoreUtilsFrameCallback& aCallback, ErrorResult& aRv);

  static already_AddRefed<nsISupports> AddDynamicFrameFilteredListener(
      const GlobalObject& aGlobal, EventTarget& aTarget, const nsAString& aType,
      JS::Handle<JS::Value> aListener, bool aUseCapture, bool aMozSystemGroup,
      ErrorResult& aRv);

  static void RemoveDynamicFrameFilteredListener(
      const GlobalObject& aGlobal, EventTarget& aTarget, const nsAString& aType,
      nsISupports* aListener, bool aUseCapture, bool aMozSystemGroup,
      ErrorResult& aRv);

  static void CollectDocShellCapabilities(const GlobalObject& aGlobal,
                                          nsIDocShell* aDocShell,
                                          nsCString& aRetVal);

  static void RestoreDocShellCapabilities(
      nsIDocShell* aDocShell, const nsCString& aDisallowCapabilities);
  static void RestoreDocShellCapabilities(
      const GlobalObject& aGlobal, nsIDocShell* aDocShell,
      const nsCString& aDisallowCapabilities) {
    return RestoreDocShellCapabilities(aDocShell, aDisallowCapabilities);
  }

  static void CollectScrollPosition(const GlobalObject& aGlobal,
                                    WindowProxyHolder& aWindow,
                                    Nullable<CollectedData>& aRetVal);

  static void RestoreScrollPosition(const GlobalObject& aGlobal,
                                    nsGlobalWindowInner& aWindow,
                                    const CollectedData& data);
  static void RestoreScrollPosition(nsGlobalWindowInner& aWindow,
                                    const nsCString& aScrollPosition);

  static uint32_t CollectFormData(Document* aDocument,
                                  sessionstore::FormData& aFormData);

  /*
    @param aDocument: DOMDocument instance to obtain form data for.
    @param aGeneratedCount: the current number of XPath expressions in the
                            returned object.
   */
  template <typename... ArgsT>
  static void CollectFromTextAreaElement(Document& aDocument,
                                         uint16_t& aGeneratedCount,
                                         ArgsT&&... args);
  template <typename... ArgsT>
  static void CollectFromInputElement(Document& aDocument,
                                      uint16_t& aGeneratedCount,
                                      ArgsT&&... args);
  template <typename... ArgsT>
  static void CollectFromSelectElement(Document& aDocument,
                                       uint16_t& aGeneratedCount,
                                       ArgsT&&... args);

  static void CollectFormData(const GlobalObject& aGlobal,
                              WindowProxyHolder& aWindow,
                              Nullable<CollectedData>& aRetVal);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  static bool RestoreFormData(const GlobalObject& aGlobal, Document& aDocument,
                              const CollectedData& aData);
  MOZ_CAN_RUN_SCRIPT static void RestoreFormData(
      Document& aDocument, const nsString& aInnerHTML,
      const nsTArray<SessionStoreRestoreData::Entry>& aEntries);

  static void ComposeInputData(const nsTArray<CollectedInputDataValue>& aData,
                               InputElementData& ret);

  MOZ_CAN_RUN_SCRIPT static already_AddRefed<nsISessionStoreRestoreData>
  ConstructSessionStoreRestoreData(const GlobalObject& aGlobal);

  MOZ_CAN_RUN_SCRIPT static already_AddRefed<Promise> InitializeRestore(
      const GlobalObject& aGlobal, CanonicalBrowsingContext& aContext,
      nsISessionStoreRestoreData* aData, ErrorResult& aError);

  static void RestoreDocShellState(
      nsIDocShell* aDocShell, const sessionstore::DocShellRestoreState& aState);

  static already_AddRefed<Promise> RestoreDocShellState(
      const GlobalObject& aGlobal, CanonicalBrowsingContext& aContext,
      const nsACString& aURL, const nsCString& aDocShellCaps,
      ErrorResult& aError);

  static void RestoreSessionStorageFromParent(
      const GlobalObject& aGlobal, const CanonicalBrowsingContext& aContext,
      const Record<nsCString, Record<nsString, nsString>>& aSessionStorage);

  static nsresult ConstructFormDataValues(
      JSContext* aCx, const nsTArray<sessionstore::FormEntry>& aValues,
      nsTArray<Record<nsString, OwningStringOrBooleanOrObject>::EntryType>&
          aEntries,
      bool aParseSessionData = false);

  static nsresult ConstructSessionStorageValues(
      CanonicalBrowsingContext* aBrowsingContext,
      const nsTArray<SSCacheCopy>& aValues,
      Record<nsCString, Record<nsString, nsString>>& aStorage);

  static bool CopyProperty(JSContext* aCx, JS::Handle<JSObject*> aDst,
                           JS::Handle<JSObject*> aSrc, const nsAString& aName);

  template <typename T>
  static bool CopyChildren(JSContext* aCx, JS::Handle<JSObject*> aDst,
                           const nsTArray<RefPtr<T>>& aChildren) {
    if (!aChildren.IsEmpty()) {
      JS::Rooted<JSObject*> children(
          aCx, JS::NewArrayObject(aCx, aChildren.Length()));

      for (const auto index : IntegerRange(aChildren.Length())) {
        if (!aChildren[index]) {
          continue;
        }

        JS::Rooted<JSObject*> object(aCx);
        aChildren[index]->ToJSON(aCx, &object);

        if (!JS_DefineElement(aCx, children, index, object, JSPROP_ENUMERATE)) {
          return false;
        }
      }

      if (!JS_DefineProperty(aCx, aDst, "children", children,
                             JSPROP_ENUMERATE)) {
        return false;
      }
    }

    return true;
  }
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SessionStoreUtils_h
