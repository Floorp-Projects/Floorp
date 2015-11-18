/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/screen_capturer.h"

#include <stddef.h>
#include <set>

#include <ApplicationServices/ApplicationServices.h>
#include <Cocoa/Cocoa.h>
#include <dlfcn.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <OpenGL/CGLMacro.h>
#include <OpenGL/OpenGL.h>

#include "webrtc/base/macutils.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/desktop_geometry.h"
#include "webrtc/modules/desktop_capture/desktop_region.h"
#include "webrtc/modules/desktop_capture/mac/desktop_configuration.h"
#include "webrtc/modules/desktop_capture/mac/desktop_configuration_monitor.h"
#include "webrtc/modules/desktop_capture/mac/scoped_pixel_buffer_object.h"
#include "webrtc/modules/desktop_capture/screen_capture_frame_queue.h"
#include "webrtc/modules/desktop_capture/screen_capturer_helper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/tick_util.h"

namespace webrtc {

namespace {

// Definitions used to dynamic-link to deprecated OS 10.6 functions.
const char* kApplicationServicesLibraryName =
    "/System/Library/Frameworks/ApplicationServices.framework/"
    "ApplicationServices";
typedef void* (*CGDisplayBaseAddressFunc)(CGDirectDisplayID);
typedef size_t (*CGDisplayBytesPerRowFunc)(CGDirectDisplayID);
typedef size_t (*CGDisplayBitsPerPixelFunc)(CGDirectDisplayID);
const char* kOpenGlLibraryName =
    "/System/Library/Frameworks/OpenGL.framework/OpenGL";
typedef CGLError (*CGLSetFullScreenFunc)(CGLContextObj);

// Standard Mac displays have 72dpi, but we report 96dpi for
// consistency with Windows and Linux.
const int kStandardDPI = 96;

// Scales all coordinates of a rect by a specified factor.
DesktopRect ScaleAndRoundCGRect(const CGRect& rect, float scale) {
  return DesktopRect::MakeLTRB(
    static_cast<int>(floor(rect.origin.x * scale)),
    static_cast<int>(floor(rect.origin.y * scale)),
    static_cast<int>(ceil((rect.origin.x + rect.size.width) * scale)),
    static_cast<int>(ceil((rect.origin.y + rect.size.height) * scale)));
}

// Copy pixels in the |rect| from |src_place| to |dest_plane|. |rect| should be
// relative to the origin of |src_plane| and |dest_plane|.
void CopyRect(const uint8_t* src_plane,
              int src_plane_stride,
              uint8_t* dest_plane,
              int dest_plane_stride,
              int bytes_per_pixel,
              const DesktopRect& rect) {
  // Get the address of the starting point.
  const int src_y_offset = src_plane_stride * rect.top();
  const int dest_y_offset = dest_plane_stride * rect.top();
  const int x_offset = bytes_per_pixel * rect.left();
  src_plane += src_y_offset + x_offset;
  dest_plane += dest_y_offset + x_offset;

  // Copy pixels in the rectangle line by line.
  const int bytes_per_line = bytes_per_pixel * rect.width();
  const int height = rect.height();
  for (int i = 0 ; i < height; ++i) {
    memcpy(dest_plane, src_plane, bytes_per_line);
    src_plane += src_plane_stride;
    dest_plane += dest_plane_stride;
  }
}

// Returns an array of CGWindowID for all the on-screen windows except
// |window_to_exclude|, or NULL if the window is not found or it fails. The
// caller should release the returned CFArrayRef.
CFArrayRef CreateWindowListWithExclusion(CGWindowID window_to_exclude) {
  if (!window_to_exclude)
    return NULL;

  CFArrayRef all_windows = CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
  if (!all_windows)
    return NULL;

  CFMutableArrayRef returned_array = CFArrayCreateMutable(
      NULL, CFArrayGetCount(all_windows), NULL);

  bool found = false;
  for (CFIndex i = 0; i < CFArrayGetCount(all_windows); ++i) {
    CFDictionaryRef window = reinterpret_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(all_windows, i));

    CFNumberRef id_ref = reinterpret_cast<CFNumberRef>(
        CFDictionaryGetValue(window, kCGWindowNumber));

    CGWindowID id;
    CFNumberGetValue(id_ref, kCFNumberIntType, &id);
    if (id == window_to_exclude) {
      found = true;
      continue;
    }
    CFArrayAppendValue(returned_array, reinterpret_cast<void *>(id));
  }
  CFRelease(all_windows);

  if (!found) {
    CFRelease(returned_array);
    returned_array = NULL;
  }
  return returned_array;
}

// Returns the bounds of |window| in physical pixels, enlarged by a small amount
// on four edges to take account of the border/shadow effects.
DesktopRect GetExcludedWindowPixelBounds(CGWindowID window,
                                         float dip_to_pixel_scale) {
  // The amount of pixels to add to the actual window bounds to take into
  // account of the border/shadow effects.
  static const int kBorderEffectSize = 20;
  CGRect rect;
  CGWindowID ids[1];
  ids[0] = window;

  CFArrayRef window_id_array =
      CFArrayCreate(NULL, reinterpret_cast<const void **>(&ids), 1, NULL);
  CFArrayRef window_array =
      CGWindowListCreateDescriptionFromArray(window_id_array);

  if (CFArrayGetCount(window_array) > 0) {
    CFDictionaryRef window = reinterpret_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(window_array, 0));
    CFDictionaryRef bounds_ref = reinterpret_cast<CFDictionaryRef>(
        CFDictionaryGetValue(window, kCGWindowBounds));
    CGRectMakeWithDictionaryRepresentation(bounds_ref, &rect);
  }

  CFRelease(window_id_array);
  CFRelease(window_array);

  rect.origin.x -= kBorderEffectSize;
  rect.origin.y -= kBorderEffectSize;
  rect.size.width += kBorderEffectSize * 2;
  rect.size.height += kBorderEffectSize * 2;
  // |rect| is in DIP, so convert to physical pixels.
  return ScaleAndRoundCGRect(rect, dip_to_pixel_scale);
}

// Create an image of the given region using the given |window_list|.
// |pixel_bounds| should be in the primary display's coordinate in physical
// pixels. The caller should release the returned CGImageRef and CFDataRef.
CGImageRef CreateExcludedWindowRegionImage(const DesktopRect& pixel_bounds,
                                           float dip_to_pixel_scale,
                                           CFArrayRef window_list,
                                           CFDataRef* data_ref) {
  CGRect window_bounds;
  // The origin is in DIP while the size is in physical pixels. That's what
  // CGWindowListCreateImageFromArray expects.
  window_bounds.origin.x = pixel_bounds.left() / dip_to_pixel_scale;
  window_bounds.origin.y = pixel_bounds.top() / dip_to_pixel_scale;
  window_bounds.size.width = pixel_bounds.width();
  window_bounds.size.height = pixel_bounds.height();

  CGImageRef excluded_image = CGWindowListCreateImageFromArray(
      window_bounds, window_list, kCGWindowImageDefault);

  CGDataProviderRef provider = CGImageGetDataProvider(excluded_image);
  *data_ref = CGDataProviderCopyData(provider);
  assert(*data_ref);
  return excluded_image;
}

// A class to perform video frame capturing for mac.
class ScreenCapturerMac : public ScreenCapturer {
 public:
  explicit ScreenCapturerMac(
      scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor);
  virtual ~ScreenCapturerMac();

  bool Init();

  // Overridden from ScreenCapturer:
  void Start(Callback* callback) override;
  void Capture(const DesktopRegion& region) override;
  void SetExcludedWindow(WindowId window) override;
  bool GetScreenList(ScreenList* screens) override;
  bool SelectScreen(ScreenId id) override;

 private:
  void GlBlitFast(const DesktopFrame& frame,
                  const DesktopRegion& region);
  void GlBlitSlow(const DesktopFrame& frame);
  void CgBlitPreLion(const DesktopFrame& frame,
                     const DesktopRegion& region);
  // Returns false if the selected screen is no longer valid.
  bool CgBlitPostLion(const DesktopFrame& frame,
                      const DesktopRegion& region);

  // Called when the screen configuration is changed.
  void ScreenConfigurationChanged();

  bool RegisterRefreshAndMoveHandlers();
  void UnregisterRefreshAndMoveHandlers();

  void ScreenRefresh(CGRectCount count, const CGRect *rect_array);
  void ScreenUpdateMove(CGScreenUpdateMoveDelta delta,
                        size_t count,
                        const CGRect *rect_array);
  static void ScreenRefreshCallback(CGRectCount count,
                                    const CGRect *rect_array,
                                    void *user_parameter);
  static void ScreenUpdateMoveCallback(CGScreenUpdateMoveDelta delta,
                                       size_t count,
                                       const CGRect *rect_array,
                                       void *user_parameter);
  void ReleaseBuffers();

  DesktopFrame* CreateFrame();

  Callback* callback_;

  CGLContextObj cgl_context_;
  ScopedPixelBufferObject pixel_buffer_object_;

  // Queue of the frames buffers.
  ScreenCaptureFrameQueue queue_;

  // Current display configuration.
  MacDesktopConfiguration desktop_config_;

  // Currently selected display, or 0 if the full desktop is selected. On OS X
  // 10.6 and before, this is always 0.
  CGDirectDisplayID current_display_;

  // The physical pixel bounds of the current screen.
  DesktopRect screen_pixel_bounds_;

  // The dip to physical pixel scale of the current screen.
  float dip_to_pixel_scale_;

  // A thread-safe list of invalid rectangles, and the size of the most
  // recently captured screen.
  ScreenCapturerHelper helper_;

  // Contains an invalid region from the previous capture.
  DesktopRegion last_invalid_region_;

  // Monitoring display reconfiguration.
  scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor_;

  // Power management assertion to prevent the screen from sleeping.
  IOPMAssertionID power_assertion_id_display_;

  // Power management assertion to indicate that the user is active.
  IOPMAssertionID power_assertion_id_user_;

  // Dynamically link to deprecated APIs for Mac OS X 10.6 support.
  void* app_services_library_;
  CGDisplayBaseAddressFunc cg_display_base_address_;
  CGDisplayBytesPerRowFunc cg_display_bytes_per_row_;
  CGDisplayBitsPerPixelFunc cg_display_bits_per_pixel_;
  void* opengl_library_;
  CGLSetFullScreenFunc cgl_set_full_screen_;

  CGWindowID excluded_window_;

  DISALLOW_COPY_AND_ASSIGN(ScreenCapturerMac);
};

// DesktopFrame wrapper that flips wrapped frame upside down by inverting
// stride.
class InvertedDesktopFrame : public DesktopFrame {
 public:
  // Takes ownership of |frame|.
  InvertedDesktopFrame(DesktopFrame* frame)
      : DesktopFrame(
            frame->size(), -frame->stride(),
            frame->data() + (frame->size().height() - 1) * frame->stride(),
            frame->shared_memory()),
        original_frame_(frame) {
    set_dpi(frame->dpi());
    set_capture_time_ms(frame->capture_time_ms());
    mutable_updated_region()->Swap(frame->mutable_updated_region());
  }
  virtual ~InvertedDesktopFrame() {}

 private:
  rtc::scoped_ptr<DesktopFrame> original_frame_;

  DISALLOW_COPY_AND_ASSIGN(InvertedDesktopFrame);
};

ScreenCapturerMac::ScreenCapturerMac(
    scoped_refptr<DesktopConfigurationMonitor> desktop_config_monitor)
    : callback_(NULL),
      cgl_context_(NULL),
      current_display_(0),
      dip_to_pixel_scale_(1.0f),
      desktop_config_monitor_(desktop_config_monitor),
      power_assertion_id_display_(kIOPMNullAssertionID),
      power_assertion_id_user_(kIOPMNullAssertionID),
      app_services_library_(NULL),
      cg_display_base_address_(NULL),
      cg_display_bytes_per_row_(NULL),
      cg_display_bits_per_pixel_(NULL),
      opengl_library_(NULL),
      cgl_set_full_screen_(NULL),
      excluded_window_(0) {
}

ScreenCapturerMac::~ScreenCapturerMac() {
  if (power_assertion_id_display_ != kIOPMNullAssertionID) {
    IOPMAssertionRelease(power_assertion_id_display_);
    power_assertion_id_display_ = kIOPMNullAssertionID;
  }
  if (power_assertion_id_user_ != kIOPMNullAssertionID) {
    IOPMAssertionRelease(power_assertion_id_user_);
    power_assertion_id_user_ = kIOPMNullAssertionID;
  }

  ReleaseBuffers();
  UnregisterRefreshAndMoveHandlers();
  dlclose(app_services_library_);
  dlclose(opengl_library_);
}

bool ScreenCapturerMac::Init() {
  if (!RegisterRefreshAndMoveHandlers()) {
    return false;
  }
  desktop_config_monitor_->Lock();
  desktop_config_ = desktop_config_monitor_->desktop_configuration();
  desktop_config_monitor_->Unlock();
  ScreenConfigurationChanged();
  return true;
}

void ScreenCapturerMac::ReleaseBuffers() {
  if (cgl_context_) {
    pixel_buffer_object_.Release();
    CGLDestroyContext(cgl_context_);
    cgl_context_ = NULL;
  }
  // The buffers might be in use by the encoder, so don't delete them here.
  // Instead, mark them as "needs update"; next time the buffers are used by
  // the capturer, they will be recreated if necessary.
  queue_.Reset();
}

void ScreenCapturerMac::Start(Callback* callback) {
  assert(!callback_);
  assert(callback);

  callback_ = callback;

  // Create power management assertions to wake the display and prevent it from
  // going to sleep on user idle.
  // TODO(jamiewalch): Use IOPMAssertionDeclareUserActivity on 10.7.3 and above
  //                   instead of the following two assertions.
  IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep,
                              kIOPMAssertionLevelOn,
                              CFSTR("Chrome Remote Desktop connection active"),
                              &power_assertion_id_display_);
  // This assertion ensures that the display is woken up if it  already asleep
  // (as used by Apple Remote Desktop).
  IOPMAssertionCreateWithName(CFSTR("UserIsActive"),
                              kIOPMAssertionLevelOn,
                              CFSTR("Chrome Remote Desktop connection active"),
                              &power_assertion_id_user_);
}

void ScreenCapturerMac::Capture(const DesktopRegion& region_to_capture) {
  TickTime capture_start_time = TickTime::Now();

  queue_.MoveToNextFrame();

  desktop_config_monitor_->Lock();
  MacDesktopConfiguration new_config =
      desktop_config_monitor_->desktop_configuration();
  if (!desktop_config_.Equals(new_config)) {
    desktop_config_ = new_config;
    // If the display configuraiton has changed then refresh capturer data
    // structures. Occasionally, the refresh and move handlers are lost when
    // the screen mode changes, so re-register them here.
    UnregisterRefreshAndMoveHandlers();
    RegisterRefreshAndMoveHandlers();
    ScreenConfigurationChanged();
  }

  DesktopRegion region;
  helper_.TakeInvalidRegion(&region);

  // If the current buffer is from an older generation then allocate a new one.
  // Note that we can't reallocate other buffers at this point, since the caller
  // may still be reading from them.
  if (!queue_.current_frame())
    queue_.ReplaceCurrentFrame(CreateFrame());

  DesktopFrame* current_frame = queue_.current_frame();

  bool flip = false;  // GL capturers need flipping.
  if (rtc::GetOSVersionName() >= rtc::kMacOSLion) {
    // Lion requires us to use their new APIs for doing screen capture. These
    // APIS currently crash on 10.6.8 if there is no monitor attached.
    if (!CgBlitPostLion(*current_frame, region)) {
      desktop_config_monitor_->Unlock();
      callback_->OnCaptureCompleted(NULL);
      return;
    }
  } else if (cgl_context_) {
    flip = true;
    if (pixel_buffer_object_.get() != 0) {
      GlBlitFast(*current_frame, region);
    } else {
      // See comment in ScopedPixelBufferObject::Init about why the slow
      // path is always used on 10.5.
      GlBlitSlow(*current_frame);
    }
  } else {
    CgBlitPreLion(*current_frame, region);
  }

  DesktopFrame* new_frame = queue_.current_frame()->Share();
  *new_frame->mutable_updated_region() = region;

  if (flip)
    new_frame = new InvertedDesktopFrame(new_frame);

  helper_.set_size_most_recent(new_frame->size());

  // Signal that we are done capturing data from the display framebuffer,
  // and accessing display structures.
  desktop_config_monitor_->Unlock();

  new_frame->set_capture_time_ms(
      (TickTime::Now() - capture_start_time).Milliseconds());
  callback_->OnCaptureCompleted(new_frame);
}

void ScreenCapturerMac::SetExcludedWindow(WindowId window) {
  excluded_window_ = window;
}

bool ScreenCapturerMac::GetScreenList(ScreenList* screens) {
  assert(screens->size() == 0);
  if (rtc::GetOSVersionName() < rtc::kMacOSLion) {
    // Single monitor cast is not supported on pre OS X 10.7.
    Screen screen;
    screen.id = kFullDesktopScreenId;
    screens->push_back(screen);
    return true;
  }

  for (MacDisplayConfigurations::iterator it = desktop_config_.displays.begin();
       it != desktop_config_.displays.end(); ++it) {
    Screen screen;
    screen.id = static_cast<ScreenId>(it->id);
    screens->push_back(screen);
  }
  return true;
}

bool ScreenCapturerMac::SelectScreen(ScreenId id) {
  if (rtc::GetOSVersionName() < rtc::kMacOSLion) {
    // Ignore the screen selection on unsupported OS.
    assert(!current_display_);
    return id == kFullDesktopScreenId;
  }

  if (id == kFullDesktopScreenId) {
    current_display_ = 0;
  } else {
    const MacDisplayConfiguration* config =
        desktop_config_.FindDisplayConfigurationById(
            static_cast<CGDirectDisplayID>(id));
    if (!config)
      return false;
    current_display_ = config->id;
  }

  ScreenConfigurationChanged();
  return true;
}

void ScreenCapturerMac::GlBlitFast(const DesktopFrame& frame,
                                   const DesktopRegion& region) {
  // Clip to the size of our current screen.
  DesktopRect clip_rect = DesktopRect::MakeSize(frame.size());
  if (queue_.previous_frame()) {
    // We are doing double buffer for the capture data so we just need to copy
    // the invalid region from the previous capture in the current buffer.
    // TODO(hclam): We can reduce the amount of copying here by subtracting
    // |capturer_helper_|s region from |last_invalid_region_|.
    // http://crbug.com/92354

    // Since the image obtained from OpenGL is upside-down, need to do some
    // magic here to copy the correct rectangle.
    const int y_offset = (frame.size().height() - 1) * frame.stride();
    for (DesktopRegion::Iterator i(last_invalid_region_);
         !i.IsAtEnd(); i.Advance()) {
      DesktopRect copy_rect = i.rect();
      copy_rect.IntersectWith(clip_rect);
      if (!copy_rect.is_empty()) {
        CopyRect(queue_.previous_frame()->data() + y_offset,
                 -frame.stride(),
                 frame.data() + y_offset,
                 -frame.stride(),
                 DesktopFrame::kBytesPerPixel,
                 copy_rect);
      }
    }
  }
  last_invalid_region_ = region;

  CGLContextObj CGL_MACRO_CONTEXT = cgl_context_;
  glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pixel_buffer_object_.get());
  glReadPixels(0, 0, frame.size().width(), frame.size().height(), GL_BGRA,
               GL_UNSIGNED_BYTE, 0);
  GLubyte* ptr = static_cast<GLubyte*>(
      glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB));
  if (ptr == NULL) {
    // If the buffer can't be mapped, assume that it's no longer valid and
    // release it.
    pixel_buffer_object_.Release();
  } else {
    // Copy only from the dirty rects. Since the image obtained from OpenGL is
    // upside-down we need to do some magic here to copy the correct rectangle.
    const int y_offset = (frame.size().height() - 1) * frame.stride();
    for (DesktopRegion::Iterator i(region);
         !i.IsAtEnd(); i.Advance()) {
      DesktopRect copy_rect = i.rect();
      copy_rect.IntersectWith(clip_rect);
      if (!copy_rect.is_empty()) {
        CopyRect(ptr + y_offset,
                 -frame.stride(),
                 frame.data() + y_offset,
                 -frame.stride(),
                 DesktopFrame::kBytesPerPixel,
                 copy_rect);
      }
    }
  }
  if (!glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB)) {
    // If glUnmapBuffer returns false, then the contents of the data store are
    // undefined. This might be because the screen mode has changed, in which
    // case it will be recreated in ScreenConfigurationChanged, but releasing
    // the object here is the best option. Capturing will fall back on
    // GlBlitSlow until such time as the pixel buffer object is recreated.
    pixel_buffer_object_.Release();
  }
  glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
}

void ScreenCapturerMac::GlBlitSlow(const DesktopFrame& frame) {
  CGLContextObj CGL_MACRO_CONTEXT = cgl_context_;
  glReadBuffer(GL_FRONT);
  glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
  glPixelStorei(GL_PACK_ALIGNMENT, 4);  // Force 4-byte alignment.
  glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  glPixelStorei(GL_PACK_SKIP_ROWS, 0);
  glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
  // Read a block of pixels from the frame buffer.
  glReadPixels(0, 0, frame.size().width(), frame.size().height(),
               GL_BGRA, GL_UNSIGNED_BYTE, frame.data());
  glPopClientAttrib();
}

void ScreenCapturerMac::CgBlitPreLion(const DesktopFrame& frame,
                                      const DesktopRegion& region) {
  // Copy the entire contents of the previous capture buffer, to capture over.
  // TODO(wez): Get rid of this as per crbug.com/145064, or implement
  // crbug.com/92354.
  if (queue_.previous_frame()) {
    memcpy(frame.data(),
           queue_.previous_frame()->data(),
           frame.stride() * frame.size().height());
  }

  for (size_t i = 0; i < desktop_config_.displays.size(); ++i) {
    const MacDisplayConfiguration& display_config = desktop_config_.displays[i];

    // Use deprecated APIs to determine the display buffer layout.
    assert(cg_display_base_address_ && cg_display_bytes_per_row_ &&
        cg_display_bits_per_pixel_);
    uint8_t* display_base_address = reinterpret_cast<uint8_t*>(
        (*cg_display_base_address_)(display_config.id));
    assert(display_base_address);
    int src_bytes_per_row = (*cg_display_bytes_per_row_)(display_config.id);
    int src_bytes_per_pixel =
        (*cg_display_bits_per_pixel_)(display_config.id) / 8;

    // Determine the display's position relative to the desktop, in pixels.
    DesktopRect display_bounds = display_config.pixel_bounds;
    display_bounds.Translate(-desktop_config_.pixel_bounds.left(),
                             -desktop_config_.pixel_bounds.top());

    // Determine which parts of the blit region, if any, lay within the monitor.
    DesktopRegion copy_region = region;
    copy_region.IntersectWith(display_bounds);
    if (copy_region.is_empty())
      continue;

    // Translate the region to be copied into display-relative coordinates.
    copy_region.Translate(-display_bounds.left(), -display_bounds.top());

    // Calculate where in the output buffer the display's origin is.
    uint8_t* out_ptr = frame.data() +
         (display_bounds.left() * src_bytes_per_pixel) +
         (display_bounds.top() * frame.stride());

    // Copy the dirty region from the display buffer into our desktop buffer.
    for (DesktopRegion::Iterator i(copy_region); !i.IsAtEnd(); i.Advance()) {
      CopyRect(display_base_address,
               src_bytes_per_row,
               out_ptr,
               frame.stride(),
               src_bytes_per_pixel,
               i.rect());
    }
  }
}

bool ScreenCapturerMac::CgBlitPostLion(const DesktopFrame& frame,
                                       const DesktopRegion& region) {
  // Copy the entire contents of the previous capture buffer, to capture over.
  // TODO(wez): Get rid of this as per crbug.com/145064, or implement
  // crbug.com/92354.
  if (queue_.previous_frame()) {
    memcpy(frame.data(),
           queue_.previous_frame()->data(),
           frame.stride() * frame.size().height());
  }

  MacDisplayConfigurations displays_to_capture;
  if (current_display_) {
    // Capturing a single screen. Note that the screen id may change when
    // screens are added or removed.
    const MacDisplayConfiguration* config =
        desktop_config_.FindDisplayConfigurationById(current_display_);
    if (config) {
      displays_to_capture.push_back(*config);
    } else {
      LOG(LS_ERROR) << "The selected screen cannot be found for capturing.";
      return false;
    }
  } else {
    // Capturing the whole desktop.
    displays_to_capture = desktop_config_.displays;
  }

  // Create the window list once for all displays.
  CFArrayRef window_list = CreateWindowListWithExclusion(excluded_window_);

  for (size_t i = 0; i < displays_to_capture.size(); ++i) {
    const MacDisplayConfiguration& display_config = displays_to_capture[i];

    // Capturing mixed-DPI on one surface is hard, so we only return displays
    // that match the "primary" display's DPI. The primary display is always
    // the first in the list.
    if (i > 0 && display_config.dip_to_pixel_scale !=
        displays_to_capture[0].dip_to_pixel_scale) {
      continue;
    }
    // Determine the display's position relative to the desktop, in pixels.
    DesktopRect display_bounds = display_config.pixel_bounds;
    display_bounds.Translate(-screen_pixel_bounds_.left(),
                             -screen_pixel_bounds_.top());

    // Determine which parts of the blit region, if any, lay within the monitor.
    DesktopRegion copy_region = region;
    copy_region.IntersectWith(display_bounds);
    if (copy_region.is_empty())
      continue;

    // Translate the region to be copied into display-relative coordinates.
    copy_region.Translate(-display_bounds.left(), -display_bounds.top());

    DesktopRect excluded_window_bounds;
    CGImageRef excluded_image = NULL;
    CFDataRef excluded_window_region_data = NULL;
    if (excluded_window_ && window_list) {
      // Get the region of the excluded window relative the primary display.
      excluded_window_bounds = GetExcludedWindowPixelBounds(
          excluded_window_, display_config.dip_to_pixel_scale);
      excluded_window_bounds.IntersectWith(display_config.pixel_bounds);

      // Create the image under the excluded window first, because it's faster
      // than captuing the whole display.
      if (!excluded_window_bounds.is_empty()) {
        excluded_image = CreateExcludedWindowRegionImage(
            excluded_window_bounds,
            display_config.dip_to_pixel_scale,
            window_list,
            &excluded_window_region_data);
      }
    }

    // Create an image containing a snapshot of the display.
    CGImageRef image = CGDisplayCreateImage(display_config.id);
    if (image == NULL)
      continue;

    // Request access to the raw pixel data via the image's DataProvider.
    CGDataProviderRef provider = CGImageGetDataProvider(image);
    CFDataRef data = CGDataProviderCopyData(provider);
    assert(data);

    const uint8_t* display_base_address = CFDataGetBytePtr(data);
    int src_bytes_per_row = CGImageGetBytesPerRow(image);
    int src_bytes_per_pixel = CGImageGetBitsPerPixel(image) / 8;

    // Calculate where in the output buffer the display's origin is.
    uint8_t* out_ptr = frame.data() +
        (display_bounds.left() * src_bytes_per_pixel) +
        (display_bounds.top() * frame.stride());

    // Copy the dirty region from the display buffer into our desktop buffer.
    for (DesktopRegion::Iterator i(copy_region); !i.IsAtEnd(); i.Advance()) {
      CopyRect(display_base_address,
               src_bytes_per_row,
               out_ptr,
               frame.stride(),
               src_bytes_per_pixel,
               i.rect());
    }

    // Copy the region of the excluded window to the frame.
    if (excluded_image) {
      assert(excluded_window_region_data);
      display_base_address = CFDataGetBytePtr(excluded_window_region_data);
      src_bytes_per_row = CGImageGetBytesPerRow(excluded_image);

      // Translate the bounds relative to the desktop, because |frame| data
      // starts from the desktop top-left corner.
      DesktopRect window_bounds_relative_to_desktop(excluded_window_bounds);
      window_bounds_relative_to_desktop.Translate(
          -screen_pixel_bounds_.left(), -screen_pixel_bounds_.top());
      out_ptr = frame.data() +
          (window_bounds_relative_to_desktop.left() * src_bytes_per_pixel) +
          (window_bounds_relative_to_desktop.top() * frame.stride());

      CopyRect(display_base_address,
               src_bytes_per_row,
               out_ptr,
               frame.stride(),
               src_bytes_per_pixel,
               DesktopRect::MakeSize(excluded_window_bounds.size()));
      CFRelease(excluded_window_region_data);
      CFRelease(excluded_image);
    }

    CFRelease(data);
    CFRelease(image);
  }
  if (window_list)
    CFRelease(window_list);
  return true;
}

void ScreenCapturerMac::ScreenConfigurationChanged() {
  if (current_display_) {
    const MacDisplayConfiguration* config =
        desktop_config_.FindDisplayConfigurationById(current_display_);
    screen_pixel_bounds_ = config ? config->pixel_bounds : DesktopRect();
    dip_to_pixel_scale_ = config ? config->dip_to_pixel_scale : 1.0f;
  } else {
    screen_pixel_bounds_ = desktop_config_.pixel_bounds;
    dip_to_pixel_scale_ = desktop_config_.dip_to_pixel_scale;
  }

  // Release existing buffers, which will be of the wrong size.
  ReleaseBuffers();

  // Clear the dirty region, in case the display is down-sizing.
  helper_.ClearInvalidRegion();

  // Re-mark the entire desktop as dirty.
  helper_.InvalidateScreen(screen_pixel_bounds_.size());

  // Make sure the frame buffers will be reallocated.
  queue_.Reset();

  // CgBlitPostLion uses CGDisplayCreateImage() to snapshot each display's
  // contents. Although the API exists in OS 10.6, it crashes the caller if
  // the machine has no monitor connected, so we fall back to depcreated APIs
  // when running on 10.6.
  if (rtc::GetOSVersionName() >= rtc::kMacOSLion) {
    LOG(LS_INFO) << "Using CgBlitPostLion.";
    // No need for any OpenGL support on Lion
    return;
  }

  // Dynamically link to the deprecated pre-Lion capture APIs.
  app_services_library_ = dlopen(kApplicationServicesLibraryName,
                                 RTLD_LAZY);
  if (!app_services_library_) {
    LOG_F(LS_ERROR) << "Failed to open " << kApplicationServicesLibraryName;
    abort();
  }

  opengl_library_ = dlopen(kOpenGlLibraryName, RTLD_LAZY);
  if (!opengl_library_) {
    LOG_F(LS_ERROR) << "Failed to open " << kOpenGlLibraryName;
    abort();
  }

  cg_display_base_address_ = reinterpret_cast<CGDisplayBaseAddressFunc>(
      dlsym(app_services_library_, "CGDisplayBaseAddress"));
  cg_display_bytes_per_row_ = reinterpret_cast<CGDisplayBytesPerRowFunc>(
      dlsym(app_services_library_, "CGDisplayBytesPerRow"));
  cg_display_bits_per_pixel_ = reinterpret_cast<CGDisplayBitsPerPixelFunc>(
      dlsym(app_services_library_, "CGDisplayBitsPerPixel"));
  cgl_set_full_screen_ = reinterpret_cast<CGLSetFullScreenFunc>(
      dlsym(opengl_library_, "CGLSetFullScreen"));
  if (!(cg_display_base_address_ && cg_display_bytes_per_row_ &&
        cg_display_bits_per_pixel_ && cgl_set_full_screen_)) {
    LOG_F(LS_ERROR);
    abort();
  }

  if (desktop_config_.displays.size() > 1) {
    LOG(LS_INFO) << "Using CgBlitPreLion (Multi-monitor).";
    return;
  }

  CGDirectDisplayID mainDevice = CGMainDisplayID();
  if (!CGDisplayUsesOpenGLAcceleration(mainDevice)) {
    LOG(LS_INFO) << "Using CgBlitPreLion (OpenGL unavailable).";
    return;
  }

  LOG(LS_INFO) << "Using GlBlit";

  CGLPixelFormatAttribute attributes[] = {
    // This function does an early return if GetOSVersionName() >= kMacOSLion,
    // this code only runs on 10.6 and can be deleted once 10.6 support is
    // dropped.  So just keep using kCGLPFAFullScreen even though it was
    // deprecated in 10.6 -- it's still functional there, and it's not used on
    // newer OS X versions.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    kCGLPFAFullScreen,
#pragma clang diagnostic pop
    kCGLPFADisplayMask,
    (CGLPixelFormatAttribute)CGDisplayIDToOpenGLDisplayMask(mainDevice),
    (CGLPixelFormatAttribute)0
  };
  CGLPixelFormatObj pixel_format = NULL;
  GLint matching_pixel_format_count = 0;
  CGLError err = CGLChoosePixelFormat(attributes,
                                      &pixel_format,
                                      &matching_pixel_format_count);
  assert(err == kCGLNoError);
  err = CGLCreateContext(pixel_format, NULL, &cgl_context_);
  assert(err == kCGLNoError);
  CGLDestroyPixelFormat(pixel_format);
  (*cgl_set_full_screen_)(cgl_context_);
  CGLSetCurrentContext(cgl_context_);

  size_t buffer_size = screen_pixel_bounds_.width() *
                       screen_pixel_bounds_.height() *
                       sizeof(uint32_t);
  pixel_buffer_object_.Init(cgl_context_, buffer_size);
}

bool ScreenCapturerMac::RegisterRefreshAndMoveHandlers() {
  CGError err = CGRegisterScreenRefreshCallback(
      ScreenCapturerMac::ScreenRefreshCallback, this);
  if (err != kCGErrorSuccess) {
    LOG(LS_ERROR) << "CGRegisterScreenRefreshCallback " << err;
    return false;
  }

  err = CGScreenRegisterMoveCallback(
      ScreenCapturerMac::ScreenUpdateMoveCallback, this);
  if (err != kCGErrorSuccess) {
    LOG(LS_ERROR) << "CGScreenRegisterMoveCallback " << err;
    return false;
  }

  return true;
}

void ScreenCapturerMac::UnregisterRefreshAndMoveHandlers() {
  CGUnregisterScreenRefreshCallback(
      ScreenCapturerMac::ScreenRefreshCallback, this);
  CGScreenUnregisterMoveCallback(
      ScreenCapturerMac::ScreenUpdateMoveCallback, this);
}

void ScreenCapturerMac::ScreenRefresh(CGRectCount count,
                                      const CGRect* rect_array) {
  if (screen_pixel_bounds_.is_empty())
    return;

  DesktopRegion region;
  DesktopVector translate_vector =
      DesktopVector().subtract(screen_pixel_bounds_.top_left());
  for (CGRectCount i = 0; i < count; ++i) {
    // Convert from Density-Independent Pixel to physical pixel coordinates.
    DesktopRect rect = ScaleAndRoundCGRect(rect_array[i], dip_to_pixel_scale_);
    // Translate from local desktop to capturer framebuffer coordinates.
    rect.Translate(translate_vector);
    region.AddRect(rect);
  }

  helper_.InvalidateRegion(region);
}

void ScreenCapturerMac::ScreenUpdateMove(CGScreenUpdateMoveDelta delta,
                                         size_t count,
                                         const CGRect* rect_array) {
  // Translate |rect_array| to identify the move's destination.
  CGRect refresh_rects[count];
  for (CGRectCount i = 0; i < count; ++i) {
    refresh_rects[i] = CGRectOffset(rect_array[i], delta.dX, delta.dY);
  }

  // Currently we just treat move events the same as refreshes.
  ScreenRefresh(count, refresh_rects);
}

void ScreenCapturerMac::ScreenRefreshCallback(CGRectCount count,
                                              const CGRect* rect_array,
                                              void* user_parameter) {
  ScreenCapturerMac* capturer =
      reinterpret_cast<ScreenCapturerMac*>(user_parameter);
  if (capturer->screen_pixel_bounds_.is_empty())
    capturer->ScreenConfigurationChanged();
  capturer->ScreenRefresh(count, rect_array);
}

void ScreenCapturerMac::ScreenUpdateMoveCallback(
    CGScreenUpdateMoveDelta delta,
    size_t count,
    const CGRect* rect_array,
    void* user_parameter) {
  ScreenCapturerMac* capturer =
      reinterpret_cast<ScreenCapturerMac*>(user_parameter);
  capturer->ScreenUpdateMove(delta, count, rect_array);
}

DesktopFrame* ScreenCapturerMac::CreateFrame() {
  rtc::scoped_ptr<DesktopFrame> frame(
      new BasicDesktopFrame(screen_pixel_bounds_.size()));

  frame->set_dpi(DesktopVector(kStandardDPI * dip_to_pixel_scale_,
                               kStandardDPI * dip_to_pixel_scale_));
  return frame.release();
}

}  // namespace

// static
ScreenCapturer* ScreenCapturer::Create(const DesktopCaptureOptions& options) {
  if (!options.configuration_monitor())
    return NULL;

  rtc::scoped_ptr<ScreenCapturerMac> capturer(
      new ScreenCapturerMac(options.configuration_monitor()));
  if (!capturer->Init())
    capturer.reset();
  return capturer.release();
}

}  // namespace webrtc
