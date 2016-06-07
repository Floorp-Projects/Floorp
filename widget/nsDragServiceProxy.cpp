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

nsresult
nsDragServiceProxy::InvokeDragSessionImpl(nsISupportsArray* aArrayTransferables,
                                          nsIScriptableRegion* aRegion,
                                          uint32_t aActionType)
{
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(mSourceDocument);
  NS_ENSURE_STATE(doc->GetDocShell());
  mozilla::dom::TabChild* child =
    mozilla::dom::TabChild::GetFrom(doc->GetDocShell());
  NS_ENSURE_STATE(child);
  nsTArray<mozilla::dom::IPCDataTransfer> dataTransfers;
  nsContentUtils::TransferablesToIPCTransferables(aArrayTransferables,
                                                  dataTransfers,
                                                  false,
                                                  child->Manager(),
                                                  nullptr);

  if (mHasImage || mSelection) {
    nsIntRect dragRect;
    nsPresContext* pc;
    RefPtr<mozilla::gfx::SourceSurface> surface;
    DrawDrag(mSourceNode, aRegion, mScreenX, mScreenY,
             &dragRect, &surface, &pc);

    if (surface) {
      RefPtr<mozilla::gfx::DataSourceSurface> dataSurface =
        surface->GetDataSurface();
      if (dataSurface) {
        size_t length;
        int32_t stride;
        mozilla::UniquePtr<char[]> surfaceData =
          nsContentUtils::GetSurfaceData(WrapNotNull(dataSurface), &length,
                                         &stride);
        nsDependentCString dragImage(surfaceData.get(), length);

        mozilla::gfx::IntSize size = dataSurface->GetSize();
        mozilla::Unused <<
          child->SendInvokeDragSession(dataTransfers, aActionType, dragImage,
                                       size.width, size.height, stride,
                                       static_cast<uint8_t>(dataSurface->GetFormat()),
                                       dragRect.x, dragRect.y);
        StartDragSession();
        return NS_OK;
      }
    }
  }

  mozilla::Unused << child->SendInvokeDragSession(dataTransfers, aActionType,
                                                  nsCString(),
                                                  0, 0, 0, 0, 0, 0);
  StartDragSession();
  return NS_OK;
}
