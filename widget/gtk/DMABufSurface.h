/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DMABufSurface_h__
#define DMABufSurface_h__

#include <stdint.h>
#include "mozilla/widget/nsWaylandDisplay.h"
#include "mozilla/widget/va_drmcommon.h"
#include "GLTypes.h"

typedef void* EGLImageKHR;
typedef void* EGLSyncKHR;

#define DMABUF_BUFFER_PLANES 4

namespace mozilla {
namespace layers {
class SurfaceDescriptor;
class SurfaceDescriptorDMABuf;
}  // namespace layers
namespace gl {
class GLContext;
}
}  // namespace mozilla

typedef enum {
  // Use alpha pixel format
  DMABUF_ALPHA = 1 << 0,
  // Surface is used as texture and may be also shared
  DMABUF_TEXTURE = 1 << 1,
  // Use modifiers. Such dmabuf surface may have more planes
  // and complex internal structure (tiling/compression/etc.)
  // so we can't do direct rendering to it.
  DMABUF_USE_MODIFIERS = 1 << 3,
} DMABufSurfaceFlags;

class DMABufSurfaceRGBA;
class DMABufSurfaceYUV;

class DMABufSurface {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DMABufSurface)

  enum SurfaceType {
    SURFACE_RGBA,
    SURFACE_NV12,
    SURFACE_YUV420,
  };

  // Import surface from SurfaceDescriptor. This is usually
  // used to copy surface from another process over IPC.
  // When a global reference counter was created for the surface
  // (see bellow) it's automatically referenced.
  static already_AddRefed<DMABufSurface> CreateDMABufSurface(
      const mozilla::layers::SurfaceDescriptor& aDesc);

  // Export surface to another process via. SurfaceDescriptor.
  virtual bool Serialize(
      mozilla::layers::SurfaceDescriptor& aOutDescriptor) = 0;

  virtual int GetWidth(int aPlane = 0) = 0;
  virtual int GetHeight(int aPlane = 0) = 0;
  virtual mozilla::gfx::SurfaceFormat GetFormat() = 0;
  virtual mozilla::gfx::SurfaceFormat GetFormatGL() = 0;

  virtual bool CreateTexture(mozilla::gl::GLContext* aGLContext,
                             int aPlane = 0) = 0;
  virtual void ReleaseTextures() = 0;
  virtual GLuint GetTexture(int aPlane = 0) = 0;
  virtual EGLImageKHR GetEGLImage(int aPlane = 0) = 0;

  SurfaceType GetSurfaceType() { return mSurfaceType; };
  virtual uint32_t GetTextureCount() = 0;

  bool IsMapped(int aPlane = 0) { return (mMappedRegion[aPlane] != nullptr); };
  void Unmap(int aPlane = 0);

  virtual DMABufSurfaceRGBA* GetAsDMABufSurfaceRGBA() { return nullptr; }
  virtual DMABufSurfaceYUV* GetAsDMABufSurfaceYUV() { return nullptr; }

  virtual mozilla::gfx::YUVColorSpace GetYUVColorSpace() {
    return mozilla::gfx::YUVColorSpace::UNKNOWN;
  };
  virtual bool IsFullRange() { return false; };

  void FenceSet();
  void FenceWait();
  void FenceDelete();

  // Set and get a global surface UID. The UID is shared across process
  // and it's used to track surface lifetime in various parts of rendering
  // engine.
  uint32_t GetUID() const { return mUID; };

  // Creates a global reference counter objects attached to the surface.
  // It's created as unreferenced, i.e. IsGlobalRefSet() returns false
  // right after GlobalRefCountCreate() call.
  //
  // The counter is shared by all surface instances across processes
  // so it tracks global surface usage.
  //
  // The counter is automatically referenced when a new surface instance is
  // created with SurfaceDescriptor (usually copied to another process over IPC)
  // and it's unreferenced when surface is deleted.
  //
  // So without any additional GlobalRefAdd()/GlobalRefRelease() calls
  // the IsGlobalRefSet() returns true if any other process use the surface.
  void GlobalRefCountCreate();

  // If global reference counter was created by GlobalRefCountCreate()
  // returns true when there's an active surface reference.
  bool IsGlobalRefSet() const;

  // Add/Remove additional reference to the surface global reference counter.
  void GlobalRefAdd();
  void GlobalRefRelease();

  // Release all underlying data.
  virtual void ReleaseSurface() = 0;

#ifdef DEBUG
  virtual void DumpToFile(const char* pFile){};
#endif

  DMABufSurface(SurfaceType aSurfaceType);

 protected:
  virtual bool Create(const mozilla::layers::SurfaceDescriptor& aDesc) = 0;
  bool FenceImportFromFd();

  void GlobalRefCountImport(int aFd);
  void GlobalRefCountDelete();

  void ReleaseDMABuf();

  void* MapInternal(uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight,
                    uint32_t* aStride, int aGbmFlags, int aPlane = 0);

  // We want to keep number of opened file descriptors low so open/close
  // DMABuf file handles only when we need them, i.e. when DMABuf is exported
  // to another process or to EGL.
  virtual bool OpenFileDescriptorForPlane(int aPlane) = 0;
  virtual void CloseFileDescriptorForPlane(int aPlane,
                                           bool aForceClose = false) = 0;
  bool OpenFileDescriptors();
  void CloseFileDescriptors(bool aForceClose = false);

  virtual ~DMABufSurface();

  SurfaceType mSurfaceType;
  uint64_t mBufferModifier;

  int mBufferPlaneCount;
  int mDmabufFds[DMABUF_BUFFER_PLANES];
  uint32_t mDrmFormats[DMABUF_BUFFER_PLANES];
  uint32_t mStrides[DMABUF_BUFFER_PLANES];
  uint32_t mOffsets[DMABUF_BUFFER_PLANES];

  struct gbm_bo* mGbmBufferObject[DMABUF_BUFFER_PLANES];
  void* mMappedRegion[DMABUF_BUFFER_PLANES];
  void* mMappedRegionData[DMABUF_BUFFER_PLANES];
  uint32_t mMappedRegionStride[DMABUF_BUFFER_PLANES];

  int mSyncFd;
  EGLSyncKHR mSync;
  RefPtr<mozilla::gl::GLContext> mGL;

  int mGlobalRefCountFd;
  uint32_t mUID;
  mozilla::Mutex mSurfaceLock;
};

class DMABufSurfaceRGBA : public DMABufSurface {
 public:
  static already_AddRefed<DMABufSurfaceRGBA> CreateDMABufSurface(
      int aWidth, int aHeight, int aDMABufSurfaceFlags);

  bool Serialize(mozilla::layers::SurfaceDescriptor& aOutDescriptor);

  DMABufSurfaceRGBA* GetAsDMABufSurfaceRGBA() { return this; }

  void Clear();

  void ReleaseSurface();

  bool CopyFrom(class DMABufSurface* aSourceSurface);

  int GetWidth(int aPlane = 0) { return mWidth; };
  int GetHeight(int aPlane = 0) { return mHeight; };
  mozilla::gfx::SurfaceFormat GetFormat();
  mozilla::gfx::SurfaceFormat GetFormatGL();
  bool HasAlpha();

  void* MapReadOnly(uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight,
                    uint32_t* aStride = nullptr);
  void* MapReadOnly(uint32_t* aStride = nullptr);
  void* Map(uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight,
            uint32_t* aStride = nullptr);
  void* Map(uint32_t* aStride = nullptr);
  void* GetMappedRegion(int aPlane = 0) { return mMappedRegion[aPlane]; };
  uint32_t GetMappedRegionStride(int aPlane = 0) {
    return mMappedRegionStride[aPlane];
  };

  bool CreateTexture(mozilla::gl::GLContext* aGLContext, int aPlane = 0);
  void ReleaseTextures();
  GLuint GetTexture(int aPlane = 0) { return mTexture; };
  EGLImageKHR GetEGLImage(int aPlane = 0) { return mEGLImage; };

  uint32_t GetTextureCount() { return 1; };

#ifdef DEBUG
  virtual void DumpToFile(const char* pFile);
#endif

  DMABufSurfaceRGBA();

 private:
  ~DMABufSurfaceRGBA();

  bool Create(int aWidth, int aHeight, int aDMABufSurfaceFlags);
  bool Create(const mozilla::layers::SurfaceDescriptor& aDesc);

  bool ImportSurfaceDescriptor(const mozilla::layers::SurfaceDescriptor& aDesc);

  bool OpenFileDescriptorForPlane(int aPlane);
  void CloseFileDescriptorForPlane(int aPlane, bool aForceClose);

 private:
  int mSurfaceFlags;

  int mWidth;
  int mHeight;
  mozilla::widget::GbmFormat* mGmbFormat;

  EGLImageKHR mEGLImage;
  GLuint mTexture;
  uint32_t mGbmBufferFlags;
};

class DMABufSurfaceYUV : public DMABufSurface {
 public:
  static already_AddRefed<DMABufSurfaceYUV> CreateYUVSurface(
      int aWidth, int aHeight, void** aPixelData = nullptr,
      int* aLineSizes = nullptr);

  static already_AddRefed<DMABufSurfaceYUV> CreateYUVSurface(
      const VADRMPRIMESurfaceDescriptor& aDesc);

  bool Serialize(mozilla::layers::SurfaceDescriptor& aOutDescriptor);

  DMABufSurfaceYUV* GetAsDMABufSurfaceYUV() { return this; };

  int GetWidth(int aPlane = 0) { return mWidth[aPlane]; }
  int GetHeight(int aPlane = 0) { return mHeight[aPlane]; }
  mozilla::gfx::SurfaceFormat GetFormat();
  mozilla::gfx::SurfaceFormat GetFormatGL();

  bool CreateTexture(mozilla::gl::GLContext* aGLContext, int aPlane = 0);
  void ReleaseTextures();

  void ReleaseSurface();

  GLuint GetTexture(int aPlane = 0) { return mTexture[aPlane]; };
  EGLImageKHR GetEGLImage(int aPlane = 0) { return mEGLImage[aPlane]; };

  uint32_t GetTextureCount();

  void SetYUVColorSpace(mozilla::gfx::YUVColorSpace aColorSpace) {
    mColorSpace = aColorSpace;
  }
  mozilla::gfx::YUVColorSpace GetYUVColorSpace() { return mColorSpace; }

  bool IsFullRange() { return true; }

  DMABufSurfaceYUV();

  bool UpdateYUVData(void** aPixelData, int* aLineSizes);
  bool UpdateYUVData(const VADRMPRIMESurfaceDescriptor& aDesc);

 private:
  ~DMABufSurfaceYUV();

  bool Create(const mozilla::layers::SurfaceDescriptor& aDesc);
  bool Create(int aWidth, int aHeight, void** aPixelData, int* aLineSizes);
  bool CreateYUVPlane(int aPlane, int aWidth, int aHeight, int aDrmFormat);
  void UpdateYUVPlane(int aPlane, void* aPixelData, int aLineSize);

  bool ImportSurfaceDescriptor(
      const mozilla::layers::SurfaceDescriptorDMABuf& aDesc);

  bool OpenFileDescriptorForPlane(int aPlane);
  void CloseFileDescriptorForPlane(int aPlane, bool aForceClose);

  int mWidth[DMABUF_BUFFER_PLANES];
  int mHeight[DMABUF_BUFFER_PLANES];
  EGLImageKHR mEGLImage[DMABUF_BUFFER_PLANES];
  GLuint mTexture[DMABUF_BUFFER_PLANES];
  mozilla::gfx::YUVColorSpace mColorSpace;
};

#endif
