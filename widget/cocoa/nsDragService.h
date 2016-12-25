/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDragService_h_
#define nsDragService_h_

#include "nsBaseDragService.h"
#include "nsChildView.h"

#include <Cocoa/Cocoa.h>

extern NSString* const kWildcardPboardType;
extern NSString* const kCorePboardType_url;
extern NSString* const kCorePboardType_urld;
extern NSString* const kCorePboardType_urln;
extern NSString* const kCustomTypesPboardType;

class nsDragService : public nsBaseDragService
{
public:
  nsDragService();

  // nsBaseDragService
  virtual nsresult InvokeDragSessionImpl(nsIArray* anArrayTransferables,
                                         nsIScriptableRegion* aRegion,
                                         uint32_t aActionType);
  // nsIDragService
  NS_IMETHOD EndDragSession(bool aDoneDrag);
  NS_IMETHOD UpdateDragImage(nsIDOMNode* aImage, int32_t aImageX, int32_t aImageY);

  // nsIDragSession
  NS_IMETHOD GetData(nsITransferable * aTransferable, uint32_t aItemIndex);
  NS_IMETHOD IsDataFlavorSupported(const char *aDataFlavor, bool *_retval);
  NS_IMETHOD GetNumDropItems(uint32_t * aNumItems);

  void DragMovedWithView(NSDraggingSession* aSession, NSPoint aPoint);

protected:
  virtual ~nsDragService();

private:

  // Creates and returns the drag image for a drag. aImagePoint will be set to
  // the origin of the drag relative to mNativeDragView.
  NSImage* ConstructDragImage(nsIDOMNode* aDOMNode,
                              nsIScriptableRegion* aRegion,
                              NSPoint* aImagePoint);

  // Creates and returns the drag image for a drag. aPoint should be the origin
  // of the drag, for example the mouse coordinate of the mousedown event.
  // aDragRect will be set the area of the drag relative to this.
  NSImage* ConstructDragImage(nsIDOMNode* aDOMNode,
                              nsIScriptableRegion* aRegion,
                              mozilla::CSSIntPoint aPoint,
                              mozilla::LayoutDeviceIntRect* aDragRect);

  bool IsValidType(NSString* availableType, bool allowFileURL);
  NSString* GetStringForType(NSPasteboardItem* item, const NSString* type,
                             bool allowFileURL = false);
  NSString* GetTitleForURL(NSPasteboardItem* item);
  NSString* GetFilePath(NSPasteboardItem* item);

  nsCOMPtr<nsIArray> mDataItems; // only valid for a drag started within gecko
  ChildView* mNativeDragView;
  NSEvent* mNativeDragEvent;

  bool mDragImageChanged;
};

#endif // nsDragService_h_
