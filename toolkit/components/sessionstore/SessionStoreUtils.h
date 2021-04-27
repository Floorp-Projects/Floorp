/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStoreUtils_h
#define mozilla_dom_SessionStoreUtils_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
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

namespace sessionstore {
class FormData;
class FormEntry;
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
      const GlobalObject& aGlobal, nsIDocShell* aDocShell,
      const nsCString& aDisallowCapabilities);

  static void CollectScrollPosition(const GlobalObject& aGlobal,
                                    WindowProxyHolder& aWindow,
                                    Nullable<CollectedData>& aRetVal);

  static void RestoreScrollPosition(const GlobalObject& aGlobal,
                                    nsGlobalWindowInner& aWindow,
                                    const CollectedData& data);
  static void RestoreScrollPosition(nsGlobalWindowInner& aWindow,
                                    const nsCString& aScrollPosition);

  static void CollectFormData(Document* aDocument,
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
  static void RestoreFormData(
      Document& aDocument, const nsString& aInnerHTML,
      const nsTArray<SessionStoreRestoreData::Entry>& aEntries);

  static void CollectedSessionStorage(BrowsingContext* aBrowsingContext,
                                      nsTArray<nsCString>& aOrigins,
                                      nsTArray<nsString>& aKeys,
                                      nsTArray<nsString>& aValues);

  static void RestoreSessionStorage(
      const GlobalObject& aGlobal, nsIDocShell* aDocShell,
      const Record<nsString, Record<nsString, nsString>>& aData);

  static void ComposeInputData(const nsTArray<CollectedInputDataValue>& aData,
                               InputElementData& ret);

  static already_AddRefed<nsISessionStoreRestoreData>
  ConstructSessionStoreRestoreData(const GlobalObject& aGlobal);

  static already_AddRefed<Promise> InitializeRestore(
      const GlobalObject& aGlobal, CanonicalBrowsingContext& aContext,
      nsISessionStoreRestoreData* aData, ErrorResult& aError);

  static nsresult ConstructFormDataValues(
      JSContext* aCx, const nsTArray<sessionstore::FormEntry>& aValues,
      nsTArray<Record<nsString, OwningStringOrBooleanOrObject>::EntryType>&
          aEntries,
      bool aParseSessionData = false);

  static void ResetSessionStore(BrowsingContext* aContext);

#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_THUNDERBIRD) || \
    defined(MOZ_SUITE)
  static constexpr bool NATIVE_LISTENER = false;
#else
  static constexpr bool NATIVE_LISTENER = true;
#endif
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SessionStoreUtils_h
