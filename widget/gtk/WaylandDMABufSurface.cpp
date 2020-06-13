/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <poll.h>

#include "mozilla/widget/gbm.h"
#include "mozilla/widget/va_drmcommon.h"
#include "GLContextTypes.h"  // for GLContext, etc
#include "GLContextEGL.h"
#include "GLContextProvider.h"

#include "mozilla/layers/LayersSurfaces.h"

/*
TODO:
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

#ifndef VA_FOURCC_NV12
#  define VA_FOURCC_NV12 0x3231564E
#endif

bool WaylandDMABufSurface::IsGlobalRefSet() const {
  if (!mGlobalRefCountFd) {
    return false;
  }
  struct pollfd pfd;
  pfd.fd = mGlobalRefCountFd;
  pfd.events = POLLIN;
  return poll(&pfd, 1, 0) == 1;
}

void WaylandDMABufSurface::GlobalRefRelease() {
  MOZ_ASSERT(mGlobalRefCountFd);
  uint64_t counter;
  if (read(mGlobalRefCountFd, &counter, sizeof(counter)) != sizeof(counter)) {
    // EAGAIN means the refcount is already zero. It happens when we release
    // last reference to the surface.
    if (errno != EAGAIN) {
      NS_WARNING("Failed to unref dmabuf global ref count!");
    }
  }
}

void WaylandDMABufSurface::GlobalRefAdd() {
  MOZ_ASSERT(mGlobalRefCountFd);
  uint64_t counter = 1;
  if (write(mGlobalRefCountFd, &counter, sizeof(counter)) != sizeof(counter)) {
    NS_WARNING("Failed to ref dmabuf global ref count!");
  }
}

void WaylandDMABufSurface::GlobalRefCountCreate() {
  MOZ_ASSERT(!mGlobalRefCountFd);
  mGlobalRefCountFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE);
  if (mGlobalRefCountFd < 0) {
    NS_WARNING("Failed to create dmabuf global ref count!");
    mGlobalRefCountFd = 0;
    return;
  }
}

void WaylandDMABufSurface::GlobalRefCountImport(int aFd) {
  MOZ_ASSERT(!mGlobalRefCountFd);
  mGlobalRefCountFd = aFd;
  GlobalRefAdd();
}

void WaylandDMABufSurface::GlobalRefCountDelete() {
  MOZ_ASSERT(mGlobalRefCountFd);
  if (mGlobalRefCountFd) {
    GlobalRefRelease();
    close(mGlobalRefCountFd);
    mGlobalRefCountFd = 0;
  }
}

WaylandDMABufSurface::WaylandDMABufSurface(SurfaceType aSurfaceType)
    : mSurfaceType(aSurfaceType),
      mBufferModifier(DRM_FORMAT_MOD_INVALID),
      mBufferPlaneCount(0),
      mDrmFormats(),
      mStrides(),
      mOffsets(),
      mSync(0),
      mGlobalRefCountFd(0),
      mUID(0) {
  for (auto& slot : mDmabufFds) {
    slot = -1;
  }
}

WaylandDMABufSurface::~WaylandDMABufSurface() {
  FenceDelete();
  GlobalRefCountDelete();
}

already_AddRefed<WaylandDMABufSurface>
WaylandDMABufSurface::CreateDMABufSurface(
    const mozilla::layers::SurfaceDescriptor& aDesc) {
  const SurfaceDescriptorDMABuf& desc = aDesc.get_SurfaceDescriptorDMABuf();
  RefPtr<WaylandDMABufSurface> surf;

  switch (desc.bufferType()) {
    case SURFACE_RGBA:
      surf = new WaylandDMABufSurfaceRGBA();
      break;
    case SURFACE_NV12:
      surf = new WaylandDMABufSurfaceNV12();
      break;
    default:
      return nullptr;
  }

  if (!surf->Create(desc)) {
    return nullptr;
  }
  return surf.forget();
}

void WaylandDMABufSurface::FenceDelete() {
  auto* egl = gl::GLLibraryEGL::Get();

  if (mSync) {
    // We can't call this unless we have the ext, but we will always have
    // the ext if we have something to destroy.
    egl->fDestroySync(egl->Display(), mSync);
    mSync = nullptr;
  }
}

void WaylandDMABufSurface::FenceSet() {
  if (!mGL || !mGL->MakeCurrent()) {
    return;
  }

  auto* egl = gl::GLLibraryEGL::Get();
  if (egl->IsExtensionSupported(GLLibraryEGL::KHR_fence_sync) &&
      egl->IsExtensionSupported(GLLibraryEGL::ANDROID_native_fence_sync)) {
    if (mSync) {
      MOZ_ALWAYS_TRUE(egl->fDestroySync(egl->Display(), mSync));
      mSync = nullptr;
    }

    mSync = egl->fCreateSync(egl->Display(),
                             LOCAL_EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr);
    if (mSync) {
      mGL->fFlush();
      return;
    }
  }

  // ANDROID_native_fence_sync may not be supported so call glFinish()
  // as a slow path.
  mGL->fFinish();
}

void WaylandDMABufSurface::FenceWait() {
  auto* egl = gl::GLLibraryEGL::Get();

  // Wait on the fence, because presumably we're going to want to read this
  // surface
  if (mSync) {
    egl->fClientWaitSync(egl->Display(), mSync, 0, LOCAL_EGL_FOREVER);
  }
}

bool WaylandDMABufSurface::FenceCreate(int aFd) {
  MOZ_ASSERT(aFd > 0);

  auto* egl = gl::GLLibraryEGL::Get();
  const EGLint attribs[] = {LOCAL_EGL_SYNC_NATIVE_FENCE_FD_ANDROID, aFd,
                            LOCAL_EGL_NONE};
  mSync = egl->fCreateSync(egl->Display(), LOCAL_EGL_SYNC_NATIVE_FENCE_ANDROID,
                           attribs);
  if (!mSync) {
    MOZ_ASSERT(false, "Failed to create GLFence!");
    return false;
  }

  return true;
}

void WaylandDMABufSurfaceRGBA::SetWLBuffer(struct wl_buffer* aWLBuffer) {
  MOZ_ASSERT(mWLBuffer == nullptr, "WLBuffer already assigned!");
  mWLBuffer = aWLBuffer;
}

wl_buffer* WaylandDMABufSurfaceRGBA::GetWLBuffer() { return mWLBuffer; }

static void buffer_release(void* data, wl_buffer* buffer) {
  auto surface = reinterpret_cast<WaylandDMABufSurfaceRGBA*>(data);
  surface->WLBufferDetach();
}

static const struct wl_buffer_listener buffer_listener = {buffer_release};

static void buffer_created(void* data,
                           struct zwp_linux_buffer_params_v1* params,
                           struct wl_buffer* new_buffer) {
  auto surface = static_cast<WaylandDMABufSurfaceRGBA*>(data);

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

WaylandDMABufSurfaceRGBA::WaylandDMABufSurfaceRGBA()
    : WaylandDMABufSurface(SURFACE_RGBA),
      mSurfaceFlags(0),
      mWidth(0),
      mHeight(0),
      mGmbFormat(nullptr),
      mWLBuffer(nullptr),
      mMappedRegion(nullptr),
      mMappedRegionStride(0),
      mGbmBufferObject(nullptr),
      mGbmBufferFlags(0),
      mEGLImage(LOCAL_EGL_NO_IMAGE),
      mTexture(0),
      mWLBufferAttached(false),
      mFastWLBufferCreation(true) {}

WaylandDMABufSurfaceRGBA::~WaylandDMABufSurfaceRGBA() { ReleaseSurface(); }

bool WaylandDMABufSurfaceRGBA::Create(int aWidth, int aHeight,
                                      int aWaylandDMABufSurfaceFlags) {
  MOZ_RELEASE_ASSERT(WaylandDisplayGet());
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

  bool useModifiers = (aWaylandDMABufSurfaceFlags & DMABUF_USE_MODIFIERS) &&
                      mGmbFormat->mModifiersCount > 0;
  if (useModifiers) {
    mGbmBufferObject = nsGbmLib::CreateWithModifiers(
        display->GetGbmDevice(), mWidth, mHeight, mGmbFormat->mFormat,
        mGmbFormat->mModifiers, mGmbFormat->mModifiersCount);
    if (mGbmBufferObject) {
      mBufferModifier = nsGbmLib::GetModifier(mGbmBufferObject);
    }
  }

  // Create without modifiers - use plain/linear format.
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

    mBufferModifier = DRM_FORMAT_MOD_INVALID;
  }

  if (!mGbmBufferObject) {
    return false;
  }

  if (mBufferModifier != DRM_FORMAT_MOD_INVALID) {
    mBufferPlaneCount = nsGbmLib::GetPlaneCount(mGbmBufferObject);
    if (mBufferPlaneCount > DMABUF_BUFFER_PLANES) {
      NS_WARNING("There's too many dmabuf planes!");
      ReleaseSurface();
      return false;
    }

    for (int i = 0; i < mBufferPlaneCount; i++) {
      uint32_t handle = nsGbmLib::GetHandleForPlane(mGbmBufferObject, i).u32;
      int ret = nsGbmLib::DrmPrimeHandleToFD(display->GetGbmDeviceFd(), handle,
                                             0, &mDmabufFds[i]);
      if (ret < 0 || mDmabufFds[i] < 0) {
        ReleaseSurface();
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
      ReleaseSurface();
      return false;
    }
  }

  if (mSurfaceFlags & DMABUF_CREATE_WL_BUFFER) {
    return CreateWLBuffer();
  }

  return true;
}

void WaylandDMABufSurfaceRGBA::ImportSurfaceDescriptor(
    const SurfaceDescriptor& aDesc) {
  const SurfaceDescriptorDMABuf& desc = aDesc.get_SurfaceDescriptorDMABuf();

  mWidth = desc.width()[0];
  mHeight = desc.height()[0];
  mBufferModifier = desc.modifier();
  if (mBufferModifier != DRM_FORMAT_MOD_INVALID) {
    mGmbFormat = WaylandDisplayGet()->GetExactGbmFormat(desc.format()[0]);
  } else {
    mDrmFormats[0] = desc.format()[0];
  }
  mBufferPlaneCount = desc.fds().Length();
  mGbmBufferFlags = desc.flags();
  MOZ_RELEASE_ASSERT(mBufferPlaneCount <= DMABUF_BUFFER_PLANES);
  mUID = desc.uid();

  for (int i = 0; i < mBufferPlaneCount; i++) {
    mDmabufFds[i] = desc.fds()[i].ClonePlatformHandle().release();
    mStrides[i] = desc.strides()[i];
    mOffsets[i] = desc.offsets()[i];
  }

  if (desc.fence().Length() > 0) {
    int fd = desc.fence()[0].ClonePlatformHandle().release();
    if (!FenceCreate(fd)) {
      close(fd);
    }
  }

  if (desc.refCount().Length() > 0) {
    GlobalRefCountImport(desc.refCount()[0].ClonePlatformHandle().release());
  }
}

bool WaylandDMABufSurfaceRGBA::Create(const SurfaceDescriptor& aDesc) {
  ImportSurfaceDescriptor(aDesc);
  return true;
}

bool WaylandDMABufSurfaceRGBA::Serialize(
    mozilla::layers::SurfaceDescriptor& aOutDescriptor) {
  AutoTArray<uint32_t, DMABUF_BUFFER_PLANES> width;
  AutoTArray<uint32_t, DMABUF_BUFFER_PLANES> height;
  AutoTArray<uint32_t, DMABUF_BUFFER_PLANES> format;
  AutoTArray<ipc::FileDescriptor, DMABUF_BUFFER_PLANES> fds;
  AutoTArray<uint32_t, DMABUF_BUFFER_PLANES> strides;
  AutoTArray<uint32_t, DMABUF_BUFFER_PLANES> offsets;
  AutoTArray<uintptr_t, DMABUF_BUFFER_PLANES> images;
  AutoTArray<ipc::FileDescriptor, 1> fenceFDs;
  AutoTArray<ipc::FileDescriptor, 1> refCountFDs;

  width.AppendElement(mWidth);
  height.AppendElement(mHeight);
  format.AppendElement(mGmbFormat->mFormat);
  for (int i = 0; i < mBufferPlaneCount; i++) {
    fds.AppendElement(ipc::FileDescriptor(mDmabufFds[i]));
    strides.AppendElement(mStrides[i]);
    offsets.AppendElement(mOffsets[i]);
  }

  if (mSync) {
    auto* egl = gl::GLLibraryEGL::Get();
    fenceFDs.AppendElement(ipc::FileDescriptor(
        egl->fDupNativeFenceFDANDROID(egl->Display(), mSync)));
  }

  if (mGlobalRefCountFd) {
    refCountFDs.AppendElement(ipc::FileDescriptor(mGlobalRefCountFd));
  }

  aOutDescriptor =
      SurfaceDescriptorDMABuf(mSurfaceType, mBufferModifier, mGbmBufferFlags,
                              fds, width, height, format, strides, offsets,
                              GetYUVColorSpace(), fenceFDs, mUID, refCountFDs);

  return true;
}

bool WaylandDMABufSurfaceRGBA::CreateWLBuffer() {
  nsWaylandDisplay* display = WaylandDisplayGet();
  if (!display->GetDmabuf()) {
    return false;
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

bool WaylandDMABufSurfaceRGBA::CreateTexture(GLContext* aGLContext,
                                             int aPlane) {
  MOZ_ASSERT(!mEGLImage && !mTexture, "EGLImage is already created!");

  nsTArray<EGLint> attribs;
  attribs.AppendElement(LOCAL_EGL_WIDTH);
  attribs.AppendElement(mWidth);
  attribs.AppendElement(LOCAL_EGL_HEIGHT);
  attribs.AppendElement(mHeight);
  attribs.AppendElement(LOCAL_EGL_LINUX_DRM_FOURCC_EXT);
  if (mGmbFormat) {
    attribs.AppendElement(mGmbFormat->mFormat);
  } else {
    attribs.AppendElement(mDrmFormats[0]);
  }
#define ADD_PLANE_ATTRIBS(plane_idx)                                        \
  {                                                                         \
    attribs.AppendElement(LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_FD_EXT);     \
    attribs.AppendElement(mDmabufFds[plane_idx]);                           \
    attribs.AppendElement(LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_OFFSET_EXT); \
    attribs.AppendElement((int)mOffsets[plane_idx]);                        \
    attribs.AppendElement(LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_PITCH_EXT);  \
    attribs.AppendElement((int)mStrides[plane_idx]);                        \
    if (mBufferModifier != DRM_FORMAT_MOD_INVALID) {                        \
      attribs.AppendElement(                                                \
          LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_MODIFIER_LO_EXT);            \
      attribs.AppendElement(mBufferModifier & 0xFFFFFFFF);                  \
      attribs.AppendElement(                                                \
          LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_MODIFIER_HI_EXT);            \
      attribs.AppendElement(mBufferModifier >> 32);                         \
    }                                                                       \
  }
  ADD_PLANE_ATTRIBS(0);
  if (mBufferPlaneCount > 1) ADD_PLANE_ATTRIBS(1);
  if (mBufferPlaneCount > 2) ADD_PLANE_ATTRIBS(2);
  if (mBufferPlaneCount > 3) ADD_PLANE_ATTRIBS(3);
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
  aGLContext->fGenTextures(1, &mTexture);
  aGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
  aGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S,
                             LOCAL_GL_CLAMP_TO_EDGE);
  aGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T,
                             LOCAL_GL_CLAMP_TO_EDGE);
  aGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER,
                             LOCAL_GL_LINEAR);
  aGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER,
                             LOCAL_GL_LINEAR);
  aGLContext->fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_2D, mEGLImage);
  mGL = aGLContext;
  return true;
}

void WaylandDMABufSurfaceRGBA::ReleaseTextures() {
  FenceDelete();

  if (mTexture && mGL->MakeCurrent()) {
    mGL->fDeleteTextures(1, &mTexture);
    mTexture = 0;
    mGL = nullptr;
  }

  if (mEGLImage) {
    auto* egl = gl::GLLibraryEGL::Get();
    egl->fDestroyImage(egl->Display(), mEGLImage);
    mEGLImage = nullptr;
  }
}

void WaylandDMABufSurfaceRGBA::ReleaseSurface() {
  MOZ_ASSERT(!IsMapped(), "We can't release mapped buffer!");

  ReleaseTextures();

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

void* WaylandDMABufSurfaceRGBA::MapInternal(uint32_t aX, uint32_t aY,
                                            uint32_t aWidth, uint32_t aHeight,
                                            uint32_t* aStride, int aGbmFlags) {
  NS_ASSERTION(!IsMapped(), "Already mapped!");
  if (!mGbmBufferObject) {
    NS_WARNING(
        "We can't map WaylandDMABufSurfaceRGBA without mGbmBufferObject");
    return nullptr;
  }

  if (mSurfaceFlags & DMABUF_USE_MODIFIERS) {
    NS_WARNING("We should not map dmabuf surfaces with modifiers!");
  }

  mMappedRegionStride = 0;
  mMappedRegion =
      nsGbmLib::Map(mGbmBufferObject, aX, aY, aWidth, aHeight, aGbmFlags,
                    &mMappedRegionStride, &mMappedRegionData);
  if (aStride) {
    *aStride = mMappedRegionStride;
  }
  return mMappedRegion;
}

void* WaylandDMABufSurfaceRGBA::MapReadOnly(uint32_t aX, uint32_t aY,
                                            uint32_t aWidth, uint32_t aHeight,
                                            uint32_t* aStride) {
  return MapInternal(aX, aY, aWidth, aHeight, aStride, GBM_BO_TRANSFER_READ);
}

void* WaylandDMABufSurfaceRGBA::MapReadOnly(uint32_t* aStride) {
  return MapInternal(0, 0, mWidth, mHeight, aStride, GBM_BO_TRANSFER_READ);
}

void* WaylandDMABufSurfaceRGBA::Map(uint32_t aX, uint32_t aY, uint32_t aWidth,
                                    uint32_t aHeight, uint32_t* aStride) {
  return MapInternal(aX, aY, aWidth, aHeight, aStride,
                     GBM_BO_TRANSFER_READ_WRITE);
}

void* WaylandDMABufSurfaceRGBA::Map(uint32_t* aStride) {
  return MapInternal(0, 0, mWidth, mHeight, aStride,
                     GBM_BO_TRANSFER_READ_WRITE);
}

void WaylandDMABufSurfaceRGBA::Unmap() {
  if (mMappedRegion) {
    nsGbmLib::Unmap(mGbmBufferObject, mMappedRegionData);
    mMappedRegion = nullptr;
    mMappedRegionData = nullptr;
    mMappedRegionStride = 0;
  }
}

bool WaylandDMABufSurfaceRGBA::Resize(int aWidth, int aHeight) {
  if (aWidth == mWidth && aHeight == mHeight) {
    return true;
  }

  if (IsMapped()) {
    NS_WARNING("We can't resize mapped surface!");
    return false;
  }

  ReleaseSurface();
  if (Create(aWidth, aHeight, mSurfaceFlags)) {
    if (mSurfaceFlags & DMABUF_CREATE_WL_BUFFER) {
      return CreateWLBuffer();
    }
  }

  return false;
}

#if 0
// Copy from source surface by GL
#  include "GLBlitHelper.h"

bool WaylandDMABufSurfaceRGBA::CopyFrom(class WaylandDMABufSurface* aSourceSurface,
                                        GLContext* aGLContext) {
  MOZ_ASSERT(aSourceSurface->GetTexture());
  MOZ_ASSERT(GetTexture());

  gfx::IntSize size(GetWidth(), GetHeight());
  aGLContext->BlitHelper()->BlitTextureToTexture(aSourceSurface->GetTexture(),
    GetTexture(), size, size);
  return true;
}
#endif

// TODO - Clear the surface by EGL
void WaylandDMABufSurfaceRGBA::Clear() {
  uint32_t destStride;
  void* destData = Map(&destStride);
  memset(destData, 0, GetHeight() * destStride);
  Unmap();
}

bool WaylandDMABufSurfaceRGBA::HasAlpha() {
  return mGmbFormat ? mGmbFormat->mHasAlpha : true;
}

gfx::SurfaceFormat WaylandDMABufSurfaceRGBA::GetFormat() {
  return HasAlpha() ? gfx::SurfaceFormat::B8G8R8A8
                    : gfx::SurfaceFormat::B8G8R8X8;
}

// GL uses swapped R and B components so report accordingly.
gfx::SurfaceFormat WaylandDMABufSurfaceRGBA::GetFormatGL() {
  return HasAlpha() ? gfx::SurfaceFormat::R8G8B8A8
                    : gfx::SurfaceFormat::R8G8B8X8;
}

already_AddRefed<WaylandDMABufSurfaceRGBA>
WaylandDMABufSurfaceRGBA::CreateDMABufSurface(int aWidth, int aHeight,
                                              int aWaylandDMABufSurfaceFlags) {
  RefPtr<WaylandDMABufSurfaceRGBA> surf = new WaylandDMABufSurfaceRGBA();
  if (!surf->Create(aWidth, aHeight, aWaylandDMABufSurfaceFlags)) {
    return nullptr;
  }
  return surf.forget();
}

already_AddRefed<WaylandDMABufSurfaceNV12>
WaylandDMABufSurfaceNV12::CreateNV12Surface(
    const VADRMPRIMESurfaceDescriptor& aDesc) {
  RefPtr<WaylandDMABufSurfaceNV12> surf = new WaylandDMABufSurfaceNV12();
  if (!surf->Create(aDesc)) {
    return nullptr;
  }
  return surf.forget();
}

WaylandDMABufSurfaceNV12::WaylandDMABufSurfaceNV12()
    : WaylandDMABufSurface(SURFACE_NV12),
      mSurfaceFormat(gfx::SurfaceFormat::NV12),
      mWidth(),
      mHeight(),
      mTexture(),
      mColorSpace(mozilla::gfx::YUVColorSpace::UNKNOWN) {
  for (int i = 0; i < DMABUF_BUFFER_PLANES; i++) {
    mEGLImage[i] = LOCAL_EGL_NO_IMAGE;
  }
}

WaylandDMABufSurfaceNV12::~WaylandDMABufSurfaceNV12() { ReleaseSurface(); }

bool WaylandDMABufSurfaceNV12::Create(
    const VADRMPRIMESurfaceDescriptor& aDesc) {
  if (aDesc.fourcc != VA_FOURCC_NV12) {
    return false;
  }
  if (aDesc.num_layers > DMABUF_BUFFER_PLANES ||
      aDesc.num_objects > DMABUF_BUFFER_PLANES) {
    return false;
  }

  mSurfaceFormat = gfx::SurfaceFormat::NV12;
  mBufferPlaneCount = aDesc.num_layers;
  mBufferModifier = aDesc.objects[0].drm_format_modifier;

  for (unsigned int i = 0; i < aDesc.num_layers; i++) {
    // Intel exports VA-API surfaces in one object,planes have the same FD.
    // AMD exports surfaces in two objects with different FDs.
    bool dupFD = (aDesc.layers[i].object_index[0] != i);
    int fd = aDesc.objects[aDesc.layers[i].object_index[0]].fd;
    mDmabufFds[i] = dupFD ? dup(fd) : fd;

    mDrmFormats[i] = aDesc.layers[i].drm_format;
    mOffsets[i] = aDesc.layers[i].offset[0];
    mStrides[i] = aDesc.layers[i].pitch[0];
    mWidth[i] = aDesc.width >> i;
    mHeight[i] = aDesc.height >> i;
  }

  return true;
}

bool WaylandDMABufSurfaceNV12::Create(const SurfaceDescriptor& aDesc) {
  ImportSurfaceDescriptor(aDesc);
  return true;
}

void WaylandDMABufSurfaceNV12::ImportSurfaceDescriptor(
    const SurfaceDescriptorDMABuf& aDesc) {
  mSurfaceFormat = gfx::SurfaceFormat::NV12;
  mBufferPlaneCount = aDesc.fds().Length();
  mBufferModifier = aDesc.modifier();
  mColorSpace = aDesc.yUVColorSpace();
  mUID = aDesc.uid();

  MOZ_RELEASE_ASSERT(mBufferPlaneCount <= DMABUF_BUFFER_PLANES);
  for (int i = 0; i < mBufferPlaneCount; i++) {
    mDmabufFds[i] = aDesc.fds()[i].ClonePlatformHandle().release();
    mWidth[i] = aDesc.width()[i];
    mHeight[i] = aDesc.height()[i];
    mDrmFormats[i] = aDesc.format()[i];
    mStrides[i] = aDesc.strides()[i];
    mOffsets[i] = aDesc.offsets()[i];
  }

  if (aDesc.fence().Length() > 0) {
    int fd = aDesc.fence()[0].ClonePlatformHandle().release();
    if (!FenceCreate(fd)) {
      close(fd);
    }
  }

  if (aDesc.refCount().Length() > 0) {
    GlobalRefCountImport(aDesc.refCount()[0].ClonePlatformHandle().release());
  }
}

bool WaylandDMABufSurfaceNV12::Serialize(
    mozilla::layers::SurfaceDescriptor& aOutDescriptor) {
  AutoTArray<uint32_t, DMABUF_BUFFER_PLANES> width;
  AutoTArray<uint32_t, DMABUF_BUFFER_PLANES> height;
  AutoTArray<uint32_t, DMABUF_BUFFER_PLANES> format;
  AutoTArray<ipc::FileDescriptor, DMABUF_BUFFER_PLANES> fds;
  AutoTArray<uint32_t, DMABUF_BUFFER_PLANES> strides;
  AutoTArray<uint32_t, DMABUF_BUFFER_PLANES> offsets;
  AutoTArray<ipc::FileDescriptor, 1> fenceFDs;
  AutoTArray<ipc::FileDescriptor, 1> refCountFDs;

  for (int i = 0; i < mBufferPlaneCount; i++) {
    width.AppendElement(mWidth[i]);
    height.AppendElement(mHeight[i]);
    format.AppendElement(mDrmFormats[i]);
    fds.AppendElement(ipc::FileDescriptor(mDmabufFds[i]));
    strides.AppendElement(mStrides[i]);
    offsets.AppendElement(mOffsets[i]);
  }

  if (mSync) {
    auto* egl = gl::GLLibraryEGL::Get();
    fenceFDs.AppendElement(ipc::FileDescriptor(
        egl->fDupNativeFenceFDANDROID(egl->Display(), mSync)));
  }

  if (mGlobalRefCountFd) {
    refCountFDs.AppendElement(ipc::FileDescriptor(mGlobalRefCountFd));
  }

  aOutDescriptor = SurfaceDescriptorDMABuf(
      mSurfaceType, mBufferModifier, 0, fds, width, height, format, strides,
      offsets, GetYUVColorSpace(), fenceFDs, mUID, refCountFDs);
  return true;
}

#if 0
already_AddRefed<gl::GLContext> MyCreateGLContextEGL() {
  nsCString discardFailureId;
  if (!gl::GLLibraryEGL::EnsureInitialized(true, &discardFailureId)) {
    gfxCriticalNote << "Failed to load EGL library: " << discardFailureId.get();
    return nullptr;
  }
  // Create GLContext with dummy EGLSurface.
  RefPtr<gl::GLContext> gl =
      gl::GLContextProviderEGL::CreateForCompositorWidget(nullptr, true, true);
  if (!gl || !gl->MakeCurrent()) {
    gfxCriticalNote << "Failed GL context creation for WebRender: "
                    << gfx::hexa(gl.get());
    return nullptr;
  }
  return gl.forget();
}
#endif

bool WaylandDMABufSurfaceNV12::CreateTexture(GLContext* aGLContext,
                                             int aPlane) {
  MOZ_ASSERT(!mEGLImage[aPlane] && !mTexture[aPlane],
             "EGLImage/Texture is already created!");

  nsTArray<EGLint> attribs;
  attribs.AppendElement(LOCAL_EGL_WIDTH);
  attribs.AppendElement(mWidth[aPlane]);
  attribs.AppendElement(LOCAL_EGL_HEIGHT);
  attribs.AppendElement(mHeight[aPlane]);
  attribs.AppendElement(LOCAL_EGL_LINUX_DRM_FOURCC_EXT);
  attribs.AppendElement(mDrmFormats[aPlane]);
#define ADD_PLANE_ATTRIBS_NV12(plane_idx)                                 \
  attribs.AppendElement(LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_FD_EXT);     \
  attribs.AppendElement(mDmabufFds[aPlane]);                              \
  attribs.AppendElement(LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_OFFSET_EXT); \
  attribs.AppendElement((int)mOffsets[aPlane]);                           \
  attribs.AppendElement(LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_PITCH_EXT);  \
  attribs.AppendElement((int)mStrides[aPlane]);
  ADD_PLANE_ATTRIBS_NV12(0);
#undef ADD_PLANE_ATTRIBS_NV12
  attribs.AppendElement(LOCAL_EGL_NONE);

  auto* egl = gl::GLLibraryEGL::Get();
  mEGLImage[aPlane] = egl->fCreateImage(egl->Display(), LOCAL_EGL_NO_CONTEXT,
                                        LOCAL_EGL_LINUX_DMA_BUF_EXT, nullptr,
                                        attribs.Elements());
  if (mEGLImage[aPlane] == LOCAL_EGL_NO_IMAGE) {
    NS_WARNING("EGLImageKHR creation failed");
    return false;
  }

  aGLContext->MakeCurrent();
  aGLContext->fGenTextures(1, &mTexture[aPlane]);
  aGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture[aPlane]);
  aGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S,
                             LOCAL_GL_CLAMP_TO_EDGE);
  aGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T,
                             LOCAL_GL_CLAMP_TO_EDGE);
  aGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER,
                             LOCAL_GL_LINEAR);
  aGLContext->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER,
                             LOCAL_GL_LINEAR);
  aGLContext->fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_2D, mEGLImage[aPlane]);
  mGL = aGLContext;
  return true;
}

void WaylandDMABufSurfaceNV12::ReleaseTextures() {
  FenceDelete();

  bool textureActive = false;
  for (int i = 0; i < mBufferPlaneCount; i++) {
    if (mTexture[i]) {
      textureActive = true;
      break;
    }
  }

  if (textureActive && mGL->MakeCurrent()) {
    mGL->fDeleteTextures(DMABUF_BUFFER_PLANES, mTexture);
    for (int i = 0; i < DMABUF_BUFFER_PLANES; i++) {
      mTexture[i] = 0;
    }
    mGL = nullptr;
  }

  for (int i = 0; i < mBufferPlaneCount; i++) {
    if (mEGLImage[i]) {
      auto* egl = gl::GLLibraryEGL::Get();
      egl->fDestroyImage(egl->Display(), mEGLImage[i]);
      mEGLImage[i] = nullptr;
    }
  }
}

gfx::SurfaceFormat WaylandDMABufSurfaceNV12::GetFormat() {
  return gfx::SurfaceFormat::NV12;
}

// GL uses swapped R and B components so report accordingly.
gfx::SurfaceFormat WaylandDMABufSurfaceNV12::GetFormatGL() {
  return gfx::SurfaceFormat::NV12;
}

void WaylandDMABufSurfaceNV12::ReleaseSurface() {
  ReleaseTextures();

  for (int i = 0; i < mBufferPlaneCount; i++) {
    if (mDmabufFds[i] >= 0) {
      close(mDmabufFds[i]);
      mDmabufFds[i] = 0;
    }
  }
}
