/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DMABufSurface.h"

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

#ifndef VA_FOURCC_YV12
#  define VA_FOURCC_YV12 0x32315659
#endif

static Atomic<int> gNewSurfaceUID(1);

bool DMABufSurface::IsGlobalRefSet() const {
  if (!mGlobalRefCountFd) {
    return false;
  }
  struct pollfd pfd;
  pfd.fd = mGlobalRefCountFd;
  pfd.events = POLLIN;
  return poll(&pfd, 1, 0) == 1;
}

void DMABufSurface::GlobalRefRelease() {
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

void DMABufSurface::GlobalRefAdd() {
  MOZ_ASSERT(mGlobalRefCountFd);
  uint64_t counter = 1;
  if (write(mGlobalRefCountFd, &counter, sizeof(counter)) != sizeof(counter)) {
    NS_WARNING("Failed to ref dmabuf global ref count!");
  }
}

void DMABufSurface::GlobalRefCountCreate() {
  MOZ_ASSERT(!mGlobalRefCountFd);
  mGlobalRefCountFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE);
  if (mGlobalRefCountFd < 0) {
    NS_WARNING("Failed to create dmabuf global ref count!");
    mGlobalRefCountFd = 0;
    return;
  }
}

void DMABufSurface::GlobalRefCountImport(int aFd) {
  MOZ_ASSERT(!mGlobalRefCountFd);
  mGlobalRefCountFd = aFd;
  GlobalRefAdd();
}

void DMABufSurface::GlobalRefCountDelete() {
  if (mGlobalRefCountFd) {
    GlobalRefRelease();
    close(mGlobalRefCountFd);
    mGlobalRefCountFd = 0;
  }
}

void DMABufSurface::ReleaseDMABuf() {
  for (int i = 0; i < mBufferPlaneCount; i++) {
    Unmap(i);

    if (mDmabufFds[i] >= 0) {
      close(mDmabufFds[i]);
      mDmabufFds[i] = -1;
    }
  }

  if (mGbmBufferObject[0]) {
    nsGbmLib::Destroy(mGbmBufferObject[0]);
    mGbmBufferObject[0] = nullptr;
  }
}

DMABufSurface::DMABufSurface(SurfaceType aSurfaceType)
    : mSurfaceType(aSurfaceType),
      mBufferModifier(DRM_FORMAT_MOD_INVALID),
      mBufferPlaneCount(0),
      mDrmFormats(),
      mStrides(),
      mOffsets(),
      mGbmBufferObject(),
      mMappedRegion(),
      mMappedRegionStride(),
      mSyncFd(-1),
      mSync(0),
      mGlobalRefCountFd(0),
      mUID(gNewSurfaceUID++) {
  for (auto& slot : mDmabufFds) {
    slot = -1;
  }
}

DMABufSurface::~DMABufSurface() {
  FenceDelete();
  GlobalRefCountDelete();
}

already_AddRefed<DMABufSurface> DMABufSurface::CreateDMABufSurface(
    const mozilla::layers::SurfaceDescriptor& aDesc) {
  const SurfaceDescriptorDMABuf& desc = aDesc.get_SurfaceDescriptorDMABuf();
  RefPtr<DMABufSurface> surf;

  switch (desc.bufferType()) {
    case SURFACE_RGBA:
      surf = new DMABufSurfaceRGBA();
      break;
    case SURFACE_NV12:
    case SURFACE_YUV420:
      surf = new DMABufSurfaceYUV();
      break;
    default:
      return nullptr;
  }

  if (!surf->Create(desc)) {
    return nullptr;
  }
  return surf.forget();
}

void DMABufSurface::FenceDelete() {
  if (!mGL) return;
  const auto& gle = gl::GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;

  if (mSyncFd > 0) {
    close(mSyncFd);
    mSyncFd = -1;
  }

  if (mSync) {
    egl->fDestroySync(mSync);
    mSync = nullptr;
  }
}

void DMABufSurface::FenceSet() {
  if (!mGL || !mGL->MakeCurrent()) {
    return;
  }
  const auto& gle = gl::GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;

  if (egl->IsExtensionSupported(EGLExtension::KHR_fence_sync) &&
      egl->IsExtensionSupported(EGLExtension::ANDROID_native_fence_sync)) {
    FenceDelete();

    mSync = egl->fCreateSync(LOCAL_EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr);
    if (mSync) {
      mSyncFd = egl->fDupNativeFenceFDANDROID(mSync);
      mGL->fFlush();
      return;
    }
  }

  // ANDROID_native_fence_sync may not be supported so call glFinish()
  // as a slow path.
  mGL->fFinish();
}

void DMABufSurface::FenceWait() {
  if (!mGL) return;
  const auto& gle = gl::GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;

  if (!mSync && mSyncFd > 0) {
    FenceImportFromFd();
  }

  // Wait on the fence, because presumably we're going to want to read this
  // surface
  if (mSync) {
    egl->fClientWaitSync(mSync, 0, LOCAL_EGL_FOREVER);
  }
}

bool DMABufSurface::FenceImportFromFd() {
  if (!mGL) return false;
  const auto& gle = gl::GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;

  const EGLint attribs[] = {LOCAL_EGL_SYNC_NATIVE_FENCE_FD_ANDROID, mSyncFd,
                            LOCAL_EGL_NONE};
  mSync = egl->fCreateSync(LOCAL_EGL_SYNC_NATIVE_FENCE_ANDROID, attribs);
  close(mSyncFd);
  mSyncFd = -1;

  if (!mSync) {
    MOZ_ASSERT(false, "Failed to create GLFence!");
    return false;
  }

  return true;
}

void DMABufSurfaceRGBA::SetWLBuffer(struct wl_buffer* aWLBuffer) {
  MOZ_ASSERT(mWLBuffer == nullptr, "WLBuffer already assigned!");
  mWLBuffer = aWLBuffer;
}

wl_buffer* DMABufSurfaceRGBA::GetWLBuffer() { return mWLBuffer; }

static void buffer_release(void* data, wl_buffer* buffer) {
  auto surface = reinterpret_cast<DMABufSurfaceRGBA*>(data);
  surface->WLBufferDetach();
}

static const struct wl_buffer_listener buffer_listener = {buffer_release};

static void buffer_created(void* data,
                           struct zwp_linux_buffer_params_v1* params,
                           struct wl_buffer* new_buffer) {
  auto surface = static_cast<DMABufSurfaceRGBA*>(data);

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

DMABufSurfaceRGBA::DMABufSurfaceRGBA()
    : DMABufSurface(SURFACE_RGBA),
      mSurfaceFlags(0),
      mWidth(0),
      mHeight(0),
      mGmbFormat(nullptr),
      mWLBuffer(nullptr),
      mEGLImage(LOCAL_EGL_NO_IMAGE),
      mTexture(0),
      mGbmBufferFlags(0),
      mWLBufferAttached(false),
      mFastWLBufferCreation(true) {}

DMABufSurfaceRGBA::~DMABufSurfaceRGBA() { ReleaseSurface(); }

bool DMABufSurfaceRGBA::Create(int aWidth, int aHeight,
                               int aDMABufSurfaceFlags) {
  MOZ_ASSERT(mGbmBufferObject[0] == nullptr, "Already created?");

  mSurfaceFlags = aDMABufSurfaceFlags;
  mWidth = aWidth;
  mHeight = aHeight;

  mGmbFormat = GetDMABufDevice()->GetGbmFormat(mSurfaceFlags & DMABUF_ALPHA);
  if (!mGmbFormat) {
    // Requested DRM format is not supported.
    return false;
  }

  bool useModifiers = (aDMABufSurfaceFlags & DMABUF_USE_MODIFIERS) &&
                      mGmbFormat->mModifiersCount > 0;
  if (useModifiers) {
    mGbmBufferObject[0] = nsGbmLib::CreateWithModifiers(
        GetDMABufDevice()->GetGbmDevice(), mWidth, mHeight, mGmbFormat->mFormat,
        mGmbFormat->mModifiers, mGmbFormat->mModifiersCount);
    if (mGbmBufferObject[0]) {
      mBufferModifier = nsGbmLib::GetModifier(mGbmBufferObject[0]);
    }
  }

  // Create without modifiers - use plain/linear format.
  if (!mGbmBufferObject[0]) {
    mGbmBufferFlags = (GBM_BO_USE_SCANOUT | GBM_BO_USE_LINEAR);
    if (mSurfaceFlags & DMABUF_CREATE_WL_BUFFER) {
      mGbmBufferFlags |= GBM_BO_USE_RENDERING;
    } else if (mSurfaceFlags & DMABUF_TEXTURE) {
      mGbmBufferFlags |= GBM_BO_USE_TEXTURING;
    }

    if (!nsGbmLib::DeviceIsFormatSupported(GetDMABufDevice()->GetGbmDevice(),
                                           mGmbFormat->mFormat,
                                           mGbmBufferFlags)) {
      mGbmBufferFlags &= ~GBM_BO_USE_SCANOUT;
    }

    mGbmBufferObject[0] =
        nsGbmLib::Create(GetDMABufDevice()->GetGbmDevice(), mWidth, mHeight,
                         mGmbFormat->mFormat, mGbmBufferFlags);

    mBufferModifier = DRM_FORMAT_MOD_INVALID;
  }

  if (!mGbmBufferObject[0]) {
    return false;
  }

  if (mBufferModifier != DRM_FORMAT_MOD_INVALID) {
    mBufferPlaneCount = nsGbmLib::GetPlaneCount(mGbmBufferObject[0]);
    if (mBufferPlaneCount > DMABUF_BUFFER_PLANES) {
      NS_WARNING("There's too many dmabuf planes!");
      ReleaseSurface();
      return false;
    }

    for (int i = 0; i < mBufferPlaneCount; i++) {
      uint32_t handle = nsGbmLib::GetHandleForPlane(mGbmBufferObject[0], i).u32;
      int ret = nsGbmLib::DrmPrimeHandleToFD(
          GetDMABufDevice()->GetGbmDeviceFd(), handle, 0, &mDmabufFds[i]);
      if (ret < 0 || mDmabufFds[i] < 0) {
        ReleaseSurface();
        return false;
      }
      mStrides[i] = nsGbmLib::GetStrideForPlane(mGbmBufferObject[0], i);
      mOffsets[i] = nsGbmLib::GetOffset(mGbmBufferObject[0], i);
    }
  } else {
    mBufferPlaneCount = 1;
    mStrides[0] = nsGbmLib::GetStride(mGbmBufferObject[0]);
    mDmabufFds[0] = nsGbmLib::GetFd(mGbmBufferObject[0]);
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

void DMABufSurfaceRGBA::ImportSurfaceDescriptor(
    const SurfaceDescriptor& aDesc) {
  const SurfaceDescriptorDMABuf& desc = aDesc.get_SurfaceDescriptorDMABuf();

  mWidth = desc.width()[0];
  mHeight = desc.height()[0];
  mBufferModifier = desc.modifier();
  if (mBufferModifier != DRM_FORMAT_MOD_INVALID) {
    mGmbFormat = GetDMABufDevice()->GetExactGbmFormat(desc.format()[0]);
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
    mSyncFd = desc.fence()[0].ClonePlatformHandle().release();
  }

  if (desc.refCount().Length() > 0) {
    GlobalRefCountImport(desc.refCount()[0].ClonePlatformHandle().release());
  }
}

bool DMABufSurfaceRGBA::Create(const SurfaceDescriptor& aDesc) {
  ImportSurfaceDescriptor(aDesc);
  return true;
}

bool DMABufSurfaceRGBA::Serialize(
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
    fenceFDs.AppendElement(ipc::FileDescriptor(mSyncFd));
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

bool DMABufSurfaceRGBA::CreateWLBuffer() {
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

bool DMABufSurfaceRGBA::CreateTexture(GLContext* aGLContext, int aPlane) {
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

  if (!aGLContext) return false;
  const auto& gle = gl::GLContextEGL::Cast(aGLContext);
  const auto& egl = gle->mEgl;
  mEGLImage =
      egl->fCreateImage(LOCAL_EGL_NO_CONTEXT, LOCAL_EGL_LINUX_DMA_BUF_EXT,
                        nullptr, attribs.Elements());
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

void DMABufSurfaceRGBA::ReleaseTextures() {
  FenceDelete();

  if (!mGL) return;
  const auto& gle = gl::GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;

  if (mTexture && mGL->MakeCurrent()) {
    mGL->fDeleteTextures(1, &mTexture);
    mTexture = 0;
    mGL = nullptr;
  }

  if (mEGLImage) {
    egl->fDestroyImage(mEGLImage);
    mEGLImage = nullptr;
  }
}

void DMABufSurfaceRGBA::ReleaseSurface() {
  MOZ_ASSERT(!IsMapped(), "We can't release mapped buffer!");

  ReleaseTextures();

  if (mWLBuffer) {
    wl_buffer_destroy(mWLBuffer);
    mWLBuffer = nullptr;
  }

  ReleaseDMABuf();
}

void* DMABufSurface::MapInternal(uint32_t aX, uint32_t aY, uint32_t aWidth,
                                 uint32_t aHeight, uint32_t* aStride,
                                 int aGbmFlags, int aPlane) {
  NS_ASSERTION(!IsMapped(aPlane), "Already mapped!");
  if (!mGbmBufferObject[aPlane]) {
    NS_WARNING("We can't map DMABufSurfaceRGBA without mGbmBufferObject");
    return nullptr;
  }

  mMappedRegionStride[aPlane] = 0;
  mMappedRegionData[aPlane] = nullptr;
  mMappedRegion[aPlane] = nsGbmLib::Map(
      mGbmBufferObject[aPlane], aX, aY, aWidth, aHeight, aGbmFlags,
      &mMappedRegionStride[aPlane], &mMappedRegionData[aPlane]);
  if (aStride) {
    *aStride = mMappedRegionStride[aPlane];
  }
  return mMappedRegion[aPlane];
}

void* DMABufSurfaceRGBA::MapReadOnly(uint32_t aX, uint32_t aY, uint32_t aWidth,
                                     uint32_t aHeight, uint32_t* aStride) {
  return MapInternal(aX, aY, aWidth, aHeight, aStride, GBM_BO_TRANSFER_READ);
}

void* DMABufSurfaceRGBA::MapReadOnly(uint32_t* aStride) {
  return MapInternal(0, 0, mWidth, mHeight, aStride, GBM_BO_TRANSFER_READ);
}

void* DMABufSurfaceRGBA::Map(uint32_t aX, uint32_t aY, uint32_t aWidth,
                             uint32_t aHeight, uint32_t* aStride) {
  return MapInternal(aX, aY, aWidth, aHeight, aStride,
                     GBM_BO_TRANSFER_READ_WRITE);
}

void* DMABufSurfaceRGBA::Map(uint32_t* aStride) {
  return MapInternal(0, 0, mWidth, mHeight, aStride,
                     GBM_BO_TRANSFER_READ_WRITE);
}

void DMABufSurface::Unmap(int aPlane) {
  if (mMappedRegion[aPlane]) {
    nsGbmLib::Unmap(mGbmBufferObject[aPlane], mMappedRegionData[aPlane]);
    mMappedRegion[aPlane] = nullptr;
    mMappedRegionData[aPlane] = nullptr;
    mMappedRegionStride[aPlane] = 0;
  }
}

bool DMABufSurfaceRGBA::Resize(int aWidth, int aHeight) {
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

bool DMABufSurfaceRGBA::CopyFrom(class DMABufSurface* aSourceSurface,
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
void DMABufSurfaceRGBA::Clear() {
  uint32_t destStride;
  void* destData = Map(&destStride);
  memset(destData, 0, GetHeight() * destStride);
  Unmap();
}

bool DMABufSurfaceRGBA::HasAlpha() {
  return mGmbFormat ? mGmbFormat->mHasAlpha : true;
}

gfx::SurfaceFormat DMABufSurfaceRGBA::GetFormat() {
  return HasAlpha() ? gfx::SurfaceFormat::B8G8R8A8
                    : gfx::SurfaceFormat::B8G8R8X8;
}

// GL uses swapped R and B components so report accordingly.
gfx::SurfaceFormat DMABufSurfaceRGBA::GetFormatGL() {
  return HasAlpha() ? gfx::SurfaceFormat::R8G8B8A8
                    : gfx::SurfaceFormat::R8G8B8X8;
}

already_AddRefed<DMABufSurfaceRGBA> DMABufSurfaceRGBA::CreateDMABufSurface(
    int aWidth, int aHeight, int aDMABufSurfaceFlags) {
  RefPtr<DMABufSurfaceRGBA> surf = new DMABufSurfaceRGBA();
  if (!surf->Create(aWidth, aHeight, aDMABufSurfaceFlags)) {
    return nullptr;
  }
  return surf.forget();
}

already_AddRefed<DMABufSurfaceYUV> DMABufSurfaceYUV::CreateYUVSurface(
    const VADRMPRIMESurfaceDescriptor& aDesc) {
  RefPtr<DMABufSurfaceYUV> surf = new DMABufSurfaceYUV();
  if (!surf->UpdateYUVData(aDesc)) {
    return nullptr;
  }
  return surf.forget();
}

already_AddRefed<DMABufSurfaceYUV> DMABufSurfaceYUV::CreateYUVSurface(
    int aWidth, int aHeight, void** aPixelData, int* aLineSizes) {
  RefPtr<DMABufSurfaceYUV> surf = new DMABufSurfaceYUV();
  if (!surf->Create(aWidth, aHeight, aPixelData, aLineSizes)) {
    return nullptr;
  }
  return surf.forget();
}

DMABufSurfaceYUV::DMABufSurfaceYUV()
    : DMABufSurface(SURFACE_NV12),
      mWidth(),
      mHeight(),
      mTexture(),
      mColorSpace(mozilla::gfx::YUVColorSpace::UNKNOWN) {
  for (int i = 0; i < DMABUF_BUFFER_PLANES; i++) {
    mEGLImage[i] = LOCAL_EGL_NO_IMAGE;
  }
}

DMABufSurfaceYUV::~DMABufSurfaceYUV() { ReleaseSurface(); }

bool DMABufSurfaceYUV::UpdateYUVData(const VADRMPRIMESurfaceDescriptor& aDesc) {
  if (aDesc.num_layers > DMABUF_BUFFER_PLANES ||
      aDesc.num_objects > DMABUF_BUFFER_PLANES) {
    return false;
  }
  if (mDmabufFds[0] >= 0) {
    NS_WARNING("DMABufSurfaceYUV is already created!");
    return false;
  }
  if (aDesc.fourcc == VA_FOURCC_NV12) {
    mSurfaceType = SURFACE_NV12;
  } else if (aDesc.fourcc == VA_FOURCC_YV12) {
    mSurfaceType = SURFACE_YUV420;
  } else {
    NS_WARNING(
        nsPrintfCString(
            "UpdateYUVData(): Can't import surface data of 0x%x format\n",
            aDesc.fourcc)
            .get());
    return false;
  }

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

bool DMABufSurfaceYUV::CreateYUVPlane(int aPlane, int aWidth, int aHeight,
                                      int aDrmFormat) {
  mWidth[aPlane] = aWidth;
  mHeight[aPlane] = aHeight;
  mDrmFormats[aPlane] = aDrmFormat;

  mGbmBufferObject[aPlane] =
      nsGbmLib::Create(GetDMABufDevice()->GetGbmDevice(), aWidth, aHeight,
                       aDrmFormat, GBM_BO_USE_LINEAR | GBM_BO_USE_TEXTURING);
  if (!mGbmBufferObject[aPlane]) {
    NS_WARNING("Failed to create GbmBufferObject!");
    return false;
  }

  mStrides[aPlane] = nsGbmLib::GetStride(mGbmBufferObject[aPlane]);
  mDmabufFds[aPlane] = nsGbmLib::GetFd(mGbmBufferObject[aPlane]);

  return true;
}

void DMABufSurfaceYUV::UpdateYUVPlane(int aPlane, void* aPixelData,
                                      int aLineSize) {
  if (aLineSize == mWidth[aPlane] &&
      (int)mMappedRegionStride[aPlane] == mWidth[aPlane]) {
    memcpy(mMappedRegion[aPlane], aPixelData, aLineSize * mHeight[aPlane]);
  } else {
    char* src = (char*)aPixelData;
    char* dest = (char*)mMappedRegion[aPlane];
    for (int i = 0; i < mHeight[aPlane]; i++) {
      memcpy(dest, src, mWidth[aPlane]);
      src += aLineSize;
      dest += mMappedRegionStride[aPlane];
    }
  }
}

bool DMABufSurfaceYUV::UpdateYUVData(void** aPixelData, int* aLineSizes) {
  if (mSurfaceType != SURFACE_YUV420) {
    NS_WARNING("UpdateYUVData can upload YUV420 surface type only!");
    return false;
  }

  if (mBufferPlaneCount != 3) {
    NS_WARNING("DMABufSurfaceYUV planes does not match!");
    return false;
  }

  auto unmapBuffers = MakeScopeExit([&] {
    Unmap(0);
    Unmap(1);
    Unmap(2);
  });

  // Map planes
  for (int i = 0; i < mBufferPlaneCount; i++) {
    MapInternal(0, 0, mWidth[i], mHeight[i], nullptr, GBM_BO_TRANSFER_WRITE, i);
    if (!mMappedRegion[i]) {
      NS_WARNING("DMABufSurfaceYUV plane can't be mapped!");
      return false;
    }
    if ((int)mMappedRegionStride[i] < mWidth[i]) {
      NS_WARNING("DMABufSurfaceYUV plane size stride does not match!");
      return false;
    }
  }

  // Copy planes
  for (int i = 0; i < mBufferPlaneCount; i++) {
    UpdateYUVPlane(i, aPixelData[i], aLineSizes[i]);
  }

  return true;
}

bool DMABufSurfaceYUV::Create(int aWidth, int aHeight, void** aPixelData,
                              int* aLineSizes) {
  mSurfaceType = SURFACE_YUV420;
  mBufferPlaneCount = 3;

  if (!CreateYUVPlane(0, aWidth, aHeight, GBM_FORMAT_R8)) {
    return false;
  }
  if (!CreateYUVPlane(1, aWidth >> 1, aHeight >> 1, GBM_FORMAT_R8)) {
    return false;
  }
  if (!CreateYUVPlane(2, aWidth >> 1, aHeight >> 1, GBM_FORMAT_R8)) {
    return false;
  }

  return aPixelData != nullptr && aLineSizes != nullptr
             ? UpdateYUVData(aPixelData, aLineSizes)
             : true;
}

bool DMABufSurfaceYUV::Create(const SurfaceDescriptor& aDesc) {
  ImportSurfaceDescriptor(aDesc);
  return true;
}

void DMABufSurfaceYUV::ImportSurfaceDescriptor(
    const SurfaceDescriptorDMABuf& aDesc) {
  mBufferPlaneCount = aDesc.fds().Length();
  mSurfaceType = (mBufferPlaneCount == 2) ? SURFACE_NV12 : SURFACE_YUV420;
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
    mSyncFd = aDesc.fence()[0].ClonePlatformHandle().release();
  }

  if (aDesc.refCount().Length() > 0) {
    GlobalRefCountImport(aDesc.refCount()[0].ClonePlatformHandle().release());
  }
}

bool DMABufSurfaceYUV::Serialize(
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
    fenceFDs.AppendElement(ipc::FileDescriptor(mSyncFd));
  }

  if (mGlobalRefCountFd) {
    refCountFDs.AppendElement(ipc::FileDescriptor(mGlobalRefCountFd));
  }

  aOutDescriptor = SurfaceDescriptorDMABuf(
      mSurfaceType, mBufferModifier, 0, fds, width, height, format, strides,
      offsets, GetYUVColorSpace(), fenceFDs, mUID, refCountFDs);
  return true;
}

bool DMABufSurfaceYUV::CreateTexture(GLContext* aGLContext, int aPlane) {
  MOZ_ASSERT(!mEGLImage[aPlane] && !mTexture[aPlane],
             "EGLImage/Texture is already created!");

  if (!aGLContext) return false;
  const auto& gle = gl::GLContextEGL::Cast(aGLContext);
  const auto& egl = gle->mEgl;

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

  mEGLImage[aPlane] =
      egl->fCreateImage(LOCAL_EGL_NO_CONTEXT, LOCAL_EGL_LINUX_DMA_BUF_EXT,
                        nullptr, attribs.Elements());
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

void DMABufSurfaceYUV::ReleaseTextures() {
  FenceDelete();

  bool textureActive = false;
  for (int i = 0; i < mBufferPlaneCount; i++) {
    if (mTexture[i]) {
      textureActive = true;
      break;
    }
  }

  if (!mGL) return;
  const auto& gle = gl::GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;

  if (textureActive && mGL->MakeCurrent()) {
    mGL->fDeleteTextures(DMABUF_BUFFER_PLANES, mTexture);
    for (int i = 0; i < DMABUF_BUFFER_PLANES; i++) {
      mTexture[i] = 0;
    }
    mGL = nullptr;
  }

  for (int i = 0; i < mBufferPlaneCount; i++) {
    if (mEGLImage[i]) {
      egl->fDestroyImage(mEGLImage[i]);
      mEGLImage[i] = nullptr;
    }
  }
}

gfx::SurfaceFormat DMABufSurfaceYUV::GetFormat() {
  switch (mSurfaceType) {
    case SURFACE_NV12:
      return gfx::SurfaceFormat::NV12;
    case SURFACE_YUV420:
      return gfx::SurfaceFormat::YUV;
    default:
      NS_WARNING("DMABufSurfaceYUV::GetFormat(): Wrong surface format!");
      return gfx::SurfaceFormat::UNKNOWN;
  }
}

// GL uses swapped R and B components so report accordingly.
gfx::SurfaceFormat DMABufSurfaceYUV::GetFormatGL() { return GetFormat(); }

uint32_t DMABufSurfaceYUV::GetTextureCount() {
  switch (mSurfaceType) {
    case SURFACE_NV12:
      return 2;
    case SURFACE_YUV420:
      return 3;
    default:
      NS_WARNING("DMABufSurfaceYUV::GetTextureCount(): Wrong surface format!");
      return 1;
  }
}

void DMABufSurfaceYUV::ReleaseSurface() {
  ReleaseTextures();
  ReleaseDMABuf();
}
