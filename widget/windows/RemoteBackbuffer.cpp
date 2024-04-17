/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteBackbuffer.h"
#include "GeckoProfiler.h"
#include "nsThreadUtils.h"
#include "mozilla/Span.h"
#include "mozilla/gfx/Point.h"
#include "WinUtils.h"
#include <algorithm>
#include <type_traits>

namespace mozilla {
namespace widget {
namespace remote_backbuffer {

// This number can be adjusted as a time-memory tradeoff
constexpr uint8_t kMaxDirtyRects = 8;

struct IpcSafeRect {
  explicit IpcSafeRect(const gfx::IntRect& aRect)
      : x(aRect.x), y(aRect.y), width(aRect.width), height(aRect.height) {}
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
};

enum class ResponseResult {
  Unknown,
  Error,
  BorrowSuccess,
  BorrowSameBuffer,
  PresentSuccess
};

enum class SharedDataType {
  BorrowRequest,
  BorrowRequestAllowSameBuffer,
  BorrowResponse,
  PresentRequest,
  PresentResponse
};

struct BorrowResponseData {
  ResponseResult result;
  int32_t width;
  int32_t height;
  HANDLE fileMapping;
};

struct PresentRequestData {
  uint8_t lenDirtyRects;
  IpcSafeRect dirtyRects[kMaxDirtyRects];
};

struct PresentResponseData {
  ResponseResult result;
};

struct SharedData {
  SharedDataType dataType;
  union {
    BorrowResponseData borrowResponse;
    PresentRequestData presentRequest;
    PresentResponseData presentResponse;
  } data;
};

static_assert(std::is_trivially_copyable<SharedData>::value &&
                  std::is_standard_layout<SharedData>::value,
              "SharedData must be safe to pass over IPC boundaries");

class SharedImage {
 public:
  SharedImage()
      : mWidth(0), mHeight(0), mFileMapping(nullptr), mPixelData(nullptr) {}

  ~SharedImage() {
    if (mPixelData) {
      MOZ_ALWAYS_TRUE(::UnmapViewOfFile(mPixelData));
    }

    if (mFileMapping) {
      MOZ_ALWAYS_TRUE(::CloseHandle(mFileMapping));
    }
  }

  bool Initialize(int32_t aWidth, int32_t aHeight) {
    MOZ_ASSERT(aWidth);
    MOZ_ASSERT(aHeight);
    MOZ_ASSERT(aWidth > 0);
    MOZ_ASSERT(aHeight > 0);

    mWidth = aWidth;
    mHeight = aHeight;

    DWORD bufferSize = static_cast<DWORD>(mHeight * GetStride());

    mFileMapping = ::CreateFileMappingW(
        INVALID_HANDLE_VALUE, nullptr /*secattr*/, PAGE_READWRITE,
        0 /*sizeHigh*/, bufferSize, nullptr /*name*/);
    if (!mFileMapping) {
      return false;
    }

    void* mappedFilePtr =
        ::MapViewOfFile(mFileMapping, FILE_MAP_ALL_ACCESS, 0 /*offsetHigh*/,
                        0 /*offsetLow*/, 0 /*bytesToMap*/);
    if (!mappedFilePtr) {
      return false;
    }

    mPixelData = reinterpret_cast<unsigned char*>(mappedFilePtr);

    return true;
  }

  bool InitializeRemote(int32_t aWidth, int32_t aHeight, HANDLE aFileMapping) {
    MOZ_ASSERT(aWidth > 0);
    MOZ_ASSERT(aHeight > 0);
    MOZ_ASSERT(aFileMapping);

    mWidth = aWidth;
    mHeight = aHeight;
    mFileMapping = aFileMapping;

    void* mappedFilePtr =
        ::MapViewOfFile(mFileMapping, FILE_MAP_ALL_ACCESS, 0 /*offsetHigh*/,
                        0 /*offsetLow*/, 0 /*bytesToMap*/);
    if (!mappedFilePtr) {
      return false;
    }

    mPixelData = reinterpret_cast<unsigned char*>(mappedFilePtr);

    return true;
  }

  HBITMAP CreateDIBSection() {
    BITMAPINFO bitmapInfo = {};
    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = mWidth;
    bitmapInfo.bmiHeader.biHeight = -mHeight;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    void* dummy = nullptr;
    return ::CreateDIBSection(nullptr /*paletteDC*/, &bitmapInfo,
                              DIB_RGB_COLORS, &dummy, mFileMapping,
                              0 /*offset*/);
  }

  HANDLE CreateRemoteFileMapping(HANDLE aTargetProcess) {
    MOZ_ASSERT(aTargetProcess);

    HANDLE fileMapping = nullptr;
    if (!::DuplicateHandle(GetCurrentProcess(), mFileMapping, aTargetProcess,
                           &fileMapping, 0 /*desiredAccess*/,
                           FALSE /*inheritHandle*/, DUPLICATE_SAME_ACCESS)) {
      return nullptr;
    }
    return fileMapping;
  }

  already_AddRefed<gfx::DrawTarget> CreateDrawTarget() {
    return gfx::Factory::CreateDrawTargetForData(
        gfx::BackendType::CAIRO, mPixelData, gfx::IntSize(mWidth, mHeight),
        GetStride(), gfx::SurfaceFormat::B8G8R8A8);
  }

  void CopyPixelsFrom(const SharedImage& other) {
    const unsigned char* src = other.mPixelData;
    unsigned char* dst = mPixelData;

    int32_t width = std::min(mWidth, other.mWidth);
    int32_t height = std::min(mHeight, other.mHeight);

    for (int32_t row = 0; row < height; ++row) {
      memcpy(dst, src, static_cast<uint32_t>(width * kBytesPerPixel));
      src += other.GetStride();
      dst += GetStride();
    }
  }

  int32_t GetWidth() const { return mWidth; }

  int32_t GetHeight() const { return mHeight; }

  SharedImage(const SharedImage&) = delete;
  SharedImage(SharedImage&&) = delete;
  SharedImage& operator=(const SharedImage&) = delete;
  SharedImage& operator=(SharedImage&&) = delete;

 private:
  static constexpr int32_t kBytesPerPixel = 4;

  int32_t GetStride() const {
    // DIB requires 32-bit row alignment
    return (((mWidth * kBytesPerPixel) + 3) / 4) * 4;
  }

  int32_t mWidth;
  int32_t mHeight;
  HANDLE mFileMapping;
  unsigned char* mPixelData;
};

class PresentableSharedImage {
 public:
  PresentableSharedImage()
      : mSharedImage(),
        mDeviceContext(nullptr),
        mDIBSection(nullptr),
        mSavedObject(nullptr) {}

  ~PresentableSharedImage() {
    if (mSavedObject) {
      MOZ_ALWAYS_TRUE(::SelectObject(mDeviceContext, mSavedObject));
    }

    if (mDIBSection) {
      MOZ_ALWAYS_TRUE(::DeleteObject(mDIBSection));
    }

    if (mDeviceContext) {
      MOZ_ALWAYS_TRUE(::DeleteDC(mDeviceContext));
    }
  }

  bool Initialize(int32_t aWidth, int32_t aHeight) {
    if (!mSharedImage.Initialize(aWidth, aHeight)) {
      return false;
    }

    mDeviceContext = ::CreateCompatibleDC(nullptr);
    if (!mDeviceContext) {
      return false;
    }

    mDIBSection = mSharedImage.CreateDIBSection();
    if (!mDIBSection) {
      return false;
    }

    mSavedObject = ::SelectObject(mDeviceContext, mDIBSection);
    if (!mSavedObject) {
      return false;
    }

    return true;
  }

  bool PresentToWindow(HWND aWindowHandle, TransparencyMode aTransparencyMode,
                       Span<const IpcSafeRect> aDirtyRects) {
    if (aTransparencyMode == TransparencyMode::Transparent) {
      // If our window is a child window or a child-of-a-child, the window
      // that needs to be updated is the top level ancestor of the tree
      HWND topLevelWindow = WinUtils::GetTopLevelHWND(aWindowHandle, true);
      MOZ_ASSERT(::GetWindowLongPtr(topLevelWindow, GWL_EXSTYLE) &
                 WS_EX_LAYERED);

      BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
      POINT srcPos = {0, 0};
      RECT clientRect = {};
      if (!::GetClientRect(aWindowHandle, &clientRect)) {
        return false;
      }
      MOZ_ASSERT(clientRect.left == 0);
      MOZ_ASSERT(clientRect.top == 0);
      int32_t width = clientRect.right;
      int32_t height = clientRect.bottom;
      SIZE winSize = {width, height};
      // Window resize could cause the client area to be different than
      // mSharedImage's size. If the client area doesn't match,
      // PresentToWindow() returns false without calling UpdateLayeredWindow().
      // Another call to UpdateLayeredWindow() will follow shortly, since the
      // resize will eventually force the backbuffer to repaint itself again.
      // When client area is larger than mSharedImage's size,
      // UpdateLayeredWindow() draws the window completely invisible. But it
      // does not return false.
      if (width != mSharedImage.GetWidth() ||
          height != mSharedImage.GetHeight()) {
        return false;
      }

      return !!::UpdateLayeredWindow(
          topLevelWindow, nullptr /*paletteDC*/, nullptr /*newPos*/, &winSize,
          mDeviceContext, &srcPos, 0 /*colorKey*/, &bf, ULW_ALPHA);
    }

    gfx::IntRect sharedImageRect{0, 0, mSharedImage.GetWidth(),
                                 mSharedImage.GetHeight()};

    bool result = true;

    HDC windowDC = ::GetDC(aWindowHandle);
    if (!windowDC) {
      return false;
    }

    for (auto& ipcDirtyRect : aDirtyRects) {
      gfx::IntRect dirtyRect{ipcDirtyRect.x, ipcDirtyRect.y, ipcDirtyRect.width,
                             ipcDirtyRect.height};
      gfx::IntRect bltRect = dirtyRect.Intersect(sharedImageRect);

      if (!::BitBlt(windowDC, bltRect.x /*dstX*/, bltRect.y /*dstY*/,
                    bltRect.width, bltRect.height, mDeviceContext,
                    bltRect.x /*srcX*/, bltRect.y /*srcY*/, SRCCOPY)) {
        result = false;
        break;
      }
    }

    MOZ_ALWAYS_TRUE(::ReleaseDC(aWindowHandle, windowDC));

    return result;
  }

  HANDLE CreateRemoteFileMapping(HANDLE aTargetProcess) {
    return mSharedImage.CreateRemoteFileMapping(aTargetProcess);
  }

  already_AddRefed<gfx::DrawTarget> CreateDrawTarget() {
    return mSharedImage.CreateDrawTarget();
  }

  void CopyPixelsFrom(const PresentableSharedImage& other) {
    mSharedImage.CopyPixelsFrom(other.mSharedImage);
  }

  int32_t GetWidth() { return mSharedImage.GetWidth(); }

  int32_t GetHeight() { return mSharedImage.GetHeight(); }

  PresentableSharedImage(const PresentableSharedImage&) = delete;
  PresentableSharedImage(PresentableSharedImage&&) = delete;
  PresentableSharedImage& operator=(const PresentableSharedImage&) = delete;
  PresentableSharedImage& operator=(PresentableSharedImage&&) = delete;

 private:
  SharedImage mSharedImage;
  HDC mDeviceContext;
  HBITMAP mDIBSection;
  HGDIOBJ mSavedObject;
};

Provider::Provider()
    : mWindowHandle(nullptr),
      mTargetProcess(nullptr),
      mFileMapping(nullptr),
      mRequestReadyEvent(nullptr),
      mResponseReadyEvent(nullptr),
      mSharedDataPtr(nullptr),
      mStopServiceThread(false),
      mServiceThread(nullptr),
      mBackbuffer() {}

Provider::~Provider() {
  mBackbuffer.reset();

  if (mServiceThread) {
    mStopServiceThread = true;
    MOZ_ALWAYS_TRUE(::SetEvent(mRequestReadyEvent));
    MOZ_ALWAYS_TRUE(PR_JoinThread(mServiceThread) == PR_SUCCESS);
  }

  if (mSharedDataPtr) {
    MOZ_ALWAYS_TRUE(::UnmapViewOfFile(mSharedDataPtr));
  }

  if (mResponseReadyEvent) {
    MOZ_ALWAYS_TRUE(::CloseHandle(mResponseReadyEvent));
  }

  if (mRequestReadyEvent) {
    MOZ_ALWAYS_TRUE(::CloseHandle(mRequestReadyEvent));
  }

  if (mFileMapping) {
    MOZ_ALWAYS_TRUE(::CloseHandle(mFileMapping));
  }

  if (mTargetProcess) {
    MOZ_ALWAYS_TRUE(::CloseHandle(mTargetProcess));
  }
}

bool Provider::Initialize(HWND aWindowHandle, DWORD aTargetProcessId,
                          TransparencyMode aTransparencyMode) {
  MOZ_ASSERT(aWindowHandle);
  MOZ_ASSERT(aTargetProcessId);

  mWindowHandle = aWindowHandle;

  mTargetProcess = ::OpenProcess(PROCESS_DUP_HANDLE, FALSE /*inheritHandle*/,
                                 aTargetProcessId);
  if (!mTargetProcess) {
    return false;
  }

  mFileMapping = ::CreateFileMappingW(
      INVALID_HANDLE_VALUE, nullptr /*secattr*/, PAGE_READWRITE, 0 /*sizeHigh*/,
      static_cast<DWORD>(sizeof(SharedData)), nullptr /*name*/);
  if (!mFileMapping) {
    return false;
  }

  mRequestReadyEvent =
      ::CreateEventW(nullptr /*secattr*/, FALSE /*manualReset*/,
                     FALSE /*initialState*/, nullptr /*name*/);
  if (!mRequestReadyEvent) {
    return false;
  }

  mResponseReadyEvent =
      ::CreateEventW(nullptr /*secattr*/, FALSE /*manualReset*/,
                     FALSE /*initialState*/, nullptr /*name*/);
  if (!mResponseReadyEvent) {
    return false;
  }

  void* mappedFilePtr =
      ::MapViewOfFile(mFileMapping, FILE_MAP_ALL_ACCESS, 0 /*offsetHigh*/,
                      0 /*offsetLow*/, 0 /*bytesToMap*/);
  if (!mappedFilePtr) {
    return false;
  }

  mSharedDataPtr = reinterpret_cast<SharedData*>(mappedFilePtr);

  mStopServiceThread = false;

  // Use a raw NSPR OS-level thread here instead of nsThread because we are
  // performing low-level synchronization across processes using Win32 Events,
  // and nsThread is designed around an incompatible "in-process task queue"
  // model
  mServiceThread = PR_CreateThread(
      PR_USER_THREAD, [](void* p) { static_cast<Provider*>(p)->ThreadMain(); },
      this, PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD,
      0 /*default stack size*/);
  if (!mServiceThread) {
    return false;
  }

  mTransparencyMode = uint32_t(aTransparencyMode);

  return true;
}

Maybe<RemoteBackbufferHandles> Provider::CreateRemoteHandles() {
  return Some(
      RemoteBackbufferHandles(ipc::FileDescriptor(mFileMapping),
                              ipc::FileDescriptor(mRequestReadyEvent),
                              ipc::FileDescriptor(mResponseReadyEvent)));
}

void Provider::UpdateTransparencyMode(TransparencyMode aTransparencyMode) {
  mTransparencyMode = uint32_t(aTransparencyMode);
}

void Provider::ThreadMain() {
  AUTO_PROFILER_REGISTER_THREAD("RemoteBackbuffer");
  NS_SetCurrentThreadName("RemoteBackbuffer");

  while (true) {
    {
      AUTO_PROFILER_THREAD_SLEEP;
      MOZ_ALWAYS_TRUE(::WaitForSingleObject(mRequestReadyEvent, INFINITE) ==
                      WAIT_OBJECT_0);
    }

    if (mStopServiceThread) {
      break;
    }

    switch (mSharedDataPtr->dataType) {
      case SharedDataType::BorrowRequest:
      case SharedDataType::BorrowRequestAllowSameBuffer: {
        BorrowResponseData responseData = {};

        HandleBorrowRequest(&responseData,
                            mSharedDataPtr->dataType ==
                                SharedDataType::BorrowRequestAllowSameBuffer);

        mSharedDataPtr->dataType = SharedDataType::BorrowResponse;
        mSharedDataPtr->data.borrowResponse = responseData;

        MOZ_ALWAYS_TRUE(::SetEvent(mResponseReadyEvent));

        break;
      }
      case SharedDataType::PresentRequest: {
        PresentRequestData requestData = mSharedDataPtr->data.presentRequest;
        PresentResponseData responseData = {};

        HandlePresentRequest(requestData, &responseData);

        mSharedDataPtr->dataType = SharedDataType::PresentResponse;
        mSharedDataPtr->data.presentResponse = responseData;

        MOZ_ALWAYS_TRUE(::SetEvent(mResponseReadyEvent));

        break;
      }
      default:
        break;
    };
  }
}

void Provider::HandleBorrowRequest(BorrowResponseData* aResponseData,
                                   bool aAllowSameBuffer) {
  MOZ_ASSERT(aResponseData);

  aResponseData->result = ResponseResult::Error;

  RECT clientRect{};
  if (!::GetClientRect(mWindowHandle, &clientRect)) {
    return;
  }

  MOZ_ASSERT(clientRect.left == 0);
  MOZ_ASSERT(clientRect.top == 0);

  const int32_t width = std::max(int32_t(clientRect.right), 1);
  const int32_t height = std::max(int32_t(clientRect.bottom), 1);

  bool needNewBackbuffer = !aAllowSameBuffer || !mBackbuffer ||
                           mBackbuffer->GetWidth() != width ||
                           mBackbuffer->GetHeight() != height;

  if (!needNewBackbuffer) {
    aResponseData->result = ResponseResult::BorrowSameBuffer;
    return;
  }

  auto newBackbuffer = std::make_unique<PresentableSharedImage>();
  if (!newBackbuffer->Initialize(width, height)) {
    return;
  }

  // Preserve the contents of the old backbuffer (if it exists)
  if (mBackbuffer) {
    newBackbuffer->CopyPixelsFrom(*mBackbuffer);
    mBackbuffer.reset();
  }

  HANDLE remoteFileMapping =
      newBackbuffer->CreateRemoteFileMapping(mTargetProcess);
  if (!remoteFileMapping) {
    return;
  }

  aResponseData->result = ResponseResult::BorrowSuccess;
  aResponseData->width = width;
  aResponseData->height = height;
  aResponseData->fileMapping = remoteFileMapping;

  mBackbuffer = std::move(newBackbuffer);
}

void Provider::HandlePresentRequest(const PresentRequestData& aRequestData,
                                    PresentResponseData* aResponseData) {
  MOZ_ASSERT(aResponseData);

  Span rectSpan(aRequestData.dirtyRects, kMaxDirtyRects);

  aResponseData->result = ResponseResult::Error;

  if (!mBackbuffer) {
    return;
  }

  if (!mBackbuffer->PresentToWindow(
          mWindowHandle, GetTransparencyMode(),
          rectSpan.First(aRequestData.lenDirtyRects))) {
    return;
  }

  aResponseData->result = ResponseResult::PresentSuccess;
}

Client::Client()
    : mFileMapping(nullptr),
      mRequestReadyEvent(nullptr),
      mResponseReadyEvent(nullptr),
      mSharedDataPtr(nullptr),
      mBackbuffer() {}

Client::~Client() {
  mBackbuffer.reset();

  if (mSharedDataPtr) {
    MOZ_ALWAYS_TRUE(::UnmapViewOfFile(mSharedDataPtr));
  }

  if (mResponseReadyEvent) {
    MOZ_ALWAYS_TRUE(::CloseHandle(mResponseReadyEvent));
  }

  if (mRequestReadyEvent) {
    MOZ_ALWAYS_TRUE(::CloseHandle(mRequestReadyEvent));
  }

  if (mFileMapping) {
    MOZ_ALWAYS_TRUE(::CloseHandle(mFileMapping));
  }
}

bool Client::Initialize(const RemoteBackbufferHandles& aRemoteHandles) {
  MOZ_ASSERT(aRemoteHandles.fileMapping().IsValid());
  MOZ_ASSERT(aRemoteHandles.requestReadyEvent().IsValid());
  MOZ_ASSERT(aRemoteHandles.responseReadyEvent().IsValid());

  // FIXME: Due to PCompositorWidget using virtual Recv methods,
  // RemoteBackbufferHandles is passed by const reference, and cannot have its
  // signature customized, meaning that we need to clone the handles here.
  //
  // Once PCompositorWidget is migrated to use direct call semantics, the
  // signature can be changed to accept RemoteBackbufferHandles by rvalue
  // reference or value, and the DuplicateHandle calls here can be avoided.
  mFileMapping = aRemoteHandles.fileMapping().ClonePlatformHandle().release();
  mRequestReadyEvent =
      aRemoteHandles.requestReadyEvent().ClonePlatformHandle().release();
  mResponseReadyEvent =
      aRemoteHandles.responseReadyEvent().ClonePlatformHandle().release();

  void* mappedFilePtr =
      ::MapViewOfFile(mFileMapping, FILE_MAP_ALL_ACCESS, 0 /*offsetHigh*/,
                      0 /*offsetLow*/, 0 /*bytesToMap*/);
  if (!mappedFilePtr) {
    return false;
  }

  mSharedDataPtr = reinterpret_cast<SharedData*>(mappedFilePtr);

  return true;
}

already_AddRefed<gfx::DrawTarget> Client::BorrowDrawTarget() {
  mSharedDataPtr->dataType = mBackbuffer
                                 ? SharedDataType::BorrowRequestAllowSameBuffer
                                 : SharedDataType::BorrowRequest;

  MOZ_ALWAYS_TRUE(::SetEvent(mRequestReadyEvent));
  MOZ_ALWAYS_TRUE(::WaitForSingleObject(mResponseReadyEvent, INFINITE) ==
                  WAIT_OBJECT_0);

  if (mSharedDataPtr->dataType != SharedDataType::BorrowResponse) {
    return nullptr;
  }

  BorrowResponseData responseData = mSharedDataPtr->data.borrowResponse;

  if ((responseData.result != ResponseResult::BorrowSameBuffer) &&
      (responseData.result != ResponseResult::BorrowSuccess)) {
    return nullptr;
  }

  if (responseData.result == ResponseResult::BorrowSuccess) {
    mBackbuffer.reset();

    auto newBackbuffer = std::make_unique<SharedImage>();
    if (!newBackbuffer->InitializeRemote(responseData.width,
                                         responseData.height,
                                         responseData.fileMapping)) {
      return nullptr;
    }

    mBackbuffer = std::move(newBackbuffer);
  }

  MOZ_ASSERT(mBackbuffer);

  return mBackbuffer->CreateDrawTarget();
}

bool Client::PresentDrawTarget(gfx::IntRegion aDirtyRegion) {
  mSharedDataPtr->dataType = SharedDataType::PresentRequest;

  // Simplify the region until it has <= kMaxDirtyRects
  aDirtyRegion.SimplifyOutward(kMaxDirtyRects);

  Span rectSpan(mSharedDataPtr->data.presentRequest.dirtyRects, kMaxDirtyRects);

  uint8_t rectIndex = 0;
  for (auto iter = aDirtyRegion.RectIter(); !iter.Done(); iter.Next()) {
    rectSpan[rectIndex] = IpcSafeRect(iter.Get());
    ++rectIndex;
  }

  mSharedDataPtr->data.presentRequest.lenDirtyRects = rectIndex;

  MOZ_ALWAYS_TRUE(::SetEvent(mRequestReadyEvent));
  MOZ_ALWAYS_TRUE(::WaitForSingleObject(mResponseReadyEvent, INFINITE) ==
                  WAIT_OBJECT_0);

  if (mSharedDataPtr->dataType != SharedDataType::PresentResponse) {
    return false;
  }

  if (mSharedDataPtr->data.presentResponse.result !=
      ResponseResult::PresentSuccess) {
    return false;
  }

  return true;
}

}  // namespace remote_backbuffer
}  // namespace widget
}  // namespace mozilla
