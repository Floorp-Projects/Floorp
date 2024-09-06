/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDragService_h__
#define nsDragService_h__

#include "nsBaseDragService.h"

#include "AndroidGraphics.h"
#include "mozilla/java/GeckoDragAndDropNatives.h"

class nsITransferable;

// Temporary inheritance from nsBaseDragService instead of nsBaseDragSession
// (which nsBaseDragService temporarily inherits).
// This will be undone at the end of this patch series.
class nsDragSession : public nsBaseDragService {
 public:
  nsDragSession() = default;

  NS_DECL_ISUPPORTS_INHERITED

  // nsIDragSession
  NS_IMETHOD GetData(nsITransferable* aTransferable, uint32_t anItem) override;
  NS_IMETHOD GetNumDropItems(uint32_t* aNumItems) override;
  NS_IMETHOD IsDataFlavorSupported(const char* aDataFlavor,
                                   bool* _retval) override;
  NS_IMETHOD
  UpdateDragImage(nsINode* aImage, int32_t aImageX, int32_t aImageY) override;

  void SetData(nsITransferable* aTransferable);

  virtual bool MustUpdateDataTransfer(mozilla::EventMessage aMessage) override;

  MOZ_CAN_RUN_SCRIPT nsresult EndDragSessionImpl(
      bool aDoneDrag, uint32_t aKeyModifiers) override;

 protected:
  virtual ~nsDragSession() = default;

  mozilla::java::sdk::Bitmap::LocalRef CreateDragImage(
      nsINode* aNode, const mozilla::Maybe<mozilla::CSSIntRegion>& aRegion);

  // our source data items
  nsCOMPtr<nsITransferable> mTransferable;
};

// Temporary inheritance from nsDragSession instead of nsBaseDragService
// (which nsDragSession temporarily inherits).
// This will be undone at the end of this patch series.
class nsDragService final : public nsDragSession {
 public:
  static already_AddRefed<nsDragService> GetInstance();

  NS_DECL_ISUPPORTS_INHERITED

  static void SetDropData(
      mozilla::java::GeckoDragAndDrop::DropData::Param aDropData);

 protected:
  virtual ~nsDragService() = default;

  // nsBaseDragService
  MOZ_CAN_RUN_SCRIPT nsresult
  InvokeDragSessionImpl(nsIWidget* aWidget, nsIArray* anArrayTransferables,
                        const mozilla::Maybe<mozilla::CSSIntRegion>& aRegion,
                        uint32_t aActionType) override;
};

#endif  // nsDragService_h__
