/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDragServiceProxy.h"
#include "nsIDocument.h"
#include "nsISupportsPrimitives.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/unused.h"
#include "nsContentUtils.h"

NS_IMPL_ISUPPORTS_INHERITED0(nsDragServiceProxy, nsBaseDragService)

nsDragServiceProxy::nsDragServiceProxy()
{
}

nsDragServiceProxy::~nsDragServiceProxy()
{
}

NS_IMETHODIMP
nsDragServiceProxy::InvokeDragSession(nsIDOMNode* aDOMNode,
                                      nsISupportsArray* aArrayTransferables,
                                      nsIScriptableRegion* aRegion,
                                      uint32_t aActionType)
{
  nsresult rv = nsBaseDragService::InvokeDragSession(aDOMNode,
                                                     aArrayTransferables,
                                                     aRegion,
                                                     aActionType);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMDocument> sourceDocument;
  aDOMNode->GetOwnerDocument(getter_AddRefs(sourceDocument));
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(sourceDocument);
  NS_ENSURE_STATE(doc->GetDocShell());
  mozilla::dom::TabChild* child =
    mozilla::dom::TabChild::GetFrom(doc->GetDocShell());
  NS_ENSURE_STATE(child);
  nsTArray<mozilla::dom::IPCDataTransfer> dataTransfers;
  nsContentUtils::TransferablesToIPCTransferables(aArrayTransferables,
                                                  dataTransfers,
                                                  child->Manager(),
                                                  nullptr);

  if (mHasImage || mSelection) {
    nsIntRect dragRect;
    nsPresContext* pc;
    mozilla::RefPtr<mozilla::gfx::SourceSurface> surface;
    DrawDrag(mSourceNode, aRegion, mScreenX, mScreenY,
             &dragRect, &surface, &pc);

    if (surface) {
      mozilla::RefPtr<mozilla::gfx::DataSourceSurface> dataSurface =
        surface->GetDataSurface();
      mozilla::gfx::DataSourceSurface::MappedSurface map;
      dataSurface->Map(mozilla::gfx::DataSourceSurface::MapType::READ, &map);
      mozilla::gfx::IntSize size = dataSurface->GetSize();
      mozilla::CheckedInt32 requiredBytes =
        mozilla::CheckedInt32(map.mStride) * mozilla::CheckedInt32(size.height);
      size_t maxBufLen = requiredBytes.isValid() ? requiredBytes.value() : 0;
      mozilla::gfx::SurfaceFormat format = dataSurface->GetFormat();
      // Surface data handling is totally nuts. This is the magic one needs to
      // know to access the data.
      size_t bufLen =
        maxBufLen - map.mStride + (size.width * BytesPerPixel(format));

      // nsDependentCString wants null-terminated string.
      mozilla::UniquePtr<char[]> dragImageData(new char[maxBufLen + 1]);
      memcpy(dragImageData.get(), reinterpret_cast<char*>(map.mData), bufLen);
      memset(dragImageData.get() + bufLen, 0, maxBufLen - bufLen + 1);
      nsDependentCString dragImage(dragImageData.get(), maxBufLen);

      mozilla::unused <<
        child->SendInvokeDragSession(dataTransfers, aActionType, dragImage,
                                     size.width, size.height, map.mStride,
                                     static_cast<uint8_t>(format),
                                     dragRect.x, dragRect.y);
      dataSurface->Unmap();
      StartDragSession();
      return NS_OK;
    }
  }

  mozilla::unused << child->SendInvokeDragSession(dataTransfers, aActionType,
                                                  nsCString(),
                                                  0, 0, 0, 0, 0, 0);
  StartDragSession();
  return NS_OK;
}
