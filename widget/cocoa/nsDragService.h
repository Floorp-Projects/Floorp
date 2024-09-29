/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDragService_h_
#define nsDragService_h_

#include "nsBaseDragService.h"
#include "nsChildView.h"

#include <Cocoa/Cocoa.h>

/**
 * Cocoa native nsIDragSession implementation
 */
class nsDragSession : public nsBaseDragSession {
 public:
  // nsIDragSession
  NS_IMETHOD GetData(nsITransferable* aTransferable,
                     uint32_t aItemIndex) override;
  NS_IMETHOD IsDataFlavorSupported(const char* aDataFlavor,
                                   bool* _retval) override;
  NS_IMETHOD GetNumDropItems(uint32_t* aNumItems) override;

  NS_IMETHOD UpdateDragImage(nsINode* aImage, int32_t aImageX,
                             int32_t aImageY) override;

  NS_IMETHOD DragMoved(int32_t aX, int32_t aY) override;

  NSDraggingSession* GetNSDraggingSession() { return mNSDraggingSession; }

  MOZ_CAN_RUN_SCRIPT nsresult
  EndDragSessionImpl(bool aDoneDrag, uint32_t aKeyModifiers) override;

 protected:
  // nsBaseDragSession
  MOZ_CAN_RUN_SCRIPT virtual nsresult InvokeDragSessionImpl(
      nsIWidget* aWidget, nsIArray* anArrayTransferables,
      const mozilla::Maybe<mozilla::CSSIntRegion>& aRegion,
      uint32_t aActionType) override;

  // Creates and returns the drag image for a drag. aImagePoint will be set to
  // the origin of the drag relative to mNativeDragView.
  NSImage* ConstructDragImage(
      nsINode* aDOMNode, const mozilla::Maybe<mozilla::CSSIntRegion>& aRegion,
      NSPoint* aImagePoint);

  // Creates and returns the drag image for a drag. aPoint should be the origin
  // of the drag, for example the mouse coordinate of the mousedown event.
  // aDragRect will be set the area of the drag relative to this.
  NSImage* ConstructDragImage(
      nsINode* aDOMNode, const mozilla::Maybe<mozilla::CSSIntRegion>& aRegion,
      mozilla::CSSIntPoint aPoint, mozilla::LayoutDeviceIntRect* aDragRect);

  nsCOMPtr<nsIArray> mDataItems;  // only valid for a drag started within gecko

  // Native widget object that this drag is over.
  ChildView* mNativeDragView = nil;
  // The native drag session object for this drag.
  NSDraggingSession* mNSDraggingSession = nil;

  NSEvent* mNativeDragEvent = nil;

  bool mDragImageChanged = false;
};

/**
 * Cocoa native nsIDragService implementation
 */
class nsDragService final : public nsBaseDragService {
 public:
  already_AddRefed<nsIDragSession> CreateDragSession() override;
};

#endif  // nsDragService_h_
