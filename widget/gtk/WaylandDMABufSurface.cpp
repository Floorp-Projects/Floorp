/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Based on weston/simple-dmabuf-egl.c

#include "WaylandDMABufSurface.h"

#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <dlfcn.h>

#include "mozilla/widget/gbm.h"

using namespace mozilla;
using namespace mozilla::widget;

#ifndef DRM_FORMAT_MOD_INVALID
#  define DRM_FORMAT_MOD_INVALID ((1ULL << 56) - 1)
#endif
#define BUFFER_FLAGS 0

void WaylandDMABufSurface::SetWLBuffer(struct wl_buffer* aWLBuffer) {
  MOZ_ASSERT(mWLBuffer == nullptr, "WLBuffer already assigned!");
  mWLBuffer = aWLBuffer;
}

wl_buffer* WaylandDMABufSurface::GetWLBuffer() { return mWLBuffer; }

static void buffer_release(void* data, wl_buffer* buffer) {
  auto surface = reinterpret_cast<WaylandDMABufSurface*>(data);
  surface->WLBufferDetach();
}

static const struct wl_buffer_listener buffer_listener = {buffer_release};

static void buffer_created(void* data,
                           struct zwp_linux_buffer_params_v1* params,
                           struct wl_buffer* new_buffer) {
  auto surface = static_cast<WaylandDMABufSurface*>(data);

  surface->SetWLBuffer(new_buffer);

  nsWaylandDisplay* display = WaylandDisplayGet();
  /* When not using explicit synchronization listen to wl_buffer.release
   * for release notifications, otherwise we are going to use
   * zwp_linux_buffer_release_v1. */
  if (!display->IsExplicitSyncEnabled()) {
    wl_buffer_add_listener(new_buffer, &buffer_listener, surface);
  }
  zwp_linux_buffer_params_v1_destroy(params);
}

static void buffer_create_failed(void* data,
                                 struct zwp_linux_buffer_params_v1* params) {
  zwp_linux_buffer_params_v1_destroy(params);
}

static const struct zwp_linux_buffer_params_v1_listener params_listener = {
    buffer_created, buffer_create_failed};

WaylandDMABufSurface::WaylandDMABufSurface()
    : mWidth(0),
      mHeight(0),
      mGmbFormat(nullptr),
      mWLBuffer(nullptr),
      mMappedRegion(nullptr),
      mGbmBufferObject(nullptr),
      mBufferModifier(DRM_FORMAT_MOD_INVALID),
      mBufferPlaneCount(1),
      mWLBufferAttached(false),
      mFastWLBufferCreation(true) {
  for (int i = 0; i < DMABUF_BUFFER_PLANES; i++) {
    mDmabufFds[i] = -1;
    mStrides[i] = 0;
    mOffsets[i] = 0;
  }
}

WaylandDMABufSurface::~WaylandDMABufSurface() { Release(); }

bool WaylandDMABufSurface::Create(int aWidth, int aHeight, bool aHasAlpha) {
  MOZ_ASSERT(mWLBuffer == nullptr);

  mWidth = aWidth;
  mHeight = aHeight;

  nsWaylandDisplay* display = WaylandDisplayGet();
  mGmbFormat = display->GetGbmFormat(aHasAlpha);
  if (!mGmbFormat) {
    // Requested DRM format is not supported.
    return false;
  }

  if (nsGbmLib::IsModifierAvailable() && mGmbFormat->mModifiersCount > 0) {
    mGbmBufferObject = nsGbmLib::CreateWithModifiers(
        display->GetGbmDevice(), mWidth, mHeight, mGmbFormat->mFormat,
        mGmbFormat->mModifiers, mGmbFormat->mModifiersCount);
    if (mGbmBufferObject) {
      mBufferModifier = nsGbmLib::GetModifier(mGbmBufferObject);
    }
  }

  if (!mGbmBufferObject) {
    mGbmBufferObject =
        nsGbmLib::Create(display->GetGbmDevice(), mWidth, mHeight,
                         mGmbFormat->mFormat, GBM_BO_USE_RENDERING);
  }

  if (!mGbmBufferObject) {
    return false;
  }

  if (nsGbmLib::IsModifierAvailable() && display->GetGbmDeviceFd() != -1) {
    mBufferPlaneCount = nsGbmLib::GetPlaneCount(mGbmBufferObject);
    for (int i = 0; i < mBufferPlaneCount; i++) {
      uint32_t handle = nsGbmLib::GetHandleForPlane(mGbmBufferObject, i).u32;
      int ret = nsGbmLib::DrmPrimeHandleToFD(display->GetGbmDeviceFd(), handle,
                                             0, &mDmabufFds[i]);
      if (ret < 0 || mDmabufFds[i] < 0) {
        Release();
        return false;
      }
      mStrides[i] = nsGbmLib::GetStrideForPlane(mGbmBufferObject, i);
      mOffsets[i] = nsGbmLib::GetOffset(mGbmBufferObject, i);
    }
  } else {
    mBufferPlaneCount = 1;
    mStrides[0] = nsGbmLib::GetStride(mGbmBufferObject);
    mDmabufFds[0] = nsGbmLib::GetFd(mGbmBufferObject);
    if (mDmabufFds[0] < 0) {
      Release();
      return false;
    }
  }

  struct zwp_linux_buffer_params_v1* params =
      zwp_linux_dmabuf_v1_create_params(display->GetDmabuf());
  for (int i = 0; i < mBufferPlaneCount; i++) {
    zwp_linux_buffer_params_v1_add(params, mDmabufFds[i], i, mOffsets[i],
                                   mStrides[i], mBufferModifier >> 32,
                                   mBufferModifier & 0xffffffff);
  }
  zwp_linux_buffer_params_v1_add_listener(params, &params_listener, this);

  if (mFastWLBufferCreation) {
    mWLBuffer = zwp_linux_buffer_params_v1_create_immed(
        params, mWidth, mHeight, mGmbFormat->mFormat, BUFFER_FLAGS);
    /* When not using explicit synchronization listen to
     * wl_buffer.release for release notifications, otherwise we
     * are going to use zwp_linux_buffer_release_v1. */
    if (!display->IsExplicitSyncEnabled()) {
      wl_buffer_add_listener(mWLBuffer, &buffer_listener, this);
    }
  } else {
    zwp_linux_buffer_params_v1_create(params, mWidth, mHeight,
                                      mGmbFormat->mFormat, BUFFER_FLAGS);
  }

  return true;
}

void WaylandDMABufSurface::Release() {
  MOZ_ASSERT(!IsMapped(), "We can't release mapped buffer!");

  if (mWLBuffer) {
    wl_buffer_destroy(mWLBuffer);
    mWLBuffer = nullptr;
  }

  if (mGbmBufferObject) {
    nsGbmLib::Destroy(mGbmBufferObject);
    mGbmBufferObject = nullptr;
  }

  for (int i = 0; i < mBufferPlaneCount; i++) {
    if (mDmabufFds[i] >= 0) {
      close(mDmabufFds[i]);
      mDmabufFds[i] = 0;
    }
  }
}

void* WaylandDMABufSurface::MapReadOnly(uint32_t aX, uint32_t aY,
                                        uint32_t aWidth, uint32_t aHeight,
                                        uint32_t* aStride) {
  NS_ASSERTION(!IsMapped(), "Already mapped!");
  void* map_data = nullptr;
  *aStride = 0;
  mMappedRegion = nsGbmLib::Map(mGbmBufferObject, aX, aY, aWidth, aHeight,
                                GBM_BO_TRANSFER_READ, aStride, &map_data);
  return mMappedRegion;
}

void* WaylandDMABufSurface::MapReadOnly(uint32_t* aStride) {
  return MapReadOnly(0, 0, mWidth, mHeight, aStride);
}

void* WaylandDMABufSurface::Map(uint32_t aX, uint32_t aY, uint32_t aWidth,
                                uint32_t aHeight, uint32_t* aStride) {
  NS_ASSERTION(!IsMapped(), "Already mapped!");
  void* map_data = nullptr;
  *aStride = 0;
  mMappedRegion = nsGbmLib::Map(mGbmBufferObject, aX, aY, aWidth, aHeight,
                                GBM_BO_TRANSFER_READ_WRITE, aStride, &map_data);
  return mMappedRegion;
}

void* WaylandDMABufSurface::Map(uint32_t* aStride) {
  return Map(0, 0, mWidth, mHeight, aStride);
}

void WaylandDMABufSurface::Unmap() {
  if (mMappedRegion) {
    nsGbmLib::Unmap(mGbmBufferObject, mMappedRegion);
    mMappedRegion = nullptr;
  }
}

bool WaylandDMABufSurface::Resize(int aWidth, int aHeight) {
  if (aWidth == mWidth && aHeight == mHeight) {
    return true;
  }

  if (IsMapped()) {
    NS_WARNING("We can't resize mapped surface!");
    return false;
  }

  Release();
  return Create(aWidth, aHeight, mGmbFormat->mHasAlpha);
}

bool WaylandDMABufSurface::CopyFrom(
    class WaylandDMABufSurface* aSourceSurface) {
  // Not implemented - we should not call that.
  MOZ_CRASH();
}

// TODO - Clear the surface by EGL
void WaylandDMABufSurface::Clear() {
  uint32_t destStride;
  void* destData = Map(&destStride);
  memset(destData, 0, GetHeight() * destStride);
  Unmap();
}
