/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYCDMUTILS_H
#define DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYCDMUTILS_H

#include <initguid.h>
#include <memory>
#include <mfidl.h>
#include <mutex>
#include <thread>
#include <windows.h>
#include <wrl.h>
#include <sstream>
#include <stdio.h>

#include "MFCDMExtra.h"
#include "mozilla/Assertions.h"
#include "mozilla/Unused.h"

namespace mozilla {

inline constexpr WCHAR kCLEARKEY_SYSTEM_NAME[] = L"org.w3.clearkey";

#define WMF_CLEARKEY_DEBUG 0

#ifdef WMF_CLEARKEY_DEBUG
#  define LOG(msg, ...)                                                   \
    printf(("[Thread %lu]: D/WMFClearKey " msg "\n"),                     \
           static_cast<unsigned long>(                                    \
               std::hash<std::thread::id>{}(std::this_thread::get_id())), \
           ##__VA_ARGS__)
#else
#  define LOG(msg, ...)
#endif

#define PRETTY_FUNC                                    \
  ([&]() -> std::string {                              \
    std::string prettyFunction(__PRETTY_FUNCTION__);   \
    std::size_t pos1 = prettyFunction.find("::");      \
    std::size_t pos2 = prettyFunction.find("(", pos1); \
    return prettyFunction.substr(0, pos2);             \
  })()                                                 \
      .c_str()

// We can't reuse the definition in the `MediaEngineUtils.h` due to the
// restriction of not being able to use XPCOM string in the external library.
#ifndef RETURN_IF_FAILED
#  define RETURN_IF_FAILED(x)               \
    do {                                    \
      HRESULT rv = x;                       \
      if (FAILED(rv)) {                     \
        LOG("(" #x ") failed, rv=%lx", rv); \
        return rv;                          \
      }                                     \
    } while (false)
#endif

#ifndef NOT_IMPLEMENTED
#  define NOT_IMPLEMENTED()                                \
    do {                                                   \
      LOG("WARNING : '%s' NOT IMPLEMENTED!", PRETTY_FUNC); \
    } while (0)
#endif

#ifndef ENTRY_LOG
#  define ENTRY_LOG(...)                 \
    do {                                 \
      LOG("%s [%p]", PRETTY_FUNC, this); \
    } while (0)
#  define ENTRY_LOG_ARGS(fmt, ...)                            \
    do {                                                      \
      LOG("%s [%p]: " fmt, PRETTY_FUNC, this, ##__VA_ARGS__); \
      (void)(0, ##__VA_ARGS__);                               \
    } while (0)
#endif

// TODO : should we use Microsoft's definition or Chromium's defintion?

// This is defined in Microsoft's sample code
// https://github.com/microsoft/media-foundation/blob/dc81175a3e893c7c58fcbf1a943ac342e39f172c/samples/storecdm/clearkeyStoreCDM/clearkeydll/guids.h#L17-L18C14
// DEFINE_GUID(CLEARKEY_GUID_CLEARKEY_PROTECTION_SYSTEM_ID, 0x9b0ff3ca, 0x1378,
//             0x4f28, 0x96, 0x65, 0x18, 0x9e, 0x15, 0x30, 0x2a, 0x71);

// Media Foundation Clear Key protection system ID from Chromium
// {E4E94971-696A-447E-96E4-93FDF3A57A7A}
// https://source.chromium.org/chromium/chromium/src/+/main:media/cdm/win/test/media_foundation_clear_key_guids.h;l=16-29?q=CLEARKEY_GUID_CLEARKEY_PROTECTION_SYSTEM_ID&ss=chromium%2Fchromium%2Fsrc
DEFINE_GUID(CLEARKEY_GUID_CLEARKEY_PROTECTION_SYSTEM_ID, 0xe4e94971, 0x696a,
            0x447e, 0x96, 0xe4, 0x93, 0xfd, 0xf3, 0xa5, 0x7a, 0x7a);

// Media Foundation Clear Key content enabler type
// {C262FD73-2F13-41C2-94E7-4CAF087AE1D1}
DEFINE_GUID(MEDIA_FOUNDATION_CLEARKEY_GUID_CONTENT_ENABLER_TYPE, 0xc262fd73,
            0x2f13, 0x41c2, 0x94, 0xe7, 0x4c, 0xaf, 0x8, 0x7a, 0xe1, 0xd1);

// https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:media/cdm/win/test/media_foundation_clear_key_guids.h;l=46;drc=fcefb4563e8ea5b2036ecde882f4f228dd1e4338
#define PLAYREADY_GUID_MEDIA_PROTECTION_SYSTEM_ID_STRING \
  L"{F4637010-03C3-42CD-B932-B48ADF3A6A54}"

// A PROPVARIANT that is automatically initialized and cleared upon respective
// construction and destruction of this class.
class AutoPropVar final {
 public:
  AutoPropVar() { PropVariantInit(&mVar); }

  ~AutoPropVar() { Reset(); }

  AutoPropVar(const AutoPropVar&) = delete;
  AutoPropVar& operator=(const AutoPropVar&) = delete;
  bool operator==(const AutoPropVar&) const = delete;
  bool operator!=(const AutoPropVar&) const = delete;

  // Returns a pointer to the underlying PROPVARIANT for use as an out param
  // in a function call.
  PROPVARIANT* Receive() {
    MOZ_ASSERT(mVar.vt == VT_EMPTY);
    return &mVar;
  }

  // Clears the instance to prepare it for re-use (e.g., via Receive).
  void Reset() {
    if (mVar.vt != VT_EMPTY) {
      HRESULT hr = PropVariantClear(&mVar);
      MOZ_ASSERT(SUCCEEDED(hr));
      Unused << hr;
    }
  }

  const PROPVARIANT& get() const { return mVar; }
  const PROPVARIANT* ptr() const { return &mVar; }

 private:
  PROPVARIANT mVar;
};

template <typename T>
class ScopedCoMem final {
 public:
  ScopedCoMem() : mPtr(nullptr) {}

  ~ScopedCoMem() { Reset(nullptr); }

  ScopedCoMem(const ScopedCoMem&) = delete;
  ScopedCoMem& operator=(const ScopedCoMem&) = delete;

  T** operator&() {               // NOLINT
    MOZ_ASSERT(mPtr == nullptr);  // To catch memory leaks.
    return &mPtr;
  }

  operator T*() { return mPtr; }

  T* operator->() {
    MOZ_ASSERT(mPtr != nullptr);
    return mPtr;
  }

  const T* operator->() const {
    MOZ_ASSERT(mPtr != nullptr);
    return mPtr;
  }

  explicit operator bool() const { return mPtr; }

  friend bool operator==(const ScopedCoMem& lhs, std::nullptr_t) {
    return lhs.Get() == nullptr;
  }

  friend bool operator==(std::nullptr_t, const ScopedCoMem& rhs) {
    return rhs.Get() == nullptr;
  }

  friend bool operator!=(const ScopedCoMem& lhs, std::nullptr_t) {
    return lhs.Get() != nullptr;
  }

  friend bool operator!=(std::nullptr_t, const ScopedCoMem& rhs) {
    return rhs.Get() != nullptr;
  }

  void Reset(T* ptr) {
    if (mPtr) {
      CoTaskMemFree(mPtr);
    }
    mPtr = ptr;
  }

  T* Get() const { return mPtr; }

 private:
  T* mPtr;
};

template <typename T>
class DataMutex final {
 public:
  DataMutex() = default;

  template <typename... Args>
  explicit DataMutex(Args&&... args) : mData(std::forward<Args>(args)...) {}

  class AutoLock {
   public:
    AutoLock(std::unique_lock<std::mutex>&& lock, DataMutex<T>* mutex)
        : mLock(std::move(lock)), mMutex(mutex) {}
    T& operator*() { return ref(); }
    T* operator->() { return &ref(); }
    T& ref() const& { return mMutex->mData; }
    operator T*() const& { return &ref(); }
    ~AutoLock() { mMutex = nullptr; }

   private:
    std::unique_lock<std::mutex> mLock;
    DataMutex<T>* mMutex;
  };

  AutoLock Lock() {
    return AutoLock(std::unique_lock<std::mutex>(mMutex), this);
  }

 private:
  std::mutex mMutex;
  T mData;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_CLEARKEY_WMFCLEARKEYCDMUTILS_H
