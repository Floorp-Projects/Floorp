/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DMABufSurface_h__
#define DMABufSurface_h__

#include <functional>
#include <stdint.h>
#include "mozilla/widget/va_drmcommon.h"
#include "GLTypes.h"
#include "ImageContainer.h"
#include "nsISupportsImpl.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/Mutex.h"

typedef void* EGLImageKHR;
typedef void* EGLSyncKHR;

#define DMABUF_BUFFER_PLANES 4

#ifndef VA_FOURCC_NV12
#  define VA_FOURCC_NV12 0x3231564E
#endif
#ifndef VA_FOURCC_YV12
#  define VA_FOURCC_YV12 0x32315659
#endif
#ifndef VA_FOURCC_P010
#  define VA_FOURCC_P010 0x30313050
#endif

namespace mozilla {
namespace gfx {
class DataSourceSurface;
}
namespace layers {
class MemoryOrShmem;
class SurfaceDescriptor;
class SurfaceDescriptorBuffer;
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
struct wl_buffer;

namespace mozilla::widget {
struct GbmFormat;
}

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
  virtual int GetTextureCount() = 0;

  bool IsMapped(int aPlane = 0) { return (mMappedRegion[aPlane] != nullptr); };
  void Unmap(int aPlane = 0);

  virtual DMABufSurfaceRGBA* GetAsDMABufSurfaceRGBA() { return nullptr; }
  virtual DMABufSurfaceYUV* GetAsDMABufSurfaceYUV() { return nullptr; }
  virtual already_AddRefed<mozilla::gfx::DataSourceSurface>
  GetAsSourceSurface() {
    return nullptr;
  }

  virtual nsresult BuildSurfaceDescriptorBuffer(
      mozilla::layers::SurfaceDescriptorBuffer& aSdBuffer,
      mozilla::layers::Image::BuildSdbFlags aFlags,
      const std::function<mozilla::layers::MemoryOrShmem(uint32_t)>& aAllocate);

  virtual mozilla::gfx::YUVColorSpace GetYUVColorSpace() {
    return mozilla::gfx::YUVColorSpace::Default;
  };

  bool IsFullRange() { return mColorRange == mozilla::gfx::ColorRange::FULL; };
  void SetColorRange(mozilla::gfx::ColorRange aColorRange) {
    mColorRange = aColorRange;
  };

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
  void GlobalRefCountDelete();

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

  // Import global ref count object from IPC by file descriptor.
  // This adds global ref count reference to the surface.
  void GlobalRefCountImport(int aFd);
  // Export global ref count object by file descriptor.
  int GlobalRefCountExport();

  void ReleaseDMABuf();

  void* MapInternal(uint32_t aX, uint32_t aY, uint32_t aWidth, uint32_t aHeight,
                    uint32_t* aStride, int aGbmFlags, int aPlane = 0);

  // We want to keep number of opened file descriptors low so open/close
  // DMABuf file handles only when we need them, i.e. when DMABuf is exported
  // to another process or to EGL.
  virtual bool OpenFileDescriptorForPlane(
      const mozilla::MutexAutoLock& aProofOfLock, int aPlane) = 0;
  virtual void CloseFileDescriptorForPlane(
      const mozilla::MutexAutoLock& aProofOfLock, int aPlane,
      bool aForceClose = false) = 0;
  bool OpenFileDescriptors(const mozilla::MutexAutoLock& aProofOfLock);
  void CloseFileDescriptors(const mozilla::MutexAutoLock& aProofOfLock,
                            bool aForceClose = false);

  virtual ~DMABufSurface();

  SurfaceType mSurfaceType;
  uint64_t mBufferModifiers[DMABUF_BUFFER_PLANES];

  int mBufferPlaneCount;
  int mDmabufFds[DMABUF_BUFFER_PLANES];
  int32_t mDrmFormats[DMABUF_BUFFER_PLANES];
  int32_t mStrides[DMABUF_BUFFER_PLANES];
  int32_t mOffsets[DMABUF_BUFFER_PLANES];

  struct gbm_bo* mGbmBufferObject[DMABUF_BUFFER_PLANES];
  void* mMappedRegion[DMABUF_BUFFER_PLANES];
  void* mMappedRegionData[DMABUF_BUFFER_PLANES];
  uint32_t mMappedRegionStride[DMABUF_BUFFER_PLANES];

  int mSyncFd;
  EGLSyncKHR mSync;
  RefPtr<mozilla::gl::GLContext> mGL;

  int mGlobalRefCountFd;
  uint32_t mUID;
  mozilla::Mutex mSurfaceLock MOZ_UNANNOTATED;

  mozilla::gfx::ColorRange mColorRange = mozilla::gfx::ColorRange::LIMITED;
};

class DMABufSurfaceRGBA final : public DMABufSurface {
 public:
  static already_AddRefed<DMABufSurfaceRGBA> CreateDMABufSurface(
      int aWidth, int aHeight, int aDMABufSurfaceFlags);

  static already_AddRefed<DMABufSurface> CreateDMABufSurface(
      mozilla::gl::GLContext* aGLContext, const EGLImageKHR aEGLImage,
      int aWidth, int aHeight);

  bool Serialize(mozilla::layers::SurfaceDescriptor& aOutDescriptor) override;

  DMABufSurfaceRGBA* GetAsDMABufSurfaceRGBA() override { return this; }

  void Clear();

  void ReleaseSurface() override;

  bool CopyFrom(class DMABufSurface* aSourceSurface);

  int GetWidth(int aPlane = 0) override { return mWidth; };
  int GetHeight(int aPlane = 0) override { return mHeight; };
  mozilla::gfx::SurfaceFormat GetFormat() override;
  mozilla::gfx::SurfaceFormat GetFormatGL() override;
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

  bool CreateTexture(mozilla::gl::GLContext* aGLContext,
                     int aPlane = 0) override;
  void ReleaseTextures() override;
  GLuint GetTexture(int aPlane = 0) override { return mTexture; };
  EGLImageKHR GetEGLImage(int aPlane = 0) override { return mEGLImage; };

#ifdef MOZ_WAYLAND
  bool CreateWlBuffer();
  void ReleaseWlBuffer();
  wl_buffer* GetWlBuffer() { return mWlBuffer; };
#endif

  int GetTextureCount() override { return 1; };

#ifdef DEBUG
  void DumpToFile(const char* pFile) override;
#endif

  DMABufSurfaceRGBA();

 private:
  DMABufSurfaceRGBA(const DMABufSurfaceRGBA&) = delete;
  DMABufSurfaceRGBA& operator=(const DMABufSurfaceRGBA&) = delete;
  ~DMABufSurfaceRGBA();

  bool Create(int aWidth, int aHeight, int aDMABufSurfaceFlags);
  bool Create(const mozilla::layers::SurfaceDescriptor& aDesc) override;
  bool Create(mozilla::gl::GLContext* aGLContext, const EGLImageKHR aEGLImage,
              int aWidth, int aHeight);

  bool ImportSurfaceDescriptor(const mozilla::layers::SurfaceDescriptor& aDesc);

  bool OpenFileDescriptorForPlane(const mozilla::MutexAutoLock& aProofOfLock,
                                  int aPlane) override;
  void CloseFileDescriptorForPlane(const mozilla::MutexAutoLock& aProofOfLock,
                                   int aPlane, bool aForceClose) override;

 private:
  int mSurfaceFlags;

  int mWidth;
  int mHeight;
  mozilla::widget::GbmFormat* mGmbFormat;

  EGLImageKHR mEGLImage;
  GLuint mTexture;
  uint32_t mGbmBufferFlags;
#ifdef MOZ_WAYLAND
  wl_buffer* mWlBuffer = nullptr;
#endif
};

class DMABufSurfaceYUV final : public DMABufSurface {
 public:
  static already_AddRefed<DMABufSurfaceYUV> CreateYUVSurface(
      int aWidth, int aHeight, void** aPixelData = nullptr,
      int* aLineSizes = nullptr);
  static already_AddRefed<DMABufSurfaceYUV> CreateYUVSurface(
      const VADRMPRIMESurfaceDescriptor& aDesc, int aWidth, int aHeight);
  static already_AddRefed<DMABufSurfaceYUV> CopyYUVSurface(
      const VADRMPRIMESurfaceDescriptor& aVaDesc, int aWidth, int aHeight);
  static void ReleaseVADRMPRIMESurfaceDescriptor(
      VADRMPRIMESurfaceDescriptor& aDesc);

  bool Serialize(mozilla::layers::SurfaceDescriptor& aOutDescriptor) override;

  DMABufSurfaceYUV* GetAsDMABufSurfaceYUV() override { return this; };
  already_AddRefed<mozilla::gfx::DataSourceSurface> GetAsSourceSurface()
      override;

  nsresult BuildSurfaceDescriptorBuffer(
      mozilla::layers::SurfaceDescriptorBuffer& aSdBuffer,
      mozilla::layers::Image::BuildSdbFlags aFlags,
      const std::function<mozilla::layers::MemoryOrShmem(uint32_t)>& aAllocate)
      override;

  int GetWidth(int aPlane = 0) override { return mWidth[aPlane]; }
  int GetHeight(int aPlane = 0) override { return mHeight[aPlane]; }
  mozilla::gfx::SurfaceFormat GetFormat() override;
  mozilla::gfx::SurfaceFormat GetFormatGL() override;

  bool CreateTexture(mozilla::gl::GLContext* aGLContext,
                     int aPlane = 0) override;
  void ReleaseTextures() override;

  void ReleaseSurface() override;

  GLuint GetTexture(int aPlane = 0) override { return mTexture[aPlane]; };
  EGLImageKHR GetEGLImage(int aPlane = 0) override {
    return mEGLImage[aPlane];
  };

  int GetTextureCount() override;

  void SetYUVColorSpace(mozilla::gfx::YUVColorSpace aColorSpace) {
    mColorSpace = aColorSpace;
  }
  mozilla::gfx::YUVColorSpace GetYUVColorSpace() override {
    return mColorSpace;
  }

  DMABufSurfaceYUV();

  bool UpdateYUVData(void** aPixelData, int* aLineSizes);
  bool UpdateYUVData(const VADRMPRIMESurfaceDescriptor& aDesc, int aWidth,
                     int aHeight, bool aCopy);
  bool VerifyTextureCreation();

 private:
  DMABufSurfaceYUV(const DMABufSurfaceYUV&) = delete;
  DMABufSurfaceYUV& operator=(const DMABufSurfaceYUV&) = delete;
  ~DMABufSurfaceYUV();

  bool Create(const mozilla::layers::SurfaceDescriptor& aDesc) override;
  bool Create(int aWidth, int aHeight, void** aPixelData, int* aLineSizes);
  bool CreateYUVPlane(int aPlane);
  bool CreateLinearYUVPlane(int aPlane, int aWidth, int aHeight,
                            int aDrmFormat);
  void UpdateYUVPlane(int aPlane, void* aPixelData, int aLineSize);

  bool MoveYUVDataImpl(const VADRMPRIMESurfaceDescriptor& aDesc, int aWidth,
                       int aHeight);
  bool CopyYUVDataImpl(const VADRMPRIMESurfaceDescriptor& aDesc, int aWidth,
                       int aHeight);

  bool ImportPRIMESurfaceDescriptor(const VADRMPRIMESurfaceDescriptor& aDesc,
                                    int aWidth, int aHeight);
  bool ImportSurfaceDescriptor(
      const mozilla::layers::SurfaceDescriptorDMABuf& aDesc);

  bool OpenFileDescriptorForPlane(const mozilla::MutexAutoLock& aProofOfLock,
                                  int aPlane) override;
  void CloseFileDescriptorForPlane(const mozilla::MutexAutoLock& aProofOfLock,
                                   int aPlane, bool aForceClose) override;

  bool CreateEGLImage(mozilla::gl::GLContext* aGLContext, int aPlane);
  void ReleaseEGLImages(mozilla::gl::GLContext* aGLContext);

  nsresult ReadIntoBuffer(uint8_t* aData, int32_t aStride,
                          const mozilla::gfx::IntSize& aSize,
                          mozilla::gfx::SurfaceFormat aFormat);

  int mWidth[DMABUF_BUFFER_PLANES];
  int mHeight[DMABUF_BUFFER_PLANES];
  // Aligned size of the surface imported from VADRMPRIMESurfaceDescriptor.
  // It's used only internally to create EGLImage as some GL drivers
  // needs that (Bug 1724385).
  int mWidthAligned[DMABUF_BUFFER_PLANES];
  int mHeightAligned[DMABUF_BUFFER_PLANES];
  EGLImageKHR mEGLImage[DMABUF_BUFFER_PLANES];
  GLuint mTexture[DMABUF_BUFFER_PLANES];
  mozilla::gfx::YUVColorSpace mColorSpace =
      mozilla::gfx::YUVColorSpace::Default;
};

#endif
