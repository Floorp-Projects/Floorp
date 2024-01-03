/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DMABufSurface.h"
#include "DMABufLibWrapper.h"

#ifdef MOZ_WAYLAND
#  include "nsWaylandDisplay.h"
#endif

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
#ifdef HAVE_EVENTFD
#  include <sys/eventfd.h>
#endif
#include <poll.h>
#ifdef HAVE_SYSIOCCOM_H
#  include <sys/ioccom.h>
#endif
#include <sys/ioctl.h>

#include "mozilla/widget/gbm.h"
#include "mozilla/widget/va_drmcommon.h"
#include "YCbCrUtils.h"
#include "mozilla/gfx/2D.h"
#include "GLContextTypes.h"  // for GLContext, etc
#include "GLContextEGL.h"
#include "GLContextProvider.h"
#include "ScopedGLHelpers.h"
#include "GLBlitHelper.h"
#include "GLReadTexImageHelper.h"
#include "nsGtkUtils.h"

#include "mozilla/layers/LayersSurfaces.h"
#include "mozilla/ScopeExit.h"

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
using namespace mozilla::gfx;

#ifdef MOZ_LOGGING
#  include "mozilla/Logging.h"
#  include "nsTArray.h"
#  include "Units.h"
static LazyLogModule gDmabufRefLog("DmabufRef");
#  define LOGDMABUFREF(args) \
    MOZ_LOG(gDmabufRefLog, mozilla::LogLevel::Debug, args)
#else
#  define LOGDMABUFREF(args)
#endif /* MOZ_LOGGING */

#define BUFFER_FLAGS 0

static RefPtr<GLContext> sSnapshotContext;
static StaticMutex sSnapshotContextMutex MOZ_UNANNOTATED;
static Atomic<int> gNewSurfaceUID(1);

RefPtr<GLContext> ClaimSnapshotGLContext() {
  if (!sSnapshotContext) {
    nsCString discardFailureId;
    sSnapshotContext = GLContextProvider::CreateHeadless({}, &discardFailureId);
    if (!sSnapshotContext) {
      LOGDMABUF(
          ("ClaimSnapshotGLContext: Failed to create snapshot GLContext."));
      return nullptr;
    }
    sSnapshotContext->mOwningThreadId = Nothing();  // No singular owner.
  }
  if (!sSnapshotContext->MakeCurrent()) {
    LOGDMABUF(("ClaimSnapshotGLContext: Failed to make GLContext current."));
    return nullptr;
  }
  return sSnapshotContext;
}

void ReturnSnapshotGLContext(RefPtr<GLContext> aGLContext) {
  // direct eglMakeCurrent() call breaks current context caching so make sure
  // it's not used.
  MOZ_ASSERT(!aGLContext->mUseTLSIsCurrent);
  if (!aGLContext->IsCurrent()) {
    LOGDMABUF(("ReturnSnapshotGLContext() failed, is not current!"));
    return;
  }
  const auto& gle = gl::GLContextEGL::Cast(aGLContext);
  const auto& egl = gle->mEgl;
  egl->fMakeCurrent(EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

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
#ifdef HAVE_EVENTFD
  if (!mGlobalRefCountFd) {
    return;
  }
  LOGDMABUFREF(("DMABufSurface::GlobalRefRelease UID %d", mUID));
  uint64_t counter;
  if (read(mGlobalRefCountFd, &counter, sizeof(counter)) != sizeof(counter)) {
    if (errno == EAGAIN) {
      LOGDMABUFREF(
          ("  GlobalRefRelease failed: already zero reference! UID %d", mUID));
    }
    // EAGAIN means the refcount is already zero. It happens when we release
    // last reference to the surface.
    if (errno != EAGAIN) {
      NS_WARNING(nsPrintfCString("Failed to unref dmabuf global ref count: %s",
                                 strerror(errno))
                     .get());
    }
  }
#endif
}

void DMABufSurface::GlobalRefAdd() {
#ifdef HAVE_EVENTFD
  LOGDMABUFREF(("DMABufSurface::GlobalRefAdd UID %d", mUID));
  MOZ_DIAGNOSTIC_ASSERT(mGlobalRefCountFd);
  uint64_t counter = 1;
  if (write(mGlobalRefCountFd, &counter, sizeof(counter)) != sizeof(counter)) {
    NS_WARNING(nsPrintfCString("Failed to ref dmabuf global ref count: %s",
                               strerror(errno))
                   .get());
  }
#endif
}

void DMABufSurface::GlobalRefCountCreate() {
#ifdef HAVE_EVENTFD
  LOGDMABUFREF(("DMABufSurface::GlobalRefCountCreate UID %d", mUID));
  MOZ_DIAGNOSTIC_ASSERT(!mGlobalRefCountFd);
  // Create global ref count initialized to 0,
  // i.e. is not referenced after create.
  mGlobalRefCountFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE);
  if (mGlobalRefCountFd < 0) {
    NS_WARNING(nsPrintfCString("Failed to create dmabuf global ref count: %s",
                               strerror(errno))
                   .get());
    mGlobalRefCountFd = 0;
    return;
  }
#endif
}

void DMABufSurface::GlobalRefCountImport(int aFd) {
#ifdef HAVE_EVENTFD
  mGlobalRefCountFd = aFd;
  if (mGlobalRefCountFd) {
    LOGDMABUFREF(("DMABufSurface::GlobalRefCountImport UID %d", mUID));
    GlobalRefAdd();
  }
#endif
}

int DMABufSurface::GlobalRefCountExport() {
#ifdef MOZ_LOGGING
  if (mGlobalRefCountFd) {
    LOGDMABUFREF(("DMABufSurface::GlobalRefCountExport UID %d", mUID));
  }
#endif
  return mGlobalRefCountFd;
}

void DMABufSurface::GlobalRefCountDelete() {
  if (mGlobalRefCountFd) {
    LOGDMABUFREF(("DMABufSurface::GlobalRefCountDelete UID %d", mUID));
    close(mGlobalRefCountFd);
    mGlobalRefCountFd = 0;
  }
}

void DMABufSurface::ReleaseDMABuf() {
  LOGDMABUF(("DMABufSurface::ReleaseDMABuf() UID %d", mUID));
  for (int i = 0; i < mBufferPlaneCount; i++) {
    Unmap(i);
  }

  MutexAutoLock lockFD(mSurfaceLock);
  CloseFileDescriptors(lockFD, /* aForceClose */ true);

  for (int i = 0; i < mBufferPlaneCount; i++) {
    if (mGbmBufferObject[i]) {
      GbmLib::Destroy(mGbmBufferObject[i]);
      mGbmBufferObject[i] = nullptr;
    }
  }
  mBufferPlaneCount = 0;
}

DMABufSurface::DMABufSurface(SurfaceType aSurfaceType)
    : mSurfaceType(aSurfaceType),
      mBufferPlaneCount(0),
      mDrmFormats(),
      mStrides(),
      mOffsets(),
      mGbmBufferObject(),
      mMappedRegion(),
      mMappedRegionStride(),
      mSyncFd(-1),
      mSync(nullptr),
      mGlobalRefCountFd(0),
      mUID(gNewSurfaceUID++),
      mSurfaceLock("DMABufSurface") {
  for (auto& slot : mDmabufFds) {
    slot = -1;
  }
  for (auto& modifier : mBufferModifiers) {
    modifier = DRM_FORMAT_MOD_INVALID;
  }
}

DMABufSurface::~DMABufSurface() {
  FenceDelete();
  GlobalRefRelease();
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
  if (mSyncFd > 0) {
    close(mSyncFd);
    mSyncFd = -1;
  }

  if (!mGL) {
    return;
  }
  const auto& gle = gl::GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;

  if (mSync) {
    egl->fDestroySync(mSync);
    mSync = nullptr;
  }
}

void DMABufSurface::FenceSet() {
  if (!mGL || !mGL->MakeCurrent()) {
    MOZ_DIAGNOSTIC_ASSERT(mGL,
                          "DMABufSurface::FenceSet(): missing GL context!");
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
  if (!mGL || mSyncFd < 0) {
    MOZ_DIAGNOSTIC_ASSERT(mGL,
                          "DMABufSurface::FenceWait() missing GL context!");
    return;
  }

  const auto& gle = gl::GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;

  const EGLint attribs[] = {LOCAL_EGL_SYNC_NATIVE_FENCE_FD_ANDROID, mSyncFd,
                            LOCAL_EGL_NONE};
  EGLSync sync = egl->fCreateSync(LOCAL_EGL_SYNC_NATIVE_FENCE_ANDROID, attribs);
  if (!sync) {
    MOZ_ASSERT(false, "DMABufSurface::FenceWait(): Failed to create GLFence!");
    // We failed to create GLFence so clear mSyncFd to avoid another try.
    close(mSyncFd);
    mSyncFd = -1;
    return;
  }

  // mSyncFd is owned by GLFence so clear local reference to avoid double close
  // at DMABufSurface::FenceDelete().
  mSyncFd = -1;

  egl->fClientWaitSync(sync, 0, LOCAL_EGL_FOREVER);
  egl->fDestroySync(sync);
}

bool DMABufSurface::OpenFileDescriptors(const MutexAutoLock& aProofOfLock) {
  for (int i = 0; i < mBufferPlaneCount; i++) {
    if (!OpenFileDescriptorForPlane(aProofOfLock, i)) {
      return false;
    }
  }
  return true;
}

// We can safely close DMABuf file descriptors only when we have a valid
// GbmBufferObject. When we don't have a valid GbmBufferObject and a DMABuf
// file descriptor is closed, whole surface is released.
void DMABufSurface::CloseFileDescriptors(const MutexAutoLock& aProofOfLock,
                                         bool aForceClose) {
  for (int i = 0; i < DMABUF_BUFFER_PLANES; i++) {
    CloseFileDescriptorForPlane(aProofOfLock, i, aForceClose);
  }
}

DMABufSurfaceRGBA::DMABufSurfaceRGBA()
    : DMABufSurface(SURFACE_RGBA),
      mSurfaceFlags(0),
      mWidth(0),
      mHeight(0),
      mGmbFormat(nullptr),
      mEGLImage(LOCAL_EGL_NO_IMAGE),
      mTexture(0),
      mGbmBufferFlags(0) {}

DMABufSurfaceRGBA::~DMABufSurfaceRGBA() {
#ifdef MOZ_WAYLAND
  ReleaseWlBuffer();
#endif
  ReleaseSurface();
}

bool DMABufSurfaceRGBA::OpenFileDescriptorForPlane(
    const MutexAutoLock& aProofOfLock, int aPlane) {
  if (mDmabufFds[aPlane] >= 0) {
    return true;
  }
  gbm_bo* bo = mGbmBufferObject[0];
  if (NS_WARN_IF(!bo)) {
    LOGDMABUF(
        ("DMABufSurfaceRGBA::OpenFileDescriptorForPlane: Missing "
         "mGbmBufferObject object!"));
    return false;
  }

  if (mBufferPlaneCount == 1) {
    MOZ_ASSERT(aPlane == 0, "DMABuf: wrong surface plane!");
    mDmabufFds[0] = GbmLib::GetFd(bo);
  } else {
    mDmabufFds[aPlane] = GetDMABufDevice()->GetDmabufFD(
        GbmLib::GetHandleForPlane(bo, aPlane).u32);
  }

  if (mDmabufFds[aPlane] < 0) {
    CloseFileDescriptors(aProofOfLock);
    return false;
  }

  return true;
}

void DMABufSurfaceRGBA::CloseFileDescriptorForPlane(
    const MutexAutoLock& aProofOfLock, int aPlane, bool aForceClose = false) {
  if ((aForceClose || mGbmBufferObject[0]) && mDmabufFds[aPlane] >= 0) {
    close(mDmabufFds[aPlane]);
    mDmabufFds[aPlane] = -1;
  }
}

bool DMABufSurfaceRGBA::Create(int aWidth, int aHeight,
                               int aDMABufSurfaceFlags) {
  MOZ_ASSERT(mGbmBufferObject[0] == nullptr, "Already created?");

  mSurfaceFlags = aDMABufSurfaceFlags;
  mWidth = aWidth;
  mHeight = aHeight;

  LOGDMABUF(("DMABufSurfaceRGBA::Create() UID %d size %d x %d\n", mUID, mWidth,
             mHeight));

  if (!GetDMABufDevice()->GetGbmDevice()) {
    LOGDMABUF(("    Missing GbmDevice!"));
    return false;
  }

  mGmbFormat = GetDMABufDevice()->GetGbmFormat(mSurfaceFlags & DMABUF_ALPHA);
  if (!mGmbFormat) {
    // Requested DRM format is not supported.
    return false;
  }
  mDrmFormats[0] = mGmbFormat->mFormat;

  bool useModifiers = (aDMABufSurfaceFlags & DMABUF_USE_MODIFIERS) &&
                      !mGmbFormat->mModifiers.IsEmpty();
  if (useModifiers) {
    LOGDMABUF(("    Creating with modifiers\n"));
    mGbmBufferObject[0] = GbmLib::CreateWithModifiers(
        GetDMABufDevice()->GetGbmDevice(), mWidth, mHeight, mDrmFormats[0],
        mGmbFormat->mModifiers.Elements(), mGmbFormat->mModifiers.Length());
    if (mGbmBufferObject[0]) {
      mBufferModifiers[0] = GbmLib::GetModifier(mGbmBufferObject[0]);
    }
  }

  if (!mGbmBufferObject[0]) {
    LOGDMABUF(("    Creating without modifiers\n"));
    mGbmBufferFlags = GBM_BO_USE_LINEAR;
    mGbmBufferObject[0] =
        GbmLib::Create(GetDMABufDevice()->GetGbmDevice(), mWidth, mHeight,
                       mDrmFormats[0], mGbmBufferFlags);
    mBufferModifiers[0] = DRM_FORMAT_MOD_INVALID;
  }

  if (!mGbmBufferObject[0]) {
    LOGDMABUF(("    Failed to create GbmBufferObject\n"));
    return false;
  }

  if (mBufferModifiers[0] != DRM_FORMAT_MOD_INVALID) {
    mBufferPlaneCount = GbmLib::GetPlaneCount(mGbmBufferObject[0]);
    if (mBufferPlaneCount > DMABUF_BUFFER_PLANES) {
      LOGDMABUF(
          ("    There's too many dmabuf planes! (%d)", mBufferPlaneCount));
      mBufferPlaneCount = DMABUF_BUFFER_PLANES;
      return false;
    }

    for (int i = 0; i < mBufferPlaneCount; i++) {
      mStrides[i] = GbmLib::GetStrideForPlane(mGbmBufferObject[0], i);
      mOffsets[i] = GbmLib::GetOffset(mGbmBufferObject[0], i);
    }
  } else {
    mBufferPlaneCount = 1;
    mStrides[0] = GbmLib::GetStride(mGbmBufferObject[0]);
  }

  LOGDMABUF(("    Success\n"));
  return true;
}

bool DMABufSurfaceRGBA::Create(mozilla::gl::GLContext* aGLContext,
                               const EGLImageKHR aEGLImage, int aWidth,
                               int aHeight) {
  LOGDMABUF(("DMABufSurfaceRGBA::Create() from EGLImage UID = %d\n", mUID));
  if (!aGLContext) {
    return false;
  }
  const auto& gle = gl::GLContextEGL::Cast(aGLContext);
  const auto& egl = gle->mEgl;

  mGL = aGLContext;
  mWidth = aWidth;
  mHeight = aHeight;
  mEGLImage = aEGLImage;
  if (!egl->fExportDMABUFImageQuery(mEGLImage, mDrmFormats, &mBufferPlaneCount,
                                    mBufferModifiers)) {
    LOGDMABUF(("  ExportDMABUFImageQueryMESA failed, quit\n"));
    return false;
  }
  if (mBufferPlaneCount > DMABUF_BUFFER_PLANES) {
    LOGDMABUF(("  wrong plane count %d, quit\n", mBufferPlaneCount));
    mBufferPlaneCount = DMABUF_BUFFER_PLANES;
    return false;
  }
  if (!egl->fExportDMABUFImage(mEGLImage, mDmabufFds, mStrides, mOffsets)) {
    LOGDMABUF(("  ExportDMABUFImageMESA failed, quit\n"));
    return false;
  }

  // A broken driver can return dmabuf without valid file descriptors
  // which leads to fails later so quit now.
  for (int i = 0; i < mBufferPlaneCount; i++) {
    if (mDmabufFds[i] < 0) {
      LOGDMABUF(
          ("  ExportDMABUFImageMESA failed, mDmabufFds[%d] is invalid, quit",
           i));
      return false;
    }
  }

  LOGDMABUF(("  imported size %d x %d format %x planes %d modifiers %" PRIx64,
             mWidth, mHeight, mDrmFormats[0], mBufferPlaneCount,
             mBufferModifiers[0]));
  return true;
}

bool DMABufSurfaceRGBA::ImportSurfaceDescriptor(
    const SurfaceDescriptor& aDesc) {
  const SurfaceDescriptorDMABuf& desc = aDesc.get_SurfaceDescriptorDMABuf();

  mWidth = desc.width()[0];
  mHeight = desc.height()[0];
  mBufferModifiers[0] = desc.modifier()[0];
  mDrmFormats[0] = desc.format()[0];
  mBufferPlaneCount = desc.fds().Length();
  mGbmBufferFlags = desc.flags();
  MOZ_RELEASE_ASSERT(mBufferPlaneCount <= DMABUF_BUFFER_PLANES);
  mUID = desc.uid();

  LOGDMABUF(
      ("DMABufSurfaceRGBA::ImportSurfaceDescriptor() UID %d size %d x %d\n",
       mUID, mWidth, mHeight));

  for (int i = 0; i < mBufferPlaneCount; i++) {
    mDmabufFds[i] = desc.fds()[i].ClonePlatformHandle().release();
    if (mDmabufFds[i] < 0) {
      LOGDMABUF(
          ("    failed to get DMABuf file descriptor: %s", strerror(errno)));
      return false;
    }
    mStrides[i] = desc.strides()[i];
    mOffsets[i] = desc.offsets()[i];
  }

  if (desc.fence().Length() > 0) {
    mSyncFd = desc.fence()[0].ClonePlatformHandle().release();
    if (mSyncFd < 0) {
      LOGDMABUF(
          ("    failed to get GL fence file descriptor: %s", strerror(errno)));
      return false;
    }
  }

  if (desc.refCount().Length() > 0) {
    GlobalRefCountImport(desc.refCount()[0].ClonePlatformHandle().release());
  }

  LOGDMABUF(("  imported size %d x %d format %x planes %d", mWidth, mHeight,
             mDrmFormats[0], mBufferPlaneCount));
  return true;
}

bool DMABufSurfaceRGBA::Create(const SurfaceDescriptor& aDesc) {
  return ImportSurfaceDescriptor(aDesc);
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
  AutoTArray<uint64_t, DMABUF_BUFFER_PLANES> modifiers;
  AutoTArray<ipc::FileDescriptor, 1> fenceFDs;
  AutoTArray<ipc::FileDescriptor, 1> refCountFDs;

  LOGDMABUF(("DMABufSurfaceRGBA::Serialize() UID %d\n", mUID));

  MutexAutoLock lockFD(mSurfaceLock);
  if (!OpenFileDescriptors(lockFD)) {
    return false;
  }

  width.AppendElement(mWidth);
  height.AppendElement(mHeight);
  format.AppendElement(mDrmFormats[0]);
  modifiers.AppendElement(mBufferModifiers[0]);
  for (int i = 0; i < mBufferPlaneCount; i++) {
    fds.AppendElement(ipc::FileDescriptor(mDmabufFds[i]));
    strides.AppendElement(mStrides[i]);
    offsets.AppendElement(mOffsets[i]);
  }

  CloseFileDescriptors(lockFD);

  if (mSync) {
    fenceFDs.AppendElement(ipc::FileDescriptor(mSyncFd));
  }

  if (mGlobalRefCountFd) {
    refCountFDs.AppendElement(ipc::FileDescriptor(GlobalRefCountExport()));
  }

  aOutDescriptor = SurfaceDescriptorDMABuf(
      mSurfaceType, modifiers, mGbmBufferFlags, fds, width, height, width,
      height, format, strides, offsets, GetYUVColorSpace(), mColorRange,
      fenceFDs, mUID, refCountFDs);
  return true;
}

bool DMABufSurfaceRGBA::CreateTexture(GLContext* aGLContext, int aPlane) {
  LOGDMABUF(("DMABufSurfaceRGBA::CreateTexture() UID %d\n", mUID));
  MOZ_ASSERT(!mEGLImage && !mTexture, "EGLImage is already created!");

  nsTArray<EGLint> attribs;
  attribs.AppendElement(LOCAL_EGL_WIDTH);
  attribs.AppendElement(mWidth);
  attribs.AppendElement(LOCAL_EGL_HEIGHT);
  attribs.AppendElement(mHeight);
  attribs.AppendElement(LOCAL_EGL_LINUX_DRM_FOURCC_EXT);
  attribs.AppendElement(mDrmFormats[0]);
#define ADD_PLANE_ATTRIBS(plane_idx)                                        \
  {                                                                         \
    attribs.AppendElement(LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_FD_EXT);     \
    attribs.AppendElement(mDmabufFds[plane_idx]);                           \
    attribs.AppendElement(LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_OFFSET_EXT); \
    attribs.AppendElement((int)mOffsets[plane_idx]);                        \
    attribs.AppendElement(LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_PITCH_EXT);  \
    attribs.AppendElement((int)mStrides[plane_idx]);                        \
    if (mBufferModifiers[0] != DRM_FORMAT_MOD_INVALID) {                    \
      attribs.AppendElement(                                                \
          LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_MODIFIER_LO_EXT);            \
      attribs.AppendElement(mBufferModifiers[0] & 0xFFFFFFFF);              \
      attribs.AppendElement(                                                \
          LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_MODIFIER_HI_EXT);            \
      attribs.AppendElement(mBufferModifiers[0] >> 32);                     \
    }                                                                       \
  }

  MutexAutoLock lockFD(mSurfaceLock);
  if (!OpenFileDescriptors(lockFD)) {
    return false;
  }
  ADD_PLANE_ATTRIBS(0);
  if (mBufferPlaneCount > 1) ADD_PLANE_ATTRIBS(1);
  if (mBufferPlaneCount > 2) ADD_PLANE_ATTRIBS(2);
  if (mBufferPlaneCount > 3) ADD_PLANE_ATTRIBS(3);
#undef ADD_PLANE_ATTRIBS
  attribs.AppendElement(LOCAL_EGL_NONE);

  if (!aGLContext) return false;

  if (!aGLContext->MakeCurrent()) {
    LOGDMABUF(
        ("DMABufSurfaceRGBA::CreateTexture(): failed to make GL context "
         "current"));
    return false;
  }

  const auto& gle = gl::GLContextEGL::Cast(aGLContext);
  const auto& egl = gle->mEgl;
  mEGLImage =
      egl->fCreateImage(LOCAL_EGL_NO_CONTEXT, LOCAL_EGL_LINUX_DMA_BUF_EXT,
                        nullptr, attribs.Elements());

  CloseFileDescriptors(lockFD);

  if (mEGLImage == LOCAL_EGL_NO_IMAGE) {
    LOGDMABUF(("EGLImageKHR creation failed"));
    return false;
  }

  aGLContext->fGenTextures(1, &mTexture);
  const ScopedBindTexture savedTex(aGLContext, mTexture);
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
  LOGDMABUF(("DMABufSurfaceRGBA::ReleaseTextures() UID %d\n", mUID));
  FenceDelete();

  if (!mTexture && !mEGLImage) {
    return;
  }

  if (!mGL) {
#ifdef NIGHTLY_BUILD
    MOZ_DIAGNOSTIC_ASSERT(mGL, "Missing GL context!");
#else
    NS_WARNING(
        "DMABufSurfaceRGBA::ReleaseTextures(): Missing GL context! We're "
        "leaking textures!");
    return;
#endif
  }

  const auto& gle = gl::GLContextEGL::Cast(mGL);
  const auto& egl = gle->mEgl;

  if (mTexture && mGL->MakeCurrent()) {
    mGL->fDeleteTextures(1, &mTexture);
    mTexture = 0;
  }

  if (mEGLImage != LOCAL_EGL_NO_IMAGE) {
    egl->fDestroyImage(mEGLImage);
    mEGLImage = LOCAL_EGL_NO_IMAGE;
  }
  mGL = nullptr;
}

void DMABufSurfaceRGBA::ReleaseSurface() {
  MOZ_ASSERT(!IsMapped(), "We can't release mapped buffer!");

  ReleaseTextures();
  ReleaseDMABuf();
}

#ifdef MOZ_WAYLAND
bool DMABufSurfaceRGBA::CreateWlBuffer() {
  MutexAutoLock lockFD(mSurfaceLock);
  if (!OpenFileDescriptors(lockFD)) {
    return false;
  }

  nsWaylandDisplay* waylandDisplay = widget::WaylandDisplayGet();
  if (!waylandDisplay->GetDmabuf()) {
    CloseFileDescriptors(lockFD);
    return false;
  }

  struct zwp_linux_buffer_params_v1* params =
      zwp_linux_dmabuf_v1_create_params(waylandDisplay->GetDmabuf());
  zwp_linux_buffer_params_v1_add(params, mDmabufFds[0], 0, mOffsets[0],
                                 mStrides[0], mBufferModifiers[0] >> 32,
                                 mBufferModifiers[0] & 0xffffffff);

  mWlBuffer = zwp_linux_buffer_params_v1_create_immed(
      params, GetWidth(), GetHeight(), mDrmFormats[0], 0);

  CloseFileDescriptors(lockFD);

  return mWlBuffer != nullptr;
}

void DMABufSurfaceRGBA::ReleaseWlBuffer() {
  MozClearPointer(mWlBuffer, wl_buffer_destroy);
}
#endif

// We should synchronize DMA Buffer object access from CPU to avoid potential
// cache incoherency and data loss.
// See
// https://01.org/linuxgraphics/gfx-docs/drm/driver-api/dma-buf.html#cpu-access-to-dma-buffer-objects
struct dma_buf_sync {
  uint64_t flags;
};
#define DMA_BUF_SYNC_READ (1 << 0)
#define DMA_BUF_SYNC_WRITE (2 << 0)
#define DMA_BUF_SYNC_START (0 << 2)
#define DMA_BUF_SYNC_END (1 << 2)
#define DMA_BUF_BASE 'b'
#define DMA_BUF_IOCTL_SYNC _IOW(DMA_BUF_BASE, 0, struct dma_buf_sync)

static void SyncDmaBuf(int aFd, uint64_t aFlags) {
  struct dma_buf_sync sync = {0};

  sync.flags = aFlags | DMA_BUF_SYNC_READ | DMA_BUF_SYNC_WRITE;
  while (true) {
    int ret;
    ret = ioctl(aFd, DMA_BUF_IOCTL_SYNC, &sync);
    if (ret == -1 && errno == EINTR) {
      continue;
    } else if (ret == -1) {
      LOGDMABUF(
          ("Failed to synchronize DMA buffer: %s FD %d", strerror(errno), aFd));
      break;
    } else {
      break;
    }
  }
}

void* DMABufSurface::MapInternal(uint32_t aX, uint32_t aY, uint32_t aWidth,
                                 uint32_t aHeight, uint32_t* aStride,
                                 int aGbmFlags, int aPlane) {
  NS_ASSERTION(!IsMapped(aPlane), "Already mapped!");
  if (!mGbmBufferObject[aPlane]) {
    NS_WARNING("We can't map DMABufSurfaceRGBA without mGbmBufferObject");
    return nullptr;
  }

  LOGDMABUF(
      ("DMABufSurfaceRGBA::MapInternal() UID %d plane %d size %d x %d -> %d x "
       "%d\n",
       mUID, aPlane, aX, aY, aWidth, aHeight));

  mMappedRegionStride[aPlane] = 0;
  mMappedRegionData[aPlane] = nullptr;
  mMappedRegion[aPlane] =
      GbmLib::Map(mGbmBufferObject[aPlane], aX, aY, aWidth, aHeight, aGbmFlags,
                  &mMappedRegionStride[aPlane], &mMappedRegionData[aPlane]);
  if (!mMappedRegion[aPlane]) {
    LOGDMABUF(("    Surface mapping failed: %s", strerror(errno)));
    return nullptr;
  }
  if (aStride) {
    *aStride = mMappedRegionStride[aPlane];
  }

  MutexAutoLock lockFD(mSurfaceLock);
  if (OpenFileDescriptorForPlane(lockFD, aPlane)) {
    SyncDmaBuf(mDmabufFds[aPlane], DMA_BUF_SYNC_START);
    CloseFileDescriptorForPlane(lockFD, aPlane);
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
    LOGDMABUF(("DMABufSurface::Unmap() UID %d plane %d\n", mUID, aPlane));
    MutexAutoLock lockFD(mSurfaceLock);
    if (OpenFileDescriptorForPlane(lockFD, aPlane)) {
      SyncDmaBuf(mDmabufFds[aPlane], DMA_BUF_SYNC_END);
      CloseFileDescriptorForPlane(lockFD, aPlane);
    }
    GbmLib::Unmap(mGbmBufferObject[aPlane], mMappedRegionData[aPlane]);
    mMappedRegion[aPlane] = nullptr;
    mMappedRegionData[aPlane] = nullptr;
    mMappedRegionStride[aPlane] = 0;
  }
}

nsresult DMABufSurface::BuildSurfaceDescriptorBuffer(
    SurfaceDescriptorBuffer& aSdBuffer, Image::BuildSdbFlags aFlags,
    const std::function<MemoryOrShmem(uint32_t)>& aAllocate) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

#ifdef DEBUG
void DMABufSurfaceRGBA::DumpToFile(const char* pFile) {
  uint32_t stride;

  if (!MapReadOnly(&stride)) {
    return;
  }
  cairo_surface_t* surface = nullptr;

  auto unmap = MakeScopeExit([&] {
    if (surface) {
      cairo_surface_destroy(surface);
    }
    Unmap();
  });

  surface = cairo_image_surface_create_for_data(
      (unsigned char*)mMappedRegion[0], CAIRO_FORMAT_ARGB32, mWidth, mHeight,
      stride);
  if (cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS) {
    cairo_surface_write_to_png(surface, pFile);
  }
}
#endif

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
  return !mGmbFormat || mGmbFormat->mHasAlpha;
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

already_AddRefed<DMABufSurface> DMABufSurfaceRGBA::CreateDMABufSurface(
    mozilla::gl::GLContext* aGLContext, const EGLImageKHR aEGLImage, int aWidth,
    int aHeight) {
  RefPtr<DMABufSurfaceRGBA> surf = new DMABufSurfaceRGBA();
  if (!surf->Create(aGLContext, aEGLImage, aWidth, aHeight)) {
    return nullptr;
  }
  return surf.forget();
}

already_AddRefed<DMABufSurfaceYUV> DMABufSurfaceYUV::CreateYUVSurface(
    const VADRMPRIMESurfaceDescriptor& aDesc, int aWidth, int aHeight) {
  RefPtr<DMABufSurfaceYUV> surf = new DMABufSurfaceYUV();
  LOGDMABUF(("DMABufSurfaceYUV::CreateYUVSurface() UID %d from desc\n",
             surf->GetUID()));
  if (!surf->UpdateYUVData(aDesc, aWidth, aHeight, /* aCopy */ false)) {
    return nullptr;
  }
  return surf.forget();
}

already_AddRefed<DMABufSurfaceYUV> DMABufSurfaceYUV::CopyYUVSurface(
    const VADRMPRIMESurfaceDescriptor& aDesc, int aWidth, int aHeight) {
  RefPtr<DMABufSurfaceYUV> surf = new DMABufSurfaceYUV();
  LOGDMABUF(("DMABufSurfaceYUV::CreateYUVSurfaceCopy() UID %d from desc\n",
             surf->GetUID()));
  if (!surf->UpdateYUVData(aDesc, aWidth, aHeight, /* aCopy */ true)) {
    return nullptr;
  }
  return surf.forget();
}

already_AddRefed<DMABufSurfaceYUV> DMABufSurfaceYUV::CreateYUVSurface(
    int aWidth, int aHeight, void** aPixelData, int* aLineSizes) {
  RefPtr<DMABufSurfaceYUV> surf = new DMABufSurfaceYUV();
  LOGDMABUF(("DMABufSurfaceYUV::CreateYUVSurface() UID %d %d x %d\n",
             surf->GetUID(), aWidth, aHeight));
  if (!surf->Create(aWidth, aHeight, aPixelData, aLineSizes)) {
    return nullptr;
  }
  return surf.forget();
}

DMABufSurfaceYUV::DMABufSurfaceYUV()
    : DMABufSurface(SURFACE_NV12),
      mWidth(),
      mHeight(),
      mWidthAligned(),
      mHeightAligned(),
      mTexture() {
  for (int i = 0; i < DMABUF_BUFFER_PLANES; i++) {
    mEGLImage[i] = LOCAL_EGL_NO_IMAGE;
  }
}

DMABufSurfaceYUV::~DMABufSurfaceYUV() { ReleaseSurface(); }

bool DMABufSurfaceYUV::OpenFileDescriptorForPlane(
    const MutexAutoLock& aProofOfLock, int aPlane) {
  // The fd is already opened, no need to reopen.
  // This can happen when we import dmabuf surface from VA-API decoder,
  // mGbmBufferObject is null and we don't close
  // file descriptors for surface as they are our only reference to it.
  if (mDmabufFds[aPlane] >= 0) {
    return true;
  }

  if (mGbmBufferObject[aPlane] == nullptr) {
    LOGDMABUF(
        ("DMABufSurfaceYUV::OpenFileDescriptorForPlane: Missing "
         "mGbmBufferObject object!"));
    return false;
  }

  mDmabufFds[aPlane] = GbmLib::GetFd(mGbmBufferObject[aPlane]);
  if (mDmabufFds[aPlane] < 0) {
    CloseFileDescriptors(aProofOfLock);
    return false;
  }
  return true;
}

void DMABufSurfaceYUV::CloseFileDescriptorForPlane(
    const MutexAutoLock& aProofOfLock, int aPlane, bool aForceClose = false) {
  if ((aForceClose || mGbmBufferObject[aPlane]) && mDmabufFds[aPlane] >= 0) {
    close(mDmabufFds[aPlane]);
    mDmabufFds[aPlane] = -1;
  }
}

bool DMABufSurfaceYUV::ImportPRIMESurfaceDescriptor(
    const VADRMPRIMESurfaceDescriptor& aDesc, int aWidth, int aHeight) {
  LOGDMABUF(("DMABufSurfaceYUV::ImportPRIMESurfaceDescriptor() UID %d", mUID));
  // Already exists?
  MOZ_DIAGNOSTIC_ASSERT(mDmabufFds[0] < 0);

  if (aDesc.num_layers > DMABUF_BUFFER_PLANES ||
      aDesc.num_objects > DMABUF_BUFFER_PLANES) {
    LOGDMABUF(("  Can't import, wrong layers/objects number (%d, %d)",
               aDesc.num_layers, aDesc.num_objects));
    return false;
  }
  if (aDesc.fourcc == VA_FOURCC_NV12) {
    mSurfaceType = SURFACE_NV12;
  } else if (aDesc.fourcc == VA_FOURCC_P010) {
    mSurfaceType = SURFACE_NV12;
  } else if (aDesc.fourcc == VA_FOURCC_YV12) {
    mSurfaceType = SURFACE_YUV420;
  } else {
    LOGDMABUF(("  Can't import surface data of 0x%x format", aDesc.fourcc));
    return false;
  }

  mBufferPlaneCount = aDesc.num_layers;

  for (unsigned int i = 0; i < aDesc.num_layers; i++) {
    // All supported formats have 4:2:0 chroma sub-sampling.
    unsigned int subsample = i == 0 ? 0 : 1;

    unsigned int object = aDesc.layers[i].object_index[0];
    mBufferModifiers[i] = aDesc.objects[object].drm_format_modifier;
    mDrmFormats[i] = aDesc.layers[i].drm_format;
    mOffsets[i] = aDesc.layers[i].offset[0];
    mStrides[i] = aDesc.layers[i].pitch[0];
    mWidthAligned[i] = aDesc.width >> subsample;
    mHeightAligned[i] = aDesc.height >> subsample;
    mWidth[i] = aWidth >> subsample;
    mHeight[i] = aHeight >> subsample;
    LOGDMABUF(("    plane %d size %d x %d format %x", i, mWidth[i], mHeight[i],
               mDrmFormats[i]));
  }
  return true;
}

bool DMABufSurfaceYUV::MoveYUVDataImpl(const VADRMPRIMESurfaceDescriptor& aDesc,
                                       int aWidth, int aHeight) {
  if (!ImportPRIMESurfaceDescriptor(aDesc, aWidth, aHeight)) {
    return false;
  }
  for (unsigned int i = 0; i < aDesc.num_layers; i++) {
    unsigned int object = aDesc.layers[i].object_index[0];
    // Keep VADRMPRIMESurfaceDescriptor untouched and dup() dmabuf
    // file descriptors.
    mDmabufFds[i] = dup(aDesc.objects[object].fd);
  }
  return true;
}

void DMABufSurfaceYUV::ReleaseVADRMPRIMESurfaceDescriptor(
    VADRMPRIMESurfaceDescriptor& aDesc) {
  for (unsigned int i = 0; i < aDesc.num_layers; i++) {
    unsigned int object = aDesc.layers[i].object_index[0];
    if (aDesc.objects[object].fd != -1) {
      close(aDesc.objects[object].fd);
      aDesc.objects[object].fd = -1;
    }
  }
}

bool DMABufSurfaceYUV::CreateYUVPlane(int aPlane) {
  LOGDMABUF(("DMABufSurfaceYUV::CreateYUVPlane() UID %d size %d x %d", mUID,
             mWidth[aPlane], mHeight[aPlane]));

  if (!GetDMABufDevice()->GetGbmDevice()) {
    LOGDMABUF(("    Missing GbmDevice!"));
    return false;
  }

  MOZ_DIAGNOSTIC_ASSERT(mGbmBufferObject[aPlane] == nullptr);
  bool useModifiers = (mBufferModifiers[aPlane] != DRM_FORMAT_MOD_INVALID);
  if (useModifiers) {
    LOGDMABUF(("    Creating with modifiers"));
    mGbmBufferObject[aPlane] = GbmLib::CreateWithModifiers(
        GetDMABufDevice()->GetGbmDevice(), mWidth[aPlane], mHeight[aPlane],
        mDrmFormats[aPlane], mBufferModifiers + aPlane, 1);
  }
  if (!mGbmBufferObject[aPlane]) {
    LOGDMABUF(("    Creating without modifiers"));
    mGbmBufferObject[aPlane] = GbmLib::Create(
        GetDMABufDevice()->GetGbmDevice(), mWidth[aPlane], mHeight[aPlane],
        mDrmFormats[aPlane], GBM_BO_USE_RENDERING);
    mBufferModifiers[aPlane] = DRM_FORMAT_MOD_INVALID;
  }
  if (!mGbmBufferObject[aPlane]) {
    LOGDMABUF(("    Failed to create GbmBufferObject: %s", strerror(errno)));
    return false;
  }

  mStrides[aPlane] = GbmLib::GetStride(mGbmBufferObject[aPlane]);
  mOffsets[aPlane] = GbmLib::GetOffset(mGbmBufferObject[aPlane], 0);
  mWidthAligned[aPlane] = mWidth[aPlane];
  mHeightAligned[aPlane] = mHeight[aPlane];
  return true;
}

bool DMABufSurfaceYUV::CopyYUVDataImpl(const VADRMPRIMESurfaceDescriptor& aDesc,
                                       int aWidth, int aHeight) {
  RefPtr<DMABufSurfaceYUV> tmpSurf = CreateYUVSurface(aDesc, aWidth, aHeight);
  if (!tmpSurf) {
    return false;
  }

  if (!ImportPRIMESurfaceDescriptor(aDesc, aWidth, aHeight)) {
    return false;
  }

  StaticMutexAutoLock lock(sSnapshotContextMutex);
  RefPtr<GLContext> context = ClaimSnapshotGLContext();
  auto releaseTextures = MakeScopeExit([&] {
    tmpSurf->ReleaseTextures();
    ReleaseTextures();
    ReturnSnapshotGLContext(context);
  });

  for (int i = 0; i < mBufferPlaneCount; i++) {
    if (!tmpSurf->CreateTexture(context, i)) {
      return false;
    }
    if (!CreateYUVPlane(i) || !CreateTexture(context, i)) {
      return false;
    }
    gfx::IntSize size(GetWidth(i), GetHeight(i));
    context->BlitHelper()->BlitTextureToTexture(
        tmpSurf->GetTexture(i), GetTexture(i), size, size, LOCAL_GL_TEXTURE_2D,
        LOCAL_GL_TEXTURE_2D);
  }
  return true;
}

bool DMABufSurfaceYUV::UpdateYUVData(const VADRMPRIMESurfaceDescriptor& aDesc,
                                     int aWidth, int aHeight, bool aCopy) {
  LOGDMABUF(("DMABufSurfaceYUV::UpdateYUVData() UID %d copy %d", mUID, aCopy));
  return aCopy ? CopyYUVDataImpl(aDesc, aWidth, aHeight)
               : MoveYUVDataImpl(aDesc, aWidth, aHeight);
}

bool DMABufSurfaceYUV::CreateLinearYUVPlane(int aPlane, int aWidth, int aHeight,
                                            int aDrmFormat) {
  LOGDMABUF(("DMABufSurfaceYUV::CreateLinearYUVPlane() UID %d size %d x %d",
             mUID, aWidth, aHeight));

  if (!GetDMABufDevice()->GetGbmDevice()) {
    LOGDMABUF(("    Missing GbmDevice!"));
    return false;
  }

  mWidth[aPlane] = aWidth;
  mHeight[aPlane] = aHeight;
  mDrmFormats[aPlane] = aDrmFormat;

  mGbmBufferObject[aPlane] =
      GbmLib::Create(GetDMABufDevice()->GetGbmDevice(), aWidth, aHeight,
                     aDrmFormat, GBM_BO_USE_LINEAR);
  if (!mGbmBufferObject[aPlane]) {
    LOGDMABUF(("    Failed to create GbmBufferObject: %s", strerror(errno)));
    return false;
  }

  mStrides[aPlane] = GbmLib::GetStride(mGbmBufferObject[aPlane]);
  mDmabufFds[aPlane] = -1;

  return true;
}

void DMABufSurfaceYUV::UpdateYUVPlane(int aPlane, void* aPixelData,
                                      int aLineSize) {
  LOGDMABUF(
      ("DMABufSurfaceYUV::UpdateYUVPlane() UID %d plane %d", mUID, aPlane));
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
  LOGDMABUF(("DMABufSurfaceYUV::UpdateYUVData() UID %d", mUID));
  if (mSurfaceType != SURFACE_YUV420) {
    LOGDMABUF(("    UpdateYUVData can upload YUV420 surface type only!"));
    return false;
  }

  if (mBufferPlaneCount != 3) {
    LOGDMABUF(("    DMABufSurfaceYUV planes does not match!"));
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
      LOGDMABUF(("    DMABufSurfaceYUV plane can't be mapped!"));
      return false;
    }
    if ((int)mMappedRegionStride[i] < mWidth[i]) {
      LOGDMABUF(("    DMABufSurfaceYUV plane size stride does not match!"));
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
  LOGDMABUF(("DMABufSurfaceYUV::Create() UID %d size %d x %d", mUID, aWidth,
             aHeight));

  mSurfaceType = SURFACE_YUV420;
  mBufferPlaneCount = 3;

  if (!CreateLinearYUVPlane(0, aWidth, aHeight, GBM_FORMAT_R8)) {
    return false;
  }
  if (!CreateLinearYUVPlane(1, aWidth >> 1, aHeight >> 1, GBM_FORMAT_R8)) {
    return false;
  }
  if (!CreateLinearYUVPlane(2, aWidth >> 1, aHeight >> 1, GBM_FORMAT_R8)) {
    return false;
  }
  if (!aPixelData || !aLineSizes) {
    return true;
  }
  return UpdateYUVData(aPixelData, aLineSizes);
}

bool DMABufSurfaceYUV::Create(const SurfaceDescriptor& aDesc) {
  return ImportSurfaceDescriptor(aDesc);
}

bool DMABufSurfaceYUV::ImportSurfaceDescriptor(
    const SurfaceDescriptorDMABuf& aDesc) {
  mBufferPlaneCount = aDesc.fds().Length();
  mSurfaceType = (mBufferPlaneCount == 2) ? SURFACE_NV12 : SURFACE_YUV420;
  mColorSpace = aDesc.yUVColorSpace();
  mColorRange = aDesc.colorRange();
  mUID = aDesc.uid();

  LOGDMABUF(("DMABufSurfaceYUV::ImportSurfaceDescriptor() UID %d", mUID));

  MOZ_RELEASE_ASSERT(mBufferPlaneCount <= DMABUF_BUFFER_PLANES);
  for (int i = 0; i < mBufferPlaneCount; i++) {
    mDmabufFds[i] = aDesc.fds()[i].ClonePlatformHandle().release();
    if (mDmabufFds[i] < 0) {
      LOGDMABUF(("    failed to get DMABuf plane file descriptor: %s",
                 strerror(errno)));
      return false;
    }
    mWidth[i] = aDesc.width()[i];
    mHeight[i] = aDesc.height()[i];
    mWidthAligned[i] = aDesc.widthAligned()[i];
    mHeightAligned[i] = aDesc.heightAligned()[i];
    mDrmFormats[i] = aDesc.format()[i];
    mStrides[i] = aDesc.strides()[i];
    mOffsets[i] = aDesc.offsets()[i];
    mBufferModifiers[i] = aDesc.modifier()[i];
    LOGDMABUF(("    plane %d fd %d size %d x %d format %x", i, mDmabufFds[i],
               mWidth[i], mHeight[i], mDrmFormats[i]));
  }

  if (aDesc.fence().Length() > 0) {
    mSyncFd = aDesc.fence()[0].ClonePlatformHandle().release();
    if (mSyncFd < 0) {
      LOGDMABUF(
          ("    failed to get GL fence file descriptor: %s", strerror(errno)));
      return false;
    }
  }

  if (aDesc.refCount().Length() > 0) {
    GlobalRefCountImport(aDesc.refCount()[0].ClonePlatformHandle().release());
  }

  return true;
}

bool DMABufSurfaceYUV::Serialize(
    mozilla::layers::SurfaceDescriptor& aOutDescriptor) {
  AutoTArray<uint32_t, DMABUF_BUFFER_PLANES> width;
  AutoTArray<uint32_t, DMABUF_BUFFER_PLANES> height;
  AutoTArray<uint32_t, DMABUF_BUFFER_PLANES> widthBytes;
  AutoTArray<uint32_t, DMABUF_BUFFER_PLANES> heightBytes;
  AutoTArray<uint32_t, DMABUF_BUFFER_PLANES> format;
  AutoTArray<ipc::FileDescriptor, DMABUF_BUFFER_PLANES> fds;
  AutoTArray<uint32_t, DMABUF_BUFFER_PLANES> strides;
  AutoTArray<uint32_t, DMABUF_BUFFER_PLANES> offsets;
  AutoTArray<uint64_t, DMABUF_BUFFER_PLANES> modifiers;
  AutoTArray<ipc::FileDescriptor, 1> fenceFDs;
  AutoTArray<ipc::FileDescriptor, 1> refCountFDs;

  LOGDMABUF(("DMABufSurfaceYUV::Serialize() UID %d", mUID));

  MutexAutoLock lockFD(mSurfaceLock);
  if (!OpenFileDescriptors(lockFD)) {
    return false;
  }

  for (int i = 0; i < mBufferPlaneCount; i++) {
    width.AppendElement(mWidth[i]);
    height.AppendElement(mHeight[i]);
    widthBytes.AppendElement(mWidthAligned[i]);
    heightBytes.AppendElement(mHeightAligned[i]);
    format.AppendElement(mDrmFormats[i]);
    fds.AppendElement(ipc::FileDescriptor(mDmabufFds[i]));
    strides.AppendElement(mStrides[i]);
    offsets.AppendElement(mOffsets[i]);
    modifiers.AppendElement(mBufferModifiers[i]);
  }

  CloseFileDescriptors(lockFD);

  if (mSync) {
    fenceFDs.AppendElement(ipc::FileDescriptor(mSyncFd));
  }

  if (mGlobalRefCountFd) {
    refCountFDs.AppendElement(ipc::FileDescriptor(GlobalRefCountExport()));
  }

  aOutDescriptor = SurfaceDescriptorDMABuf(
      mSurfaceType, modifiers, 0, fds, width, height, widthBytes, heightBytes,
      format, strides, offsets, GetYUVColorSpace(), mColorRange, fenceFDs, mUID,
      refCountFDs);
  return true;
}

bool DMABufSurfaceYUV::CreateEGLImage(GLContext* aGLContext, int aPlane) {
  LOGDMABUF(
      ("DMABufSurfaceYUV::CreateEGLImage() UID %d plane %d", mUID, aPlane));
  MOZ_ASSERT(mEGLImage[aPlane] == LOCAL_EGL_NO_IMAGE,
             "EGLImage is already created!");
  MOZ_ASSERT(aGLContext, "Missing GLContext!");

  const auto& gle = gl::GLContextEGL::Cast(aGLContext);
  const auto& egl = gle->mEgl;

  MutexAutoLock lockFD(mSurfaceLock);
  if (!OpenFileDescriptorForPlane(lockFD, aPlane)) {
    LOGDMABUF(("  failed to open dmabuf file descriptors"));
    return false;
  }

  nsTArray<EGLint> attribs;
  attribs.AppendElement(LOCAL_EGL_WIDTH);
  attribs.AppendElement(mWidthAligned[aPlane]);
  attribs.AppendElement(LOCAL_EGL_HEIGHT);
  attribs.AppendElement(mHeightAligned[aPlane]);
  attribs.AppendElement(LOCAL_EGL_LINUX_DRM_FOURCC_EXT);
  attribs.AppendElement(mDrmFormats[aPlane]);
#define ADD_PLANE_ATTRIBS_NV12(plane_idx)                                 \
  attribs.AppendElement(LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_FD_EXT);     \
  attribs.AppendElement(mDmabufFds[aPlane]);                              \
  attribs.AppendElement(LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_OFFSET_EXT); \
  attribs.AppendElement((int)mOffsets[aPlane]);                           \
  attribs.AppendElement(LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_PITCH_EXT);  \
  attribs.AppendElement((int)mStrides[aPlane]);                           \
  if (mBufferModifiers[aPlane] != DRM_FORMAT_MOD_INVALID) {               \
    attribs.AppendElement(                                                \
        LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_MODIFIER_LO_EXT);            \
    attribs.AppendElement(mBufferModifiers[aPlane] & 0xFFFFFFFF);         \
    attribs.AppendElement(                                                \
        LOCAL_EGL_DMA_BUF_PLANE##plane_idx##_MODIFIER_HI_EXT);            \
    attribs.AppendElement(mBufferModifiers[aPlane] >> 32);                \
  }
  ADD_PLANE_ATTRIBS_NV12(0);
#undef ADD_PLANE_ATTRIBS_NV12
  attribs.AppendElement(LOCAL_EGL_NONE);

  mEGLImage[aPlane] =
      egl->fCreateImage(LOCAL_EGL_NO_CONTEXT, LOCAL_EGL_LINUX_DMA_BUF_EXT,
                        nullptr, attribs.Elements());

  CloseFileDescriptorForPlane(lockFD, aPlane);

  if (mEGLImage[aPlane] == LOCAL_EGL_NO_IMAGE) {
    LOGDMABUF(("  EGLImageKHR creation failed"));
    return false;
  }

  LOGDMABUF(("  Success."));
  return true;
}

void DMABufSurfaceYUV::ReleaseEGLImages(GLContext* aGLContext) {
  LOGDMABUF(("DMABufSurfaceYUV::ReleaseEGLImages() UID %d", mUID));
  MOZ_ASSERT(aGLContext, "Missing GLContext!");

  const auto& gle = gl::GLContextEGL::Cast(aGLContext);
  const auto& egl = gle->mEgl;

  for (int i = 0; i < mBufferPlaneCount; i++) {
    if (mEGLImage[i] != LOCAL_EGL_NO_IMAGE) {
      egl->fDestroyImage(mEGLImage[i]);
      mEGLImage[i] = LOCAL_EGL_NO_IMAGE;
    }
  }
}

bool DMABufSurfaceYUV::CreateTexture(GLContext* aGLContext, int aPlane) {
  LOGDMABUF(
      ("DMABufSurfaceYUV::CreateTexture() UID %d plane %d", mUID, aPlane));
  MOZ_ASSERT(!mTexture[aPlane], "Texture is already created!");
  MOZ_ASSERT(aGLContext, "Missing GLContext!");

  if (!aGLContext->MakeCurrent()) {
    LOGDMABUF(("  Failed to make GL context current."));
    return false;
  }
  if (!CreateEGLImage(aGLContext, aPlane)) {
    return false;
  }
  aGLContext->fGenTextures(1, &mTexture[aPlane]);
  const ScopedBindTexture savedTex(aGLContext, mTexture[aPlane]);
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
  LOGDMABUF(("DMABufSurfaceYUV::ReleaseTextures() UID %d", mUID));

  FenceDelete();

  bool textureActive = false;
  for (int i = 0; i < mBufferPlaneCount; i++) {
    if (mTexture[i] || mEGLImage[i]) {
      textureActive = true;
      break;
    }
  }

  if (!textureActive) {
    return;
  }

  if (!mGL) {
#ifdef NIGHTLY_BUILD
    MOZ_DIAGNOSTIC_ASSERT(mGL, "Missing GL context!");
#else
    NS_WARNING(
        "DMABufSurfaceYUV::ReleaseTextures(): Missing GL context! We're "
        "leaking textures!");
    return;
#endif
  }

  if (!mGL->MakeCurrent()) {
    NS_WARNING(
        "DMABufSurfaceYUV::ReleaseTextures(): MakeCurrent failed. We're "
        "leaking textures!");
    return;
  }

  mGL->fDeleteTextures(DMABUF_BUFFER_PLANES, mTexture);
  for (int i = 0; i < DMABUF_BUFFER_PLANES; i++) {
    mTexture[i] = 0;
  }
  ReleaseEGLImages(mGL);
  mGL = nullptr;
}

bool DMABufSurfaceYUV::VerifyTextureCreation() {
  LOGDMABUF(("DMABufSurfaceYUV::VerifyTextureCreation() UID %d", mUID));

  StaticMutexAutoLock lock(sSnapshotContextMutex);
  RefPtr<GLContext> context = ClaimSnapshotGLContext();
  auto release = MakeScopeExit([&] {
    ReleaseEGLImages(context);
    ReturnSnapshotGLContext(context);
  });

  for (int i = 0; i < mBufferPlaneCount; i++) {
    if (!CreateEGLImage(context, i)) {
      LOGDMABUF(("  failed to create EGL image!"));
      return false;
    }
  }

  LOGDMABUF(("  success"));
  return true;
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

int DMABufSurfaceYUV::GetTextureCount() {
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
  LOGDMABUF(("DMABufSurfaceYUV::ReleaseSurface() UID %d", mUID));
  ReleaseTextures();
  ReleaseDMABuf();
}

nsresult DMABufSurfaceYUV::ReadIntoBuffer(uint8_t* aData, int32_t aStride,
                                          const gfx::IntSize& aSize,
                                          gfx::SurfaceFormat aFormat) {
  LOGDMABUF(("DMABufSurfaceYUV::ReadIntoBuffer UID %d", mUID));

  MOZ_ASSERT(aSize.width == GetWidth());
  MOZ_ASSERT(aSize.height == GetHeight());

  StaticMutexAutoLock lock(sSnapshotContextMutex);
  RefPtr<GLContext> context = ClaimSnapshotGLContext();
  auto releaseTextures = mozilla::MakeScopeExit([&] {
    ReleaseTextures();
    ReturnSnapshotGLContext(context);
  });

  for (int i = 0; i < GetTextureCount(); i++) {
    if (!GetTexture(i) && !CreateTexture(context, i)) {
      LOGDMABUF(("ReadIntoBuffer: Failed to create DMABuf textures."));
      return NS_ERROR_FAILURE;
    }
  }

  ScopedTexture scopedTex(context);
  ScopedBindTexture boundTex(context, scopedTex.Texture());

  context->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0, LOCAL_GL_RGBA, aSize.width,
                       aSize.height, 0, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE,
                       nullptr);

  ScopedFramebufferForTexture autoFBForTex(context, scopedTex.Texture());
  if (!autoFBForTex.IsComplete()) {
    LOGDMABUF(("ReadIntoBuffer: ScopedFramebufferForTexture failed."));
    return NS_ERROR_FAILURE;
  }

  const gl::OriginPos destOrigin = gl::OriginPos::BottomLeft;
  {
    const ScopedBindFramebuffer bindFB(context, autoFBForTex.FB());
    if (!context->BlitHelper()->Blit(this, aSize, destOrigin)) {
      LOGDMABUF(("ReadIntoBuffer: Blit failed."));
      return NS_ERROR_FAILURE;
    }
  }

  ScopedBindFramebuffer bind(context, autoFBForTex.FB());
  ReadPixelsIntoBuffer(context, aData, aStride, aSize, aFormat);
  return NS_OK;
}

already_AddRefed<gfx::DataSourceSurface>
DMABufSurfaceYUV::GetAsSourceSurface() {
  LOGDMABUF(("DMABufSurfaceYUV::GetAsSourceSurface UID %d", mUID));

  gfx::IntSize size(GetWidth(), GetHeight());
  const auto format = gfx::SurfaceFormat::B8G8R8A8;
  RefPtr<gfx::DataSourceSurface> source =
      gfx::Factory::CreateDataSourceSurface(size, format);
  if (NS_WARN_IF(!source)) {
    LOGDMABUF(("GetAsSourceSurface: CreateDataSourceSurface failed."));
    return nullptr;
  }

  gfx::DataSourceSurface::ScopedMap map(source,
                                        gfx::DataSourceSurface::READ_WRITE);
  if (NS_WARN_IF(!map.IsMapped())) {
    LOGDMABUF(("GetAsSourceSurface: Mapping surface failed."));
    return nullptr;
  }

  if (NS_WARN_IF(NS_FAILED(
          ReadIntoBuffer(map.GetData(), map.GetStride(), size, format)))) {
    LOGDMABUF(("GetAsSourceSurface: Reading into buffer failed."));
    return nullptr;
  }

  return source.forget();
}

nsresult DMABufSurfaceYUV::BuildSurfaceDescriptorBuffer(
    SurfaceDescriptorBuffer& aSdBuffer, Image::BuildSdbFlags aFlags,
    const std::function<MemoryOrShmem(uint32_t)>& aAllocate) {
  LOGDMABUF(("DMABufSurfaceYUV::BuildSurfaceDescriptorBuffer UID %d", mUID));

  gfx::IntSize size(GetWidth(), GetHeight());
  const auto format = gfx::SurfaceFormat::B8G8R8A8;

  uint8_t* buffer = nullptr;
  int32_t stride = 0;
  nsresult rv = Image::AllocateSurfaceDescriptorBufferRgb(
      size, format, buffer, aSdBuffer, stride, aAllocate);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOGDMABUF(("BuildSurfaceDescriptorBuffer allocate descriptor failed"));
    return rv;
  }

  return ReadIntoBuffer(buffer, stride, size, format);
}

#if 0
void DMABufSurfaceYUV::ClearPlane(int aPlane) {
  if (!MapInternal(0, 0, mWidth[aPlane], mHeight[aPlane], nullptr,
                   GBM_BO_TRANSFER_WRITE, aPlane)) {
    return;
  }
  if ((int)mMappedRegionStride[aPlane] < mWidth[aPlane]) {
    return;
  }
  memset((char*)mMappedRegion[aPlane], 0,
         mMappedRegionStride[aPlane] * mHeight[aPlane]);
  Unmap(aPlane);
}

#  include "gfxUtils.h"

void DMABufSurfaceYUV::DumpToFile(const char* aFile) {
  RefPtr<gfx::DataSourceSurface> surf = GetAsSourceSurface();
  gfxUtils::WriteAsPNG(surf, aFile);
}
#endif
