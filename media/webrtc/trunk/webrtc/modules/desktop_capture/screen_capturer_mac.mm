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
#include <sys/utsname.h>

#include "webrtc/modules/desktop_capture/desktop_frame.h"
#include "webrtc/modules/desktop_capture/desktop_geometry.h"
#include "webrtc/modules/desktop_capture/desktop_region.h"
#include "webrtc/modules/desktop_capture/mac/desktop_configuration.h"
#include "webrtc/modules/desktop_capture/mac/scoped_pixel_buffer_object.h"
#include "webrtc/modules/desktop_capture/mouse_cursor_shape.h"
#include "webrtc/modules/desktop_capture/screen_capture_frame_queue.h"
#include "webrtc/modules/desktop_capture/screen_capturer_helper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
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

// Copy pixels in the |rect| from |src_place| to |dest_plane|.
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

int GetDarwinVersion() {
  struct utsname uname_info;
  if (uname(&uname_info) != 0) {
    LOG(LS_ERROR) << "uname failed";
    return 0;
  }

  if (strcmp(uname_info.sysname, "Darwin") != 0)
    return 0;

  char* dot;
  int result = strtol(uname_info.release, &dot, 10);
  if (*dot != '.') {
    LOG(LS_ERROR) << "Failed to parse version";
    return 0;
  }

  return result;
}

bool IsOSLionOrLater() {
  static int darwin_version = GetDarwinVersion();

  // Verify that the version has been parsed correctly.
  if (darwin_version < 6) {
    LOG_F(LS_ERROR) << "Invalid Darwin version: " << darwin_version;
    abort();
  }

  // Darwin major version 11 corresponds to OSX 10.7.
  return darwin_version >= 11;
}

// The amount of time allowed for displays to reconfigure.
const int64_t kDisplayConfigurationEventTimeoutMs = 10 * 1000;

// A class to perform video frame capturing for mac.
class ScreenCapturerMac : public ScreenCapturer {
 public:
  ScreenCapturerMac();
  virtual ~ScreenCapturerMac();

  bool Init();

  // Overridden from ScreenCapturer:
  virtual void Start(Callback* callback) OVERRIDE;
  virtual void Capture(const DesktopRegion& region) OVERRIDE;
  virtual void SetMouseShapeObserver(
      MouseShapeObserver* mouse_shape_observer) OVERRIDE;

 private:
  void CaptureCursor();

  void GlBlitFast(const DesktopFrame& frame,
                  const DesktopRegion& region);
  void GlBlitSlow(const DesktopFrame& frame);
  void CgBlitPreLion(const DesktopFrame& frame,
                     const DesktopRegion& region);
  void CgBlitPostLion(const DesktopFrame& frame,
                      const DesktopRegion& region);

  // Called when the screen configuration is changed.
  void ScreenConfigurationChanged();

  bool RegisterRefreshAndMoveHandlers();
  void UnregisterRefreshAndMoveHandlers();

  void ScreenRefresh(CGRectCount count, const CGRect *rect_array);
  void ScreenUpdateMove(CGScreenUpdateMoveDelta delta,
                        size_t count,
                        const CGRect *rect_array);
  void DisplaysReconfigured(CGDirectDisplayID display,
                            CGDisplayChangeSummaryFlags flags);
  static void ScreenRefreshCallback(CGRectCount count,
                                    const CGRect *rect_array,
                                    void *user_parameter);
  static void ScreenUpdateMoveCallback(CGScreenUpdateMoveDelta delta,
                                       size_t count,
                                       const CGRect *rect_array,
                                       void *user_parameter);
  static void DisplaysReconfiguredCallback(CGDirectDisplayID display,
                                           CGDisplayChangeSummaryFlags flags,
                                           void *user_parameter);

  void ReleaseBuffers();

  Callback* callback_;
  MouseShapeObserver* mouse_shape_observer_;

  CGLContextObj cgl_context_;
  ScopedPixelBufferObject pixel_buffer_object_;

  // Queue of the frames buffers.
  ScreenCaptureFrameQueue queue_;

  // Current display configuration.
  MacDesktopConfiguration desktop_config_;

  // A thread-safe list of invalid rectangles, and the size of the most
  // recently captured screen.
  ScreenCapturerHelper helper_;

  // The last cursor that we sent to the client.
  MouseCursorShape last_cursor_;

  // Contains an invalid region from the previous capture.
  DesktopRegion last_invalid_region_;

  // Used to ensure that frame captures do not take place while displays
  // are being reconfigured.
  scoped_ptr<EventWrapper> display_configuration_capture_event_;

  // Records the Ids of attached displays which are being reconfigured.
  // Accessed on the thread on which we are notified of display events.
  std::set<CGDirectDisplayID> reconfiguring_displays_;

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
  scoped_ptr<DesktopFrame> original_frame_;

  DISALLOW_COPY_AND_ASSIGN(InvertedDesktopFrame);
};

DesktopFrame* CreateFrame(
    const MacDesktopConfiguration& desktop_config) {

  DesktopSize size(desktop_config.pixel_bounds.width(),
                           desktop_config.pixel_bounds.height());
  scoped_ptr<DesktopFrame> frame(new BasicDesktopFrame(size));

  frame->set_dpi(DesktopVector(
      kStandardDPI * desktop_config.dip_to_pixel_scale,
      kStandardDPI * desktop_config.dip_to_pixel_scale));
  return frame.release();
}

ScreenCapturerMac::ScreenCapturerMac()
    : callback_(NULL),
      mouse_shape_observer_(NULL),
      cgl_context_(NULL),
      display_configuration_capture_event_(EventWrapper::Create()),
      power_assertion_id_display_(kIOPMNullAssertionID),
      power_assertion_id_user_(kIOPMNullAssertionID),
      app_services_library_(NULL),
      cg_display_base_address_(NULL),
      cg_display_bytes_per_row_(NULL),
      cg_display_bits_per_pixel_(NULL),
      opengl_library_(NULL),
      cgl_set_full_screen_(NULL) {
  display_configuration_capture_event_->Set();
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
  CGError err = CGDisplayRemoveReconfigurationCallback(
      ScreenCapturerMac::DisplaysReconfiguredCallback, this);
  if (err != kCGErrorSuccess)
    LOG(LS_ERROR) << "CGDisplayRemoveReconfigurationCallback " << err;

  dlclose(app_services_library_);
  dlclose(opengl_library_);
}

bool ScreenCapturerMac::Init() {
  if (!RegisterRefreshAndMoveHandlers()) {
    return false;
  }

  CGError err = CGDisplayRegisterReconfigurationCallback(
      ScreenCapturerMac::DisplaysReconfiguredCallback, this);
  if (err != kCGErrorSuccess) {
    LOG(LS_ERROR) << "CGDisplayRegisterReconfigurationCallback " << err;
    return false;
  }

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

void ScreenCapturerMac::Capture(
    const DesktopRegion& region_to_capture) {
  TickTime capture_start_time = TickTime::Now();

  queue_.MoveToNextFrame();

  // Wait until the display configuration is stable. If one or more displays
  // are reconfiguring then |display_configuration_capture_event_| will not be
  // set until the reconfiguration completes.
  // TODO(wez): Replace this with an early-exit (See crbug.com/104542).
  if (!display_configuration_capture_event_->Wait(
          kDisplayConfigurationEventTimeoutMs)) {
    LOG_F(LS_ERROR) << "Event wait timed out.";
    abort();
  }

  DesktopRegion region;
  helper_.TakeInvalidRegion(&region);

  // If the current buffer is from an older generation then allocate a new one.
  // Note that we can't reallocate other buffers at this point, since the caller
  // may still be reading from them.
  if (!queue_.current_frame())
    queue_.ReplaceCurrentFrame(CreateFrame(desktop_config_));

  DesktopFrame* current_frame = queue_.current_frame();

  bool flip = false;  // GL capturers need flipping.
  if (IsOSLionOrLater()) {
    // Lion requires us to use their new APIs for doing screen capture. These
    // APIS currently crash on 10.6.8 if there is no monitor attached.
    CgBlitPostLion(*current_frame, region);
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
  display_configuration_capture_event_->Set();

  // Capture the current cursor shape and notify |callback_| if it has changed.
  CaptureCursor();

  new_frame->set_capture_time_ms(
      (TickTime::Now() - capture_start_time).Milliseconds());
  callback_->OnCaptureCompleted(new_frame);
}

void ScreenCapturerMac::SetMouseShapeObserver(
      MouseShapeObserver* mouse_shape_observer) {
  assert(!mouse_shape_observer_);
  assert(mouse_shape_observer);
  mouse_shape_observer_ = mouse_shape_observer;
}

void ScreenCapturerMac::CaptureCursor() {
  if (!mouse_shape_observer_)
    return;

  NSCursor* cursor = [NSCursor currentSystemCursor];
  if (cursor == nil)
    return;

  NSImage* nsimage = [cursor image];
  NSPoint hotspot = [cursor hotSpot];
  NSSize size = [nsimage size];
  CGImageRef image = [nsimage CGImageForProposedRect:NULL
                                             context:nil
                                               hints:nil];
  if (image == nil)
    return;

  if (CGImageGetBitsPerPixel(image) != 32 ||
      CGImageGetBytesPerRow(image) != (size.width * 4) ||
      CGImageGetBitsPerComponent(image) != 8) {
    return;
  }

  CGDataProviderRef provider = CGImageGetDataProvider(image);
  CFDataRef image_data_ref = CGDataProviderCopyData(provider);
  if (image_data_ref == NULL)
    return;

  const char* cursor_src_data =
      reinterpret_cast<const char*>(CFDataGetBytePtr(image_data_ref));
  int data_size = CFDataGetLength(image_data_ref);

  // Create a MouseCursorShape that describes the cursor and pass it to
  // the client.
  scoped_ptr<MouseCursorShape> cursor_shape(new MouseCursorShape());
  cursor_shape->size.set(size.width, size.height);
  cursor_shape->hotspot.set(hotspot.x, hotspot.y);
  cursor_shape->data.assign(cursor_src_data, cursor_src_data + data_size);

  CFRelease(image_data_ref);

  // Compare the current cursor with the last one we sent to the client. If
  // they're the same, then don't bother sending the cursor again.
  if (last_cursor_.size.equals(cursor_shape->size) &&
      last_cursor_.hotspot.equals(cursor_shape->hotspot) &&
      last_cursor_.data == cursor_shape->data) {
    return;
  }

  // Record the last cursor image that we sent to the client.
  last_cursor_ = *cursor_shape;

  mouse_shape_observer_->OnCursorShapeChanged(cursor_shape.release());
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

void ScreenCapturerMac::CgBlitPostLion(const DesktopFrame& frame,
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

    CFRelease(data);
    CFRelease(image);
  }
}

void ScreenCapturerMac::ScreenConfigurationChanged() {
  // Release existing buffers, which will be of the wrong size.
  ReleaseBuffers();

  // Clear the dirty region, in case the display is down-sizing.
  helper_.ClearInvalidRegion();

  // Refresh the cached desktop configuration.
  desktop_config_ = MacDesktopConfiguration::GetCurrent(
      MacDesktopConfiguration::TopLeftOrigin);

  // Re-mark the entire desktop as dirty.
  helper_.InvalidateScreen(
      DesktopSize(desktop_config_.pixel_bounds.width(),
                          desktop_config_.pixel_bounds.height()));

  // Make sure the frame buffers will be reallocated.
  queue_.Reset();

  // CgBlitPostLion uses CGDisplayCreateImage() to snapshot each display's
  // contents. Although the API exists in OS 10.6, it crashes the caller if
  // the machine has no monitor connected, so we fall back to depcreated APIs
  // when running on 10.6.
  if (IsOSLionOrLater()) {
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
    // This function does an early return if IsOSLionOrLater(), this code only
    // runs on 10.6 and can be deleted once 10.6 support is dropped.  So just
    // keep using kCGLPFAFullScreen even though it was deprecated in 10.6 --
    // it's still functional there, and it's not used on newer OS X versions.
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

  size_t buffer_size = desktop_config_.pixel_bounds.width() *
                       desktop_config_.pixel_bounds.height() *
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
  if (desktop_config_.pixel_bounds.is_empty())
    return;

  DesktopRegion region;

  for (CGRectCount i = 0; i < count; ++i) {
    // Convert from Density-Independent Pixel to physical pixel coordinates.
    DesktopRect rect =
      ScaleAndRoundCGRect(rect_array[i], desktop_config_.dip_to_pixel_scale);

    // Translate from local desktop to capturer framebuffer coordinates.
    rect.Translate(-desktop_config_.pixel_bounds.left(),
                   -desktop_config_.pixel_bounds.top());

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

void ScreenCapturerMac::DisplaysReconfigured(
    CGDirectDisplayID display,
    CGDisplayChangeSummaryFlags flags) {
  if (flags & kCGDisplayBeginConfigurationFlag) {
    if (reconfiguring_displays_.empty()) {
      // If this is the first display to start reconfiguring then wait on
      // |display_configuration_capture_event_| to block the capture thread
      // from accessing display memory until the reconfiguration completes.
      if (!display_configuration_capture_event_->Wait(
              kDisplayConfigurationEventTimeoutMs)) {
        LOG_F(LS_ERROR) << "Event wait timed out.";
        abort();
      }
    }

    reconfiguring_displays_.insert(display);
  } else {
    reconfiguring_displays_.erase(display);

    if (reconfiguring_displays_.empty()) {
      // If no other displays are reconfiguring then refresh capturer data
      // structures and un-block the capturer thread. Occasionally, the
      // refresh and move handlers are lost when the screen mode changes,
      // so re-register them here (the same does not appear to be true for
      // the reconfiguration handler itself).
      UnregisterRefreshAndMoveHandlers();
      RegisterRefreshAndMoveHandlers();
      ScreenConfigurationChanged();
      display_configuration_capture_event_->Set();
    }
  }
}

void ScreenCapturerMac::ScreenRefreshCallback(CGRectCount count,
                                              const CGRect* rect_array,
                                              void* user_parameter) {
  ScreenCapturerMac* capturer =
      reinterpret_cast<ScreenCapturerMac*>(user_parameter);
  if (capturer->desktop_config_.pixel_bounds.is_empty())
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

void ScreenCapturerMac::DisplaysReconfiguredCallback(
    CGDirectDisplayID display,
    CGDisplayChangeSummaryFlags flags,
    void* user_parameter) {
  ScreenCapturerMac* capturer =
      reinterpret_cast<ScreenCapturerMac*>(user_parameter);
  capturer->DisplaysReconfigured(display, flags);
}

}  // namespace

// static
ScreenCapturer* ScreenCapturer::Create() {
  scoped_ptr<ScreenCapturerMac> capturer(new ScreenCapturerMac());
  if (!capturer->Init())
    capturer.reset();
  return capturer.release();
}

}  // namespace webrtc
