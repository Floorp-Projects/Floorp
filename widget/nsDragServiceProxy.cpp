/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDragServiceProxy.h"
#include "mozilla/dom/Document.h"
#include "nsISupportsPrimitives.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "nsContentUtils.h"

using mozilla::CSSIntRegion;
using mozilla::LayoutDeviceIntRect;
using mozilla::Maybe;
using mozilla::dom::BrowserChild;
using mozilla::gfx::DataSourceSurface;
using mozilla::gfx::SourceSurface;
using mozilla::gfx::SurfaceFormat;
using mozilla::ipc::Shmem;

nsDragServiceProxy::nsDragServiceProxy() {}

nsDragServiceProxy::~nsDragServiceProxy() {}

nsresult nsDragServiceProxy::InvokeDragSessionImpl(
    nsIArray* aArrayTransferables, const Maybe<CSSIntRegion>& aRegion,
    uint32_t aActionType) {
  NS_ENSURE_STATE(mSourceDocument->GetDocShell());
  BrowserChild* child = BrowserChild::GetFrom(mSourceDocument->GetDocShell());
  NS_ENSURE_STATE(child);
  nsTArray<mozilla::dom::IPCDataTransfer> dataTransfers;
  nsContentUtils::TransferablesToIPCTransferables(
      aArrayTransferables, dataTransfers, false, child->Manager(), nullptr);

  nsCOMPtr<nsIPrincipal> principal;
  if (mSourceNode) {
    principal = mSourceNode->NodePrincipal();
  }

  LayoutDeviceIntRect dragRect;
  if (mHasImage || mSelection) {
    nsPresContext* pc;
    RefPtr<SourceSurface> surface;
    DrawDrag(mSourceNode, aRegion, mScreenPosition, &dragRect, &surface, &pc);

    if (surface) {
      RefPtr<DataSourceSurface> dataSurface = surface->GetDataSurface();
      if (dataSurface) {
        size_t length;
        int32_t stride;
        Maybe<Shmem> maybeShm = nsContentUtils::GetSurfaceData(
            dataSurface, &length, &stride, child);
        if (maybeShm.isNothing()) {
          return NS_ERROR_FAILURE;
        }

        auto surfaceData = maybeShm.value();

        // Save the surface data to shared memory.
        if (!surfaceData.IsReadable() || !surfaceData.get<char>()) {
          NS_WARNING("Failed to create shared memory for drag session.");
          return NS_ERROR_FAILURE;
        }

        mozilla::Unused << child->SendInvokeDragSession(
            dataTransfers, aActionType, Some(std::move(surfaceData)), stride,
            dataSurface->GetFormat(), dragRect, IPC::Principal(principal));
        StartDragSession();
        return NS_OK;
      }
    }
  }

  mozilla::Unused << child->SendInvokeDragSession(
      dataTransfers, aActionType, Nothing(), 0, static_cast<SurfaceFormat>(0),
      dragRect, IPC::Principal(principal));
  StartDragSession();
  return NS_OK;
}
