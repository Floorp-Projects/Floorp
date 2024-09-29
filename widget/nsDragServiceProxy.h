/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef NSDRAGSERVICEPROXY_H
#define NSDRAGSERVICEPROXY_H

#include "nsBaseDragService.h"

class nsDragSessionProxy : public nsBaseDragSession {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(nsDragSessionProxy, nsBaseDragSession)

  MOZ_CAN_RUN_SCRIPT virtual nsresult InvokeDragSession(
      nsIWidget* aWidget, nsINode* aDOMNode, nsIPrincipal* aPrincipal,
      nsIContentSecurityPolicy* aCsp, nsICookieJarSettings* aCookieJarSettings,
      nsIArray* aTransferableArray, uint32_t aActionType,
      nsContentPolicyType aContentPolicyType) override;

  nsresult InvokeDragSessionImpl(
      nsIWidget* aWidget, nsIArray* anArrayTransferables,
      const mozilla::Maybe<mozilla::CSSIntRegion>& aRegion,
      uint32_t aActionType) override;

  void SetDragTarget(mozilla::dom::BrowserChild* aTarget);

  MOZ_CAN_RUN_SCRIPT
  nsresult EndDragSessionImpl(bool aDoneDrag, uint32_t aKeyModifiers) override;

 private:
  ~nsDragSessionProxy();

  // The source for this drag.  This is null if the source is a different
  // application or drag session.
  nsWeakPtr mSourceBrowser;
  // The target for this drag.  This is null if the target is a different
  // application or drag session.
  nsWeakPtr mTargetBrowser;
};

class nsDragServiceProxy : public nsBaseDragService {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(nsDragServiceProxy, nsBaseDragService)

  already_AddRefed<nsIDragSession> CreateDragSession() override;

  nsIDragSession* StartDragSession(nsISupports* aWidgetProvider) override;

  NS_IMETHOD GetCurrentSession(nsISupports* aWidgetProvider,
                               nsIDragSession** aSession) override;

 private:
  virtual ~nsDragServiceProxy();
};

#endif  // NSDRAGSERVICEPROXY_H
