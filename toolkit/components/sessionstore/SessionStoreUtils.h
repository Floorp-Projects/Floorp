/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStoreUtils_h
#define mozilla_dom_SessionStoreUtils_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/SessionStoreUtilsBinding.h"

class nsIDocument;
class nsGlobalWindowInner;

namespace mozilla {
namespace dom {

class GlobalObject;
struct SSScrollPositionDict;

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

  static void CollectFormData(const GlobalObject& aGlobal,
                              WindowProxyHolder& aWindow,
                              Nullable<CollectedData>& aRetVal);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  static bool RestoreFormData(const GlobalObject& aGlobal, Document& aDocument,
                              const CollectedData& aData);

  static void CollectSessionStorage(
      const GlobalObject& aGlobal, WindowProxyHolder& aWindow,
      Record<nsString, Record<nsString, nsString>>& aRetVal);

  static void RestoreSessionStorage(
      const GlobalObject& aGlobal, nsIDocShell* aDocShell,
      const Record<nsString, Record<nsString, nsString>>& aData);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SessionStoreUtils_h
