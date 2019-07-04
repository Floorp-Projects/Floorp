/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WaylandDMABufSurface_h__
#define WaylandDMABufSurface_h__

#include <stdint.h>
#include "mozilla/widget/nsWaylandDisplay.h"

#define DMABUF_BUFFER_PLANES 4

class WaylandDMABufSurface {
 public:
  bool Create(int aWidth, int aHeight, bool aHasAlpha = true);
  bool Resize(int aWidth, int aHeight);
  void Release();
  void Clear();

  bool CopyFrom(class WaylandDMABufSurface* aSourceSurface);

  int GetWidth() { return mWidth; };
  int GetHeight() { return mHeight; };

  void* MapReadOnly(uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight,
                    uint32_t* aStride);
  void* MapReadOnly(uint32_t* aStride);
  void* Map(uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight,
            uint32_t* aStride);
  void* Map(uint32_t* aStride);
  bool IsMapped() { return mMappedRegion; };
  void Unmap();

  void SetWLBuffer(struct wl_buffer* aWLBuffer);
  wl_buffer* GetWLBuffer();

  void WLBufferDetach() { mWLBufferAttached = false; };
  bool WLBufferIsAttached() { return mWLBufferAttached; };
  void WLBufferSetAttached() { mWLBufferAttached = true; };

  WaylandDMABufSurface();
  ~WaylandDMABufSurface();

 private:
  int mWidth;
  int mHeight;
  mozilla::widget::GbmFormat* mGmbFormat;

  wl_buffer* mWLBuffer;
  void* mMappedRegion;

  struct gbm_bo* mGbmBufferObject;
  uint64_t mBufferModifier;
  int mBufferPlaneCount;
  int mDmabufFds[DMABUF_BUFFER_PLANES];
  uint32_t mStrides[DMABUF_BUFFER_PLANES];
  uint32_t mOffsets[DMABUF_BUFFER_PLANES];

  bool mWLBufferAttached;
  bool mFastWLBufferCreation;
};

#endif
