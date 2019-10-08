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
#include "GLContextTypes.h"  // for GLContext, etc
#include "GLContextEGL.h"
#include "GLContextProvider.h"

#include "mozilla/layers/LayersSurfaces.h"

/*
TODO

Lock/Unlock:
An important detail is that you *must* use DMA_BUF_IOCTL_SYNC to
bracket your actual CPU read/write sequences to the dmabuf. That ioctl
will ensure the appropriate caches are flushed correctly (you might not
notice anything wrong on x86, but on other hardware forgetting to do
that can randomly result in bad data), and I think it also waits for
implicit fences (e.g. if you had GPU write to the dmabuf earlier, to
ensure the operation finished).

DRM device selection:
https://lists.freedesktop.org/archives/wayland-devel/2018-November/039660.html
*/

/* C++ / C typecast macros for special EGL handle values */
#if defined(__cplusplus)
#  define EGL_CAST(type, value) (static_cast<type>(value))
#else
#  define EGL_CAST(type, value) ((type)(value))
#endif

using namespace mozilla;
using namespace mozilla::widget;
using namespace mozilla::gl;
using namespace mozilla::layers;

#ifndef DRM_FORMAT_MOD_INVALID
#  define DRM_FORMAT_MOD_INVALID ((1ULL << 56) - 1)
#endif
#define BUFFER_FLAGS 0

#ifndef GBM_BO_USE_TEXTURING
#  define GBM_BO_USE_TEXTURING (1 << 5)
#endif

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
      mMappedRegionStride(0),
      mGbmBufferObject(nullptr),
      mBufferModifier(DRM_FORMAT_MOD_INVALID),
      mBufferPlaneCount(1),
      mGbmBufferFlags(0),
      mEGLImage(LOCAL_EGL_NO_IMAGE),
      mGLFbo(0),
      mWLBufferAttached(false),
      mFastWLBufferCreation(true) {
  for (int i = 0; i < DMABUF_BUFFER_PLANES; i++) {
    mDmabufFds[i] = -1;
    mStrides[i] = 0;
    mOffsets[i] = 0;
  }
}

WaylandDMABufSurface::~WaylandDMABufSurface() { ReleaseDMABufSurface(); }

bool WaylandDMABufSurface::Create(int aWidth, int aHeight,
                                  int aWaylandDMABufSurfaceFlags) {
  MOZ_ASSERT(mGbmBufferObject == nullptr, "Already created?");

  mSurfaceFlags = aWaylandDMABufSurfaceFlags;
  mWidth = aWidth;
  mHeight = aHeight;

  nsWaylandDisplay* display = WaylandDisplayGet();
  mGmbFormat = display->GetGbmFormat(mSurfaceFlags & DMABUF_ALPHA);
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
    mGbmBufferFlags = (GBM_BO_USE_SCANOUT | GBM_BO_USE_LINEAR);
    if (mSurfaceFlags & DMABUF_CREATE_WL_BUFFER) {
      mGbmBufferFlags |= GBM_BO_USE_RENDERING;
    } else if (mSurfaceFlags & DMABUF_TEXTURE) {
      mGbmBufferFlags |= GBM_BO_USE_TEXTURING;
    }

    if (!nsGbmLib::DeviceIsFormatSupported(
            display->GetGbmDevice(), mGmbFormat->mFormat, mGbmBufferFlags)) {
      mGbmBufferFlags &= ~GBM_BO_USE_SCANOUT;
    }

    mGbmBufferObject =
        nsGbmLib::Create(display->GetGbmDevice(), mWidth, mHeight,
                         mGmbFormat->mFormat, mGbmBufferFlags);
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

  if (mSurfaceFlags & DMABUF_CREATE_WL_BUFFER) {
    return CreateWLBuffer();
  }

  return true;
}

void WaylandDMABufSurface::FillFdData(struct gbm_import_fd_data& aData) {
  aData.fd = mDmabufFds[0];
  aData.width = mWidth;
  aData.height = mHeight;
  aData.stride = mStrides[0];
  aData.format = mGmbFormat->mFormat;
}

void WaylandDMABufSurface::ImportSurfaceDescriptor(
    const SurfaceDescriptor& aDesc) {
  const SurfaceDescriptorDMABuf& desc = aDesc.get_SurfaceDescriptorDMABuf();

  mWidth = desc.width();
  mHeight = desc.height();
  mGmbFormat = WaylandDisplayGet()->GetExactGbmFormat(desc.format());
  mBufferPlaneCount = 1;
  mGbmBufferFlags = desc.flags();
  mDmabufFds[0] = desc.fd().ClonePlatformHandle().release();
  mStrides[0] = desc.stride();
  mOffsets[0] = desc.offset();
}

bool WaylandDMABufSurface::Create(const SurfaceDescriptor& aDesc) {
  MOZ_ASSERT(mGbmBufferObject == nullptr, "Already created?");

  ImportSurfaceDescriptor(aDesc);

  struct gbm_import_fd_data importData;
  FillFdData(importData);
  mGbmBufferObject =
      nsGbmLib::Import(WaylandDisplayGet()->GetGbmDevice(), GBM_BO_IMPORT_FD,
                       &importData, mGbmBufferFlags);

  if (!mGbmBufferObject) {
    Release();
    return false;
  }

  return true;
}

bool WaylandDMABufSurface::Serialize(
    mozilla::layers::SurfaceDescriptor& aOutDescriptor) {
  MOZ_ASSERT(mBufferPlaneCount == 1,
             "We can't export multi-plane dmabuf surfaces!");

  aOutDescriptor = SurfaceDescriptorDMABuf(
      mWidth, mHeight, mGmbFormat->mFormat, mGbmBufferFlags,
      ipc::FileDescriptor(mDmabufFds[0]), mStrides[0], mOffsets[0]);

  return true;
}

bool WaylandDMABufSurface::CreateWLBuffer() {
  nsWaylandDisplay* display = WaylandDisplayGet();
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

bool WaylandDMABufSurface::IsEGLSupported(mozilla::gl::GLContext* aGLContext) {
  auto* egl = gl::GLLibraryEGL::Get();
  return (egl->HasKHRImageBase() &&
          aGLContext->IsExtensionSupported(GLContext::OES_EGL_image_external));
}

bool WaylandDMABufSurface::CreateEGLImage(mozilla::gl::GLContext* aGLContext) {
  MOZ_ASSERT(mGbmBufferObject, "Can't create EGLImage, missing dmabuf object!");
  MOZ_ASSERT(mBufferPlaneCount == 1, "Modifiers are not supported yet!");

  nsTArray<EGLint> attribs;
  attribs.AppendElement(LOCAL_EGL_WIDTH);
  attribs.AppendElement(mWidth);
  attribs.AppendElement(LOCAL_EGL_HEIGHT);
  attribs.AppendElement(mHeight);
  attribs.AppendElement(LOCAL_EGL_LINUX_DRM_FOURCC_EXT);
  attribs.AppendElement(mGmbFormat->mFormat);
#define ADD_PLANE_ATTRIBS(plane_idx)                                        \
  {                                                                         \
    attribs.AppendElement(LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_FD_EXT);     \
    attribs.AppendElement(mDmabufFds[plane_idx]);                           \
    attribs.AppendElement(LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_OFFSET_EXT); \
    attribs.AppendElement((int)mOffsets[plane_idx]);                        \
    attribs.AppendElement(LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_PITCH_EXT);  \
    attribs.AppendElement((int)mStrides[plane_idx]);                        \
  }
  ADD_PLANE_ATTRIBS(0);
#undef ADD_PLANE_ATTRIBS
  attribs.AppendElement(LOCAL_EGL_NONE);

  auto* egl = gl::GLLibraryEGL::Get();
  mEGLImage = egl->fCreateImage(egl->Display(), LOCAL_EGL_NO_CONTEXT,
                                LOCAL_EGL_LINUX_DMA_BUF_EXT, nullptr,
                                attribs.Elements());
  if (mEGLImage == LOCAL_EGL_NO_IMAGE) {
    NS_WARNING("EGLImageKHR creation failed");
    return false;
  }

  aGLContext->MakeCurrent();

  int savedFb = 0;
  aGLContext->fGetIntegerv(LOCAL_GL_FRAMEBUFFER_BINDING, &savedFb);

  GLuint texture;
  aGLContext->fGenTextures(1, &texture);
  aGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, texture);
  aGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S,
                             LOCAL_GL_CLAMP_TO_EDGE);
  aGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T,
                             LOCAL_GL_CLAMP_TO_EDGE);
  aGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER,
                             LOCAL_GL_LINEAR);
  aGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER,
                             LOCAL_GL_LINEAR);

  aGLContext->fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_2D, mEGLImage);
  aGLContext->fGenFramebuffers(1, &mGLFbo);
  aGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mGLFbo);
  aGLContext->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER,
                                    LOCAL_GL_COLOR_ATTACHMENT0,
                                    LOCAL_GL_TEXTURE_2D, texture, 0);
  bool ret = (aGLContext->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER) ==
              LOCAL_GL_FRAMEBUFFER_COMPLETE);
  if (!ret) {
    NS_WARNING("WaylandDMABufSurface - FBO creation failed");
  }
  aGLContext->fDeleteTextures(1, &texture);

  aGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, savedFb);

  mGL = aGLContext;

  return ret;
}

void WaylandDMABufSurface::ReleaseEGLImage() {
  if (mEGLImage) {
    auto* egl = gl::GLLibraryEGL::Get();
    egl->fDestroyImage(egl->Display(), mEGLImage);
    mEGLImage = nullptr;
  }

  if (mGLFbo) {
    if (mGL->MakeCurrent()) {
      mGL->fDeleteFramebuffers(1, &mGLFbo);
    }
    mGLFbo = 0;
  }

  mGL = nullptr;
}

void WaylandDMABufSurface::ReleaseDMABufSurface() {
  MOZ_ASSERT(!IsMapped(), "We can't release mapped buffer!");

  ReleaseEGLImage();

  if (mWLBuffer) {
    wl_buffer_destroy(mWLBuffer);
    mWLBuffer = nullptr;
  }

  for (int i = 0; i < mBufferPlaneCount; i++) {
    if (mDmabufFds[i] >= 0) {
      close(mDmabufFds[i]);
      mDmabufFds[i] = 0;
    }
  }

  if (mGbmBufferObject) {
    nsGbmLib::Destroy(mGbmBufferObject);
    mGbmBufferObject = nullptr;
  }
}

void* WaylandDMABufSurface::MapReadOnly(uint32_t aX, uint32_t aY,
                                        uint32_t aWidth, uint32_t aHeight,
                                        uint32_t* aStride) {
  NS_ASSERTION(!IsMapped(), "Already mapped!");
  void* map_data = nullptr;
  mMappedRegionStride = 0;
  mMappedRegion =
      nsGbmLib::Map(mGbmBufferObject, aX, aY, aWidth, aHeight,
                    GBM_BO_TRANSFER_READ, &mMappedRegionStride, &map_data);
  if (aStride) {
    *aStride = mMappedRegionStride;
  }
  return mMappedRegion;
}

void* WaylandDMABufSurface::MapReadOnly(uint32_t* aStride) {
  return MapReadOnly(0, 0, mWidth, mHeight, aStride);
}

void* WaylandDMABufSurface::Map(uint32_t aX, uint32_t aY, uint32_t aWidth,
                                uint32_t aHeight, uint32_t* aStride) {
  NS_ASSERTION(!IsMapped(), "Already mapped!");
  void* map_data = nullptr;
  mMappedRegionStride = 0;
  mMappedRegion = nsGbmLib::Map(mGbmBufferObject, aX, aY, aWidth, aHeight,
                                GBM_BO_TRANSFER_READ_WRITE,
                                &mMappedRegionStride, &map_data);
  if (aStride) {
    *aStride = mMappedRegionStride;
  }
  return mMappedRegion;
}

void* WaylandDMABufSurface::Map(uint32_t* aStride) {
  return Map(0, 0, mWidth, mHeight, aStride);
}

void WaylandDMABufSurface::Unmap() {
  if (mMappedRegion) {
    nsGbmLib::Unmap(mGbmBufferObject, mMappedRegion);
    mMappedRegion = nullptr;
    mMappedRegionStride = 0;
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
  if (Create(aWidth, aHeight, mSurfaceFlags)) {
    if (mSurfaceFlags & DMABUF_CREATE_WL_BUFFER) {
      return CreateWLBuffer();
    }
  }

  return false;
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

bool WaylandDMABufSurface::HasAlpha() {
  return mGmbFormat ? mGmbFormat->mHasAlpha : true;
}

gfx::SurfaceFormat WaylandDMABufSurface::GetFormat() {
  return HasAlpha() ? gfx::SurfaceFormat::B8G8R8A8
                    : gfx::SurfaceFormat::B8G8R8X8;
}

already_AddRefed<WaylandDMABufSurface>
WaylandDMABufSurface::CreateDMABufSurface(int aWidth, int aHeight,
                                          int aWaylandDMABufSurfaceFlags) {
  RefPtr<WaylandDMABufSurface> surf = new WaylandDMABufSurface();
  surf->Create(aWidth, aHeight, aWaylandDMABufSurfaceFlags);
  return surf.forget();
}

already_AddRefed<WaylandDMABufSurface>
WaylandDMABufSurface::CreateDMABufSurface(
    const mozilla::layers::SurfaceDescriptor& aDesc) {
  RefPtr<WaylandDMABufSurface> surf = new WaylandDMABufSurface();
  surf->Create(aDesc);
  return surf.forget();
}
