/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WaylandDMABufSurface_h__
#define WaylandDMABufSurface_h__

#include <stdint.h>
#include "GLContextTypes.h"
#include "mozilla/widget/nsWaylandDisplay.h"

typedef void* EGLImageKHR;
typedef void* EGLSyncKHR;

#define DMABUF_BUFFER_PLANES 4

namespace mozilla {
namespace layers {
class SurfaceDescriptor;
}
}  // namespace mozilla

typedef enum {
  // Use alpha pixel format
  DMABUF_ALPHA = 1 << 0,
  // Surface is used as texture and may be also shared
  DMABUF_TEXTURE = 1 << 1,
  // Automatically create wl_buffer / EGLImage in Create routines.
  DMABUF_CREATE_WL_BUFFER = 1 << 2,
  // Use modifiers. Such dmabuf surface may have more planes
  // and complex internal structure (tiling/compression/etc.)
  // so we can't do direct rendering to it.
  DMABUF_USE_MODIFIERS = 1 << 3,
} WaylandDMABufSurfaceFlags;

class WaylandDMABufSurface {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WaylandDMABufSurface)

  static already_AddRefed<WaylandDMABufSurface> CreateDMABufSurface(
      int aWidth, int aHeight, int aWaylandDMABufSurfaceFlags);
  static already_AddRefed<WaylandDMABufSurface> CreateDMABufSurface(
      const mozilla::layers::SurfaceDescriptor& aDesc);

  bool Create(int aWidth, int aHeight, int aWaylandDMABufSurfaceFlags);
  bool Create(const mozilla::layers::SurfaceDescriptor& aDesc);

  bool Serialize(mozilla::layers::SurfaceDescriptor& aOutDescriptor);

  bool Resize(int aWidth, int aHeight);
  void Clear();

  bool CopyFrom(class WaylandDMABufSurface* aSourceSurface);

  int GetWidth() { return mWidth; };
  int GetHeight() { return mHeight; };
  mozilla::gfx::SurfaceFormat GetFormat();
  bool HasAlpha();

  void* MapReadOnly(uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight,
                    uint32_t* aStride = nullptr);
  void* MapReadOnly(uint32_t* aStride = nullptr);
  void* Map(uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight,
            uint32_t* aStride = nullptr);
  void* Map(uint32_t* aStride = nullptr);
  void* GetMappedRegion() { return mMappedRegion; };
  uint32_t GetMappedRegionStride() { return mMappedRegionStride; };
  bool IsMapped() { return (mMappedRegion != nullptr); };
  void Unmap();

  bool IsEGLSupported(mozilla::gl::GLContext* aGLContext);
  bool CreateEGLImage(mozilla::gl::GLContext* aGLContext);
  void ReleaseEGLImage();
  EGLImageKHR GetEGLImage() { return mEGLImage; };
  GLuint GetGLTexture() { return mTexture; };

  void SetWLBuffer(struct wl_buffer* aWLBuffer);
  wl_buffer* GetWLBuffer();

  void WLBufferDetach() { mWLBufferAttached = false; };
  bool WLBufferIsAttached() { return mWLBufferAttached; };
  void WLBufferSetAttached() { mWLBufferAttached = true; };

  WaylandDMABufSurface();

 private:
  ~WaylandDMABufSurface();

  void ReleaseDMABufSurface();

  bool CreateWLBuffer();

  void FillFdData(struct gbm_import_fd_data& aData);
  void FillFdData(struct gbm_import_fd_modifier_data& aData);
  void ImportSurfaceDescriptor(const mozilla::layers::SurfaceDescriptor& aDesc);

  void* MapInternal(uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight,
                    uint32_t* aStride, int aGbmFlags);

 private:
  int mSurfaceFlags;

  int mWidth;
  int mHeight;
  mozilla::widget::GbmFormat* mGmbFormat;

  wl_buffer* mWLBuffer;
  void* mMappedRegion;
  uint32_t mMappedRegionStride;

  struct gbm_bo* mGbmBufferObject;
  uint64_t mBufferModifier;
  int mBufferPlaneCount;
  int mDmabufFds[DMABUF_BUFFER_PLANES];
  uint32_t mStrides[DMABUF_BUFFER_PLANES];
  uint32_t mOffsets[DMABUF_BUFFER_PLANES];
  uint32_t mGbmBufferFlags;

  RefPtr<mozilla::gl::GLContext> mGL;
  EGLImageKHR mEGLImage;
  GLuint mGLFbo;
  GLuint mTexture;

  bool mWLBufferAttached;
  bool mFastWLBufferCreation;
};

#endif
