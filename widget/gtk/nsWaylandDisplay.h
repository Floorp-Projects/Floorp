/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __MOZ_WAYLAND_DISPLAY_H__
#define __MOZ_WAYLAND_DISPLAY_H__

#include "mozilla/widget/mozwayland.h"
#include "mozilla/widget/gtk-primary-selection-client-protocol.h"
#include "mozilla/widget/idle-inhibit-unstable-v1-client-protocol.h"

#include "base/message_loop.h"  // for MessageLoop
#include "base/task.h"          // for NewRunnableMethod, etc
#include "mozilla/StaticMutex.h"

#include "mozilla/widget/gbm.h"
#include "mozilla/widget/linux-dmabuf-unstable-v1-client-protocol.h"

namespace mozilla {
namespace widget {

struct GbmFormat {
  bool mIsSupported;
  bool mHasAlpha;
  int mFormat;
  uint64_t* mModifiers;
  int mModifiersCount;
};

// Our general connection to Wayland display server,
// holds our display connection and runs event loop.
// We have a global nsWaylandDisplay object for each thread,
// recently we have three for main, compositor and render one.
class nsWaylandDisplay {
 public:
  explicit nsWaylandDisplay(wl_display* aDisplay);
  virtual ~nsWaylandDisplay();

  bool DispatchEventQueue();

  void SyncBegin();
  void SyncEnd();
  void WaitForSyncEnd();

  bool Matches(wl_display* aDisplay);

  wl_display* GetDisplay() { return mDisplay; };
  wl_event_queue* GetEventQueue() { return mEventQueue; };
  wl_compositor* GetCompositor(void) { return mCompositor; };
  wl_subcompositor* GetSubcompositor(void) { return mSubcompositor; };
  wl_data_device_manager* GetDataDeviceManager(void) {
    return mDataDeviceManager;
  };
  wl_seat* GetSeat(void) { return mSeat; };
  wl_shm* GetShm(void) { return mShm; };
  gtk_primary_selection_device_manager* GetPrimarySelectionDeviceManager(void) {
    return mPrimarySelectionDeviceManager;
  };
  zwp_idle_inhibit_manager_v1* GetIdleInhibitManager(void) {
    return mIdleInhibitManager;
  }

  void SetShm(wl_shm* aShm);
  void SetCompositor(wl_compositor* aCompositor);
  void SetSubcompositor(wl_subcompositor* aSubcompositor);
  void SetDataDeviceManager(wl_data_device_manager* aDataDeviceManager);
  void SetSeat(wl_seat* aSeat);
  void SetPrimarySelectionDeviceManager(
      gtk_primary_selection_device_manager* aPrimarySelectionDeviceManager);
  void SetIdleInhibitManager(zwp_idle_inhibit_manager_v1* aIdleInhibitManager);

  MessageLoop* GetThreadLoop() { return mThreadLoop; }
  void ShutdownThreadLoop();

  void SetDmabuf(zwp_linux_dmabuf_v1* aDmabuf);
  zwp_linux_dmabuf_v1* GetDmabuf() { return mDmabuf; };
  gbm_device* GetGbmDevice();
  // Returns -1 if we fails to gbm device file descriptor.
  int GetGbmDeviceFd();
  bool IsExplicitSyncEnabled() { return mExplicitSync; }
  GbmFormat* GetGbmFormat(bool aHasAlpha);
  GbmFormat* GetExactGbmFormat(int aFormat);

  void AddFormat(bool aHasAlpha, int aFormat);
  void AddFormatModifier(bool aHasAlpha, int aFormat, uint32_t mModifierHi,
                         uint32_t mModifierLo);

  static bool IsDMABufEnabled();
  static bool IsDMABufBasicEnabled();
  static bool IsDMABufTexturesEnabled();
  static bool IsDMABufWebGLEnabled();
  static bool IsDMABufVAAPIEnabled();

 private:
  bool ConfigureGbm();

  MessageLoop* mThreadLoop;
  PRThread* mThreadId;
  wl_display* mDisplay;
  wl_event_queue* mEventQueue;
  wl_data_device_manager* mDataDeviceManager;
  wl_compositor* mCompositor;
  wl_subcompositor* mSubcompositor;
  wl_seat* mSeat;
  wl_shm* mShm;
  wl_callback* mSyncCallback;
  gtk_primary_selection_device_manager* mPrimarySelectionDeviceManager;
  zwp_idle_inhibit_manager_v1* mIdleInhibitManager;
  wl_registry* mRegistry;
  zwp_linux_dmabuf_v1* mDmabuf;
  gbm_device* mGbmDevice;
  int mGbmFd;
  GbmFormat mXRGBFormat;
  GbmFormat mARGBFormat;
  bool mGdmConfigured;
  bool mExplicitSync;
  static bool sIsDMABufEnabled;
  static bool sIsDMABufConfigured;
};

void WaylandDispatchDisplays();
void WaylandDisplayShutdown();
nsWaylandDisplay* WaylandDisplayGet(GdkDisplay* aGdkDisplay = nullptr);
wl_display* WaylandDisplayGetWLDisplay(GdkDisplay* aGdkDisplay = nullptr);

typedef struct gbm_device* (*CreateDeviceFunc)(int);
typedef struct gbm_bo* (*CreateFunc)(struct gbm_device*, uint32_t, uint32_t,
                                     uint32_t, uint32_t);
typedef struct gbm_bo* (*CreateWithModifiersFunc)(struct gbm_device*, uint32_t,
                                                  uint32_t, uint32_t,
                                                  const uint64_t*,
                                                  const unsigned int);
typedef uint64_t (*GetModifierFunc)(struct gbm_bo*);
typedef uint32_t (*GetStrideFunc)(struct gbm_bo*);
typedef int (*GetFdFunc)(struct gbm_bo*);
typedef void (*DestroyFunc)(struct gbm_bo*);
typedef void* (*MapFunc)(struct gbm_bo*, uint32_t, uint32_t, uint32_t, uint32_t,
                         uint32_t, uint32_t*, void**);
typedef void (*UnmapFunc)(struct gbm_bo*, void*);
typedef int (*GetPlaneCountFunc)(struct gbm_bo*);
typedef union gbm_bo_handle (*GetHandleForPlaneFunc)(struct gbm_bo*, int);
typedef uint32_t (*GetStrideForPlaneFunc)(struct gbm_bo*, int);
typedef uint32_t (*GetOffsetFunc)(struct gbm_bo*, int);
typedef int (*DeviceIsFormatSupportedFunc)(struct gbm_device*, uint32_t,
                                           uint32_t);
typedef int (*DrmPrimeHandleToFDFunc)(int, uint32_t, uint32_t, int*);

class nsGbmLib {
 public:
  static bool Load();
  static bool IsLoaded();
  static bool IsAvailable();
  static bool IsModifierAvailable();

  static struct gbm_device* CreateDevice(int fd) { return sCreateDevice(fd); };
  static struct gbm_bo* Create(struct gbm_device* gbm, uint32_t width,
                               uint32_t height, uint32_t format,
                               uint32_t flags) {
    return sCreate(gbm, width, height, format, flags);
  }
  static void Destroy(struct gbm_bo* bo) { sDestroy(bo); }
  static uint32_t GetStride(struct gbm_bo* bo) { return sGetStride(bo); }
  static int GetFd(struct gbm_bo* bo) { return sGetFd(bo); }
  static void* Map(struct gbm_bo* bo, uint32_t x, uint32_t y, uint32_t width,
                   uint32_t height, uint32_t flags, uint32_t* stride,
                   void** map_data) {
    return sMap(bo, x, y, width, height, flags, stride, map_data);
  }
  static void Unmap(struct gbm_bo* bo, void* map_data) { sUnmap(bo, map_data); }
  static struct gbm_bo* CreateWithModifiers(struct gbm_device* gbm,
                                            uint32_t width, uint32_t height,
                                            uint32_t format,
                                            const uint64_t* modifiers,
                                            const unsigned int count) {
    return sCreateWithModifiers(gbm, width, height, format, modifiers, count);
  }
  static uint64_t GetModifier(struct gbm_bo* bo) { return sGetModifier(bo); }
  static int GetPlaneCount(struct gbm_bo* bo) { return sGetPlaneCount(bo); }
  static union gbm_bo_handle GetHandleForPlane(struct gbm_bo* bo, int plane) {
    return sGetHandleForPlane(bo, plane);
  }
  static uint32_t GetStrideForPlane(struct gbm_bo* bo, int plane) {
    return sGetStrideForPlane(bo, plane);
  }
  static uint32_t GetOffset(struct gbm_bo* bo, int plane) {
    return sGetOffset(bo, plane);
  }
  static int DeviceIsFormatSupported(struct gbm_device* gbm, uint32_t format,
                                     uint32_t usage) {
    return sDeviceIsFormatSupported(gbm, format, usage);
  }

  static int DrmPrimeHandleToFD(int fd, uint32_t handle, uint32_t flags,
                                int* prime_fd) {
    return sDrmPrimeHandleToFD(fd, handle, flags, prime_fd);
  }

 private:
  static CreateDeviceFunc sCreateDevice;
  static CreateFunc sCreate;
  static CreateWithModifiersFunc sCreateWithModifiers;
  static GetModifierFunc sGetModifier;
  static GetStrideFunc sGetStride;
  static GetFdFunc sGetFd;
  static DestroyFunc sDestroy;
  static MapFunc sMap;
  static UnmapFunc sUnmap;
  static GetPlaneCountFunc sGetPlaneCount;
  static GetHandleForPlaneFunc sGetHandleForPlane;
  static GetStrideForPlaneFunc sGetStrideForPlane;
  static GetOffsetFunc sGetOffset;
  static DeviceIsFormatSupportedFunc sDeviceIsFormatSupported;
  static DrmPrimeHandleToFDFunc sDrmPrimeHandleToFD;

  static void* sGbmLibHandle;
  static void* sXf86DrmLibHandle;
  static bool sLibLoaded;
};

}  // namespace widget
}  // namespace mozilla

#endif  // __MOZ_WAYLAND_DISPLAY_H__
