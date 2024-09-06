/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDragService_h__
#define nsDragService_h__

#include "nsBaseDragService.h"
#include <windows.h>
#include <shlobj.h>

struct IDataObject;
class nsDataObjCollection;

/**
 *  Windows native nsIDragSession implementation
 */
class nsDragSession : public nsBaseDragSession {
 public:
  virtual ~nsDragSession();

  // nsIDragSession
  NS_IMETHOD GetData(nsITransferable* aTransferable, uint32_t anItem) override;
  NS_IMETHOD GetNumDropItems(uint32_t* aNumItems) override;
  NS_IMETHOD IsDataFlavorSupported(const char* aDataFlavor,
                                   bool* _retval) override;
  NS_IMETHOD UpdateDragImage(nsINode* aImage, int32_t aImageX,
                             int32_t aImageY) override;
  void SetIDataObject(IDataObject* aDataObj);
  IDataObject* GetDataObject() { return mDataObject; }

  MOZ_CAN_RUN_SCRIPT virtual nsresult InvokeDragSessionImpl(
      nsIWidget* aWidget, nsIArray* anArrayTransferables,
      const mozilla::Maybe<mozilla::CSSIntRegion>& aRegion,
      uint32_t aActionType);

  MOZ_CAN_RUN_SCRIPT nsresult
  EndDragSessionImpl(bool aDoneDrag, uint32_t aKeyModifiers) override;

  MOZ_CAN_RUN_SCRIPT nsresult StartInvokingDragSession(nsIWidget* aWidget,
                                                       IDataObject* aDataObj,
                                                       uint32_t aActionType);

  // A drop occurred within the application vs. outside of it.
  void SetDroppedLocal();

 protected:
  // determine if we have a single data object or one of our private
  // collections
  static bool IsCollectionObject(IDataObject* inDataObj);
  static nsDataObjCollection* GetDataObjCollection(IDataObject* aDataObj);

  // Create a bitmap for drag operations
  bool CreateDragImage(nsINode* aDOMNode,
                       const mozilla::Maybe<mozilla::CSSIntRegion>& aRegion,
                       SHDRAGIMAGE* psdi);

  IDataObject* mDataObject = nullptr;
  bool mSentLocalDropEvent = false;
};

/**
 *  Windows native nsIDragService implementation
 */
class nsDragService final : public nsBaseDragService {
 public:
  already_AddRefed<nsIDragSession> CreateDragSession() override;
};

#endif  // nsDragService_h__
