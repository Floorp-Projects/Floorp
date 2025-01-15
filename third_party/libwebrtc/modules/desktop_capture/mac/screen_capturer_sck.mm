/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/mac/screen_capturer_sck.h"

#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <atomic>

#include "modules/desktop_capture/mac/desktop_frame_iosurface.h"
#include "modules/desktop_capture/shared_desktop_frame.h"
#include "rtc_base/logging.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/time_utils.h"
#include "sck_picker_handle.h"
#include "sdk/objc/helpers/scoped_cftyperef.h"

using webrtc::DesktopFrameIOSurface;

#define SCK_AVAILABLE @available(macOS 14.0, *)
#define SCCSPICKER_AVAILABLE @available(macOS 15.0, *)

namespace webrtc {
class ScreenCapturerSck;
}  // namespace webrtc

// The ScreenCaptureKit API was available in macOS 12.3, but full-screen capture was reported to be
// broken before macOS 13 - see http://crbug.com/40234870.
// Also, the `SCContentFilter` fields `contentRect` and `pointPixelScale` were introduced in
// macOS 14.
API_AVAILABLE(macos(14.0))
@interface SckHelper : NSObject <SCStreamDelegate, SCStreamOutput, SCContentSharingPickerObserver>

- (instancetype)initWithCapturer:(webrtc::ScreenCapturerSck*)capturer;

- (void)onShareableContentCreated:(SCShareableContent*)content;

// Called just before the capturer is destroyed. This avoids a dangling pointer, and prevents any
// new calls into a deleted capturer. If any method-call on the capturer is currently running on a
// different thread, this blocks until it completes.
- (void)releaseCapturer;

@end

namespace webrtc {

bool ScreenCapturerSckAvailable() {
  bool sonomaOrHigher = false;
  if (SCK_AVAILABLE) {
    sonomaOrHigher = true;
  }
  return sonomaOrHigher;
}

bool GenericCapturerSckWithPickerAvailable() {
  bool available = false;
  if (SCCSPICKER_AVAILABLE) {
    available = true;
  }
  return available;
}

class API_AVAILABLE(macos(14.0)) ScreenCapturerSck final : public DesktopCapturer {
 public:
  explicit ScreenCapturerSck(const DesktopCaptureOptions& options);
  ScreenCapturerSck(const DesktopCaptureOptions& options, SCContentSharingPickerMode modes);
  ScreenCapturerSck(const ScreenCapturerSck&) = delete;
  ScreenCapturerSck& operator=(const ScreenCapturerSck&) = delete;

  ~ScreenCapturerSck() override;

  // DesktopCapturer interface. All these methods run on the caller's thread.
  void Start(DesktopCapturer::Callback* callback) override;
  void SetMaxFrameRate(uint32_t max_frame_rate) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;
  // Prep for implementing DelegatedSourceListController interface, for now used by Start().
  // Triggers SCContentSharingPicker. Runs on the caller's thread.
  void EnsureVisible();
  // Helper functions to forward SCContentSharingPickerObserver notifications to
  // source_list_observer_.
  void NotifySourceSelection(SCContentFilter* filter, SCStream* stream);
  void NotifySourceCancelled(SCStream* stream);
  void NotifySourceError();

  // Called after a SCStreamDelegate stop notification.
  void NotifyCaptureStopped(SCStream* stream);

  // Called by SckHelper when shareable content is returned by ScreenCaptureKit. `content` will be
  // nil if an error occurred. May run on an arbitrary thread.
  void OnShareableContentCreated(SCShareableContent* content);

  // Start capture with the given filter. Creates or updates stream_ as needed.
  void StartWithFilter(SCContentFilter* filter) RTC_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Called by SckHelper to notify of a newly captured frame. May run on an arbitrary thread.
  void OnNewIOSurface(IOSurfaceRef io_surface, NSDictionary* attachment);

 private:
  // Called when starting the capturer or the configuration has changed (either from a
  // SelectSource() call, or the screen-resolution has changed). This tells SCK to fetch new
  // shareable content, and the completion-handler will either start a new stream, or reconfigure
  // the existing stream. Runs on the caller's thread.
  void StartOrReconfigureCapturer();

  // Helper object to receive Objective-C callbacks from ScreenCaptureKit and call into this C++
  // object. The helper may outlive this C++ instance, if a completion-handler is passed to
  // ScreenCaptureKit APIs and the C++ object is deleted before the handler executes.
  SckHelper* __strong helper_;

  // Callback for returning captured frames, or errors, to the caller. Only used on the caller's
  // thread.
  Callback* callback_ = nullptr;

  // Helper class that tracks the number of capturers needing SCContentSharingPicker to stay active.
  // Only used on the caller's thread.
  std::unique_ptr<SckPickerHandleInterface> picker_handle_;

  // Flag to track if we have added ourselves as observer to picker_handle_.
  // Only used on the caller's thread.
  bool picker_handle_registered_ = false;

  // Options passed to the constructor. May be accessed on any thread, but the options are
  // unchanged during the capturer's lifetime.
  const DesktopCaptureOptions capture_options_;

  // Modes to use iff using the system picker. See docs on SCContentSharingPickerMode.
  const SCContentSharingPickerMode picker_modes_;

  // Signals that a permanent error occurred. This may be set on any thread, and is read by
  // CaptureFrame() which runs on the caller's thread.
  std::atomic<bool> permanent_error_ = false;

  // Guards some variables that may be accessed on different threads.
  Mutex lock_;

  // Provides captured desktop frames.
  SCStream* __strong stream_ RTC_GUARDED_BY(lock_);

  // Current filter on stream_.
  SCContentFilter* __strong filter_ RTC_GUARDED_BY(lock_);

  // Currently selected display, or 0 if the full desktop is selected. This capturer does not
  // support full-desktop capture, and will fall back to the first display.
  CGDirectDisplayID current_display_ RTC_GUARDED_BY(lock_) = 0;

  // Used by CaptureFrame() to detect if the screen configuration has changed. Only used on the
  // caller's thread.
  MacDesktopConfiguration desktop_config_;

  Mutex latest_frame_lock_ RTC_ACQUIRED_AFTER(lock_);
  std::unique_ptr<SharedDesktopFrame> latest_frame_ RTC_GUARDED_BY(latest_frame_lock_);

  int32_t latest_frame_dpi_ RTC_GUARDED_BY(latest_frame_lock_) = kStandardDPI;

  // Tracks whether the latest frame contains new data since it was returned to the caller. This is
  // used to set the DesktopFrame's `updated_region` property. The flag is cleared after the frame
  // is sent to OnCaptureResult(), and is set when SCK reports a new frame with non-empty "dirty"
  // rectangles.
  // TODO: crbug.com/327458809 - Replace this flag with ScreenCapturerHelper to more accurately
  // track the dirty rectangles from the SCStreamFrameInfoDirtyRects attachment.
  bool frame_is_dirty_ RTC_GUARDED_BY(latest_frame_lock_) = true;
};

ScreenCapturerSck::ScreenCapturerSck(const DesktopCaptureOptions& options)
    : ScreenCapturerSck(options, SCContentSharingPickerModeSingleDisplay) {}

ScreenCapturerSck::ScreenCapturerSck(const DesktopCaptureOptions& options, SCContentSharingPickerMode modes)
    : capture_options_(options),
      picker_modes_(modes) {
  picker_handle_ = CreateSckPickerHandle();
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " created. allow_sck_system_picker="
                   << capture_options_.allow_sck_system_picker() << ", source="
                   << (picker_handle_ ? picker_handle_->Source() : -1) << ", mode=" << ([&modes] {
                        std::stringstream ss;
                        bool empty = true;
                        auto maybeAppend = [&](auto mode, auto* str) {
                          if (modes & mode) {
                            if (!empty) {
                              ss << "|";
                            }
                            empty = false;
                            ss << str;
                          }
                        };
                        maybeAppend(SCContentSharingPickerModeSingleWindow, "SingleWindow");
                        maybeAppend(SCContentSharingPickerModeMultipleWindows, "MultiWindow");
                        maybeAppend(SCContentSharingPickerModeSingleApplication, "SingleApp");
                        maybeAppend(SCContentSharingPickerModeMultipleApplications, "MultiApp");
                        maybeAppend(SCContentSharingPickerModeSingleDisplay, "SingleDisplay");
                        return ss.str();
                      })();
  helper_ = [[SckHelper alloc] initWithCapturer:this];
}

ScreenCapturerSck::~ScreenCapturerSck() {
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " destroyed.";
  [stream_ stopCaptureWithCompletionHandler:nil];
  [helper_ releaseCapturer];
}

void ScreenCapturerSck::Start(DesktopCapturer::Callback* callback) {
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " " << __func__ << ".";
  callback_ = callback;
  desktop_config_ = capture_options_.configuration_monitor()->desktop_configuration();
  if (capture_options_.allow_sck_system_picker()) {
    EnsureVisible();
    return;
  }
  StartOrReconfigureCapturer();
}

void ScreenCapturerSck::SetMaxFrameRate(uint32_t max_frame_rate) {
  // TODO: crbug.com/327458809 - Implement this.
}

void ScreenCapturerSck::CaptureFrame() {
  int64_t capture_start_time_millis = rtc::TimeMillis();

  if (permanent_error_) {
    RTC_LOG(LS_VERBOSE) << "ScreenCapturerSck " << this << " CaptureFrame() -> ERROR_PERMANENT";
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  MacDesktopConfiguration new_config =
      capture_options_.configuration_monitor()->desktop_configuration();
  if (!desktop_config_.Equals(new_config)) {
    desktop_config_ = new_config;
    StartOrReconfigureCapturer();
  }

  std::unique_ptr<DesktopFrame> frame;
  {
    MutexLock lock(&latest_frame_lock_);
    if (latest_frame_) {
      frame = latest_frame_->Share();
      frame->set_dpi(DesktopVector(latest_frame_dpi_, latest_frame_dpi_));
      if (frame_is_dirty_) {
        frame->mutable_updated_region()->AddRect(DesktopRect::MakeSize(frame->size()));
        frame_is_dirty_ = false;
      }
    }
  }

  if (frame) {
    RTC_LOG(LS_VERBOSE) << "ScreenCapturerSck " << this << " CaptureFrame() -> SUCCESS";
    frame->set_capture_time_ms(rtc::TimeSince(capture_start_time_millis));
    callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
  } else {
    RTC_LOG(LS_VERBOSE) << "ScreenCapturerSck " << this << " CaptureFrame() -> ERROR_TEMPORARY";
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
  }
}

void ScreenCapturerSck::EnsureVisible() {
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " " << __func__ << ".";
  if (picker_handle_) {
    if (!picker_handle_registered_) {
      picker_handle_registered_ = true;
      [picker_handle_->GetPicker() addObserver:helper_];
    }
  } else {
    // We reached the maximum number of streams.
    RTC_LOG(LS_ERROR) << "ScreenCapturerSck " << this
                      << " EnsureVisible() reached the maximum number of streams.";
    permanent_error_ = true;
    return;
  }
  SCContentSharingPicker* picker = picker_handle_->GetPicker();
  SCStream* stream;
  {
    MutexLock lock(&lock_);
    stream = stream_;
    stream_ = nil;
    filter_ = nil;
  }
  [stream removeStreamOutput:helper_ type:SCStreamOutputTypeScreen error:nil];
  [stream stopCaptureWithCompletionHandler:nil];
  SCContentSharingPickerConfiguration* config = picker.defaultConfiguration;
  config.allowedPickerModes = picker_modes_;
  picker.defaultConfiguration = config;
  // Pick a sensible style to start out with, based on our current mode.
  // Default to Screen because if using Window the picker automatically hides
  // our current window to show others.
  SCShareableContentStyle style = SCShareableContentStyleDisplay;
  if (picker_modes_ == SCContentSharingPickerModeSingleDisplay) {
    style = SCShareableContentStyleDisplay;
  } else if (picker_modes_ == SCContentSharingPickerModeSingleWindow ||
             picker_modes_ == SCContentSharingPickerModeMultipleWindows) {
    style = SCShareableContentStyleWindow;
  } else if (picker_modes_ == SCContentSharingPickerModeSingleApplication ||
             picker_modes_ == SCContentSharingPickerModeMultipleApplications) {
    style = SCShareableContentStyleApplication;
  }
  // This dies silently if maximumStreamCount is already running. We need our
  // own stream count bookkeeping because of this, and to be able to unset `active`.
  [picker presentPickerForStream:stream usingContentStyle:style];
}

void ScreenCapturerSck::NotifySourceSelection(SCContentFilter* filter, SCStream* stream) {
  MutexLock lock(&lock_);
  if (stream_ != stream) {
    // The picker selected a source for another capturer.
    RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " " << __func__ << ". stream_ != stream.";
    return;
  }
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " " << __func__ << ". Starting.";
  StartWithFilter(filter);
}

void ScreenCapturerSck::NotifySourceCancelled(SCStream* stream) {
  MutexLock lock(&lock_);
  if (stream_ != stream) {
    // The picker was cancelled for another capturer.
    return;
  }
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " " << __func__ << ".";
  if (!stream_) {
    // The initial picker was cancelled. There is no stream to fall back to.
    permanent_error_ = true;
  }
}

void ScreenCapturerSck::NotifySourceError() {
  {
    MutexLock lock(&lock_);
    if (stream_) {
      // The picker failed to start. But fear not, it was not our picker,
      // we already have a stream!
      return;
    }
  }
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " " << __func__ << ".";
  permanent_error_ = true;
}

void ScreenCapturerSck::NotifyCaptureStopped(SCStream* stream) {
  MutexLock lock(&lock_);
  if (stream_ != stream) {
    return;
  }
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " " << __func__ << ".";
  permanent_error_ = true;
}

bool ScreenCapturerSck::GetSourceList(SourceList* sources) {
  sources->clear();
  if (capture_options_.allow_sck_system_picker() && picker_handle_) {
    sources->push_back({picker_handle_->Source()});
  }
  return true;
}

bool ScreenCapturerSck::SelectSource(SourceId id) {
  if (capture_options_.allow_sck_system_picker()) {
    return true;
  }

  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " SelectSource(id=" << id << ").";
  bool stream_started = false;
  {
    MutexLock lock(&lock_);
    current_display_ = id;

    if (stream_) {
      stream_started = true;
    }
  }

  // If the capturer was already started, reconfigure it. Otherwise, wait until Start() gets called.
  if (stream_started) {
    StartOrReconfigureCapturer();
  }

  return true;
}

void ScreenCapturerSck::OnShareableContentCreated(SCShareableContent* content) {
  if (!content) {
    RTC_LOG(LS_ERROR) << "ScreenCapturerSck " << this << " getShareableContent failed.";
    permanent_error_ = true;
    return;
  }

  if (!content.displays.count) {
    RTC_LOG(LS_ERROR) << "ScreenCapturerSck " << this
                      << " getShareableContent returned no displays.";
    permanent_error_ = true;
    return;
  }

  MutexLock lock(&lock_);
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " " << __func__
                   << ". current_display_=" << current_display_;
  SCDisplay* captured_display;
  {
    for (SCDisplay* display in content.displays) {
      if (current_display_ == display.displayID) {
        captured_display = display;
        break;
      }
    }
    if (!captured_display) {
      if (current_display_ == static_cast<CGDirectDisplayID>(kFullDesktopScreenId)) {
        RTC_LOG(LS_WARNING)
            << "ScreenCapturerSck " << this
            << " Full screen capture is not supported, falling back to first display.";
      } else {
        RTC_LOG(LS_WARNING) << "ScreenCapturerSck " << this << " Display " << current_display_
                            << " not found, falling back to first display.";
      }
      captured_display = content.displays.firstObject;
    }
  }

  SCContentFilter* filter = [[SCContentFilter alloc] initWithDisplay:captured_display
                                                    excludingWindows:@[]];
  StartWithFilter(filter);
}

void ScreenCapturerSck::StartWithFilter(SCContentFilter* __strong filter) {
  lock_.AssertHeld();
  SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
  config.pixelFormat = kCVPixelFormatType_32BGRA;
  config.colorSpaceName = kCGColorSpaceSRGB;
  config.showsCursor = capture_options_.prefer_cursor_embedded();
  config.captureResolution = SCCaptureResolutionAutomatic;

  {
    MutexLock lock(&latest_frame_lock_);
    latest_frame_dpi_ = filter.pointPixelScale * kStandardDPI;
  }

  filter_ = filter;

  if (stream_) {
    RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " Updating stream configuration.";
    [stream_ updateContentFilter:filter completionHandler:nil];
    [stream_ updateConfiguration:config completionHandler:nil];
  } else {
    RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " Creating new stream.";
    stream_ = [[SCStream alloc] initWithFilter:filter configuration:config delegate:helper_];

    // TODO: crbug.com/327458809 - Choose an appropriate sampleHandlerQueue for best performance.
    NSError* add_stream_output_error;
    bool add_stream_output_result = [stream_ addStreamOutput:helper_
                                                        type:SCStreamOutputTypeScreen
                                          sampleHandlerQueue:nil
                                                       error:&add_stream_output_error];
    if (!add_stream_output_result) {
      stream_ = nil;
      filter_ = nil;
      RTC_LOG(LS_ERROR) << "ScreenCapturerSck " << this << " addStreamOutput failed.";
      permanent_error_ = true;
      return;
    }

    auto handler = ^(NSError* error) {
      if (error) {
        // It should be safe to access `this` here, because the C++ destructor calls
        // stopCaptureWithCompletionHandler on the stream, which cancels this handler.
        permanent_error_ = true;
        RTC_LOG(LS_ERROR) << "ScreenCapturerSck " << this << " Starting failed.";
      } else {
        RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " Capture started.";
      }
    };

    [stream_ startCaptureWithCompletionHandler:handler];
  }
}

void ScreenCapturerSck::OnNewIOSurface(IOSurfaceRef io_surface, NSDictionary* attachment) {
  RTC_LOG(LS_VERBOSE) << "ScreenCapturerSck " << this << " " << __func__
                      << " width=" << IOSurfaceGetWidth(io_surface)
                      << ", height=" << IOSurfaceGetHeight(io_surface) << ".";

  const auto* dirty_rects = (NSArray*)attachment[SCStreamFrameInfoDirtyRects];

  rtc::ScopedCFTypeRef<IOSurfaceRef> scoped_io_surface(io_surface, rtc::RetainPolicy::RETAIN);
  std::unique_ptr<DesktopFrameIOSurface> desktop_frame_io_surface =
      DesktopFrameIOSurface::Wrap(scoped_io_surface);
  if (!desktop_frame_io_surface) {
    RTC_LOG(LS_ERROR) << "Failed to lock IOSurface.";
    return;
  }

  std::unique_ptr<SharedDesktopFrame> frame =
      SharedDesktopFrame::Wrap(std::move(desktop_frame_io_surface));

  bool dirty;
  {
    MutexLock lock(&latest_frame_lock_);
    // Mark the frame as dirty if it has a different size, and ignore any DirtyRects attachment in
    // this case. This is because SCK does not apply a correct attachment to the frame in the case
    // where the stream was reconfigured.
    dirty = !latest_frame_ || !latest_frame_->size().equals(frame->size());
  }

  if (!dirty) {
    if (!dirty_rects) {
      // This is never expected to happen - SCK attaches a non-empty dirty-rects list to every
      // frame, even when nothing has changed.
      return;
    }
    for (NSUInteger i = 0; i < dirty_rects.count; i++) {
      const auto* rect_ptr = (__bridge CFDictionaryRef)dirty_rects[i];
      if (CFGetTypeID(rect_ptr) != CFDictionaryGetTypeID()) {
        // This is never expected to happen - the dirty-rects attachment should always be an array
        // of dictionaries.
        return;
      }
      CGRect rect{};
      CGRectMakeWithDictionaryRepresentation(rect_ptr, &rect);
      if (!CGRectIsEmpty(rect)) {
        dirty = true;
        break;
      }
    }
  }

  if (dirty) {
    MutexLock lock(&latest_frame_lock_);
    frame_is_dirty_ = true;
    std::swap(latest_frame_, frame);
  }
}

void ScreenCapturerSck::StartOrReconfigureCapturer() {
  if (capture_options_.allow_sck_system_picker()) {
    MutexLock lock(&lock_);
    if (filter_) {
      StartWithFilter(filter_);
    }
    return;
  }

  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << this << " " << __func__ << ".";
  // The copy is needed to avoid capturing `this` in the Objective-C block. Accessing `helper_`
  // inside the block is equivalent to `this->helper_` and would crash (UAF) if `this` is
  // deleted before the block is executed.
  SckHelper* local_helper = helper_;
  auto handler = ^(SCShareableContent* content, NSError* error) {
    [local_helper onShareableContentCreated:content];
  };

  [SCShareableContent getShareableContentWithCompletionHandler:handler];
}

std::unique_ptr<DesktopCapturer> CreateScreenCapturerSck(const DesktopCaptureOptions& options) {
  if (SCK_AVAILABLE) {
    return std::make_unique<ScreenCapturerSck>(options);
  }
  return nullptr;
}

std::unique_ptr<DesktopCapturer> CreateGenericCapturerSck(const DesktopCaptureOptions& options) {
  if (SCCSPICKER_AVAILABLE) {
    if (options.allow_sck_system_picker()) {
      return std::make_unique<ScreenCapturerSck>(
        options,
        SCContentSharingPickerModeSingleDisplay | SCContentSharingPickerModeMultipleWindows);
    }
  }
  return nullptr;
}

}  // namespace webrtc

@implementation SckHelper {
  // This lock is to prevent the capturer being destroyed while an instance method is still running
  // on another thread.
  webrtc::Mutex _capturer_lock;
  webrtc::ScreenCapturerSck* _capturer;
}

- (instancetype)initWithCapturer:(webrtc::ScreenCapturerSck*)capturer {
  if (self = [super init]) {
    _capturer = capturer;
  }
  return self;
}

- (void)onShareableContentCreated:(SCShareableContent*)content {
  webrtc::MutexLock lock(&_capturer_lock);
  if (_capturer) {
    _capturer->OnShareableContentCreated(content);
  }
}

- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error {
  webrtc::MutexLock lock(&_capturer_lock);
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << _capturer << " " << __func__ << ".";
  if (_capturer) {
    _capturer->NotifyCaptureStopped(stream);
  }
}

- (void)userDidStopStream:(SCStream*)stream NS_SWIFT_NAME(userDidStopStream(_:))
                              API_AVAILABLE(macos(14.4)) {
  webrtc::MutexLock lock(&_capturer_lock);
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << _capturer << " " << __func__ << ".";
  if (_capturer) {
    _capturer->NotifyCaptureStopped(stream);
  }
}

- (void)contentSharingPicker:(SCContentSharingPicker*)picker
         didUpdateWithFilter:(SCContentFilter*)filter
                   forStream:(SCStream*)stream {
  webrtc::MutexLock lock(&_capturer_lock);
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << _capturer << " " << __func__ << ".";
  if (_capturer) {
    _capturer->NotifySourceSelection(filter, stream);
  }
}

- (void)contentSharingPicker:(SCContentSharingPicker*)picker didCancelForStream:(SCStream*)stream {
  webrtc::MutexLock lock(&_capturer_lock);
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << _capturer << " " << __func__ << ".";
  if (_capturer) {
    _capturer->NotifySourceCancelled(stream);
  }
}

- (void)contentSharingPickerStartDidFailWithError:(NSError*)error {
  webrtc::MutexLock lock(&_capturer_lock);
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << _capturer << " " << __func__
                   << ". error.code=" << error.code;
  if (_capturer) {
    _capturer->NotifySourceError();
  }
}

- (void)stream:(SCStream*)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type {
  CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
  if (!pixelBuffer) {
    return;
  }

  IOSurfaceRef ioSurface = CVPixelBufferGetIOSurface(pixelBuffer);
  if (!ioSurface) {
    return;
  }

  CFArrayRef attachmentsArray =
      CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, /*createIfNecessary=*/false);
  if (!attachmentsArray || CFArrayGetCount(attachmentsArray) <= 0) {
    RTC_LOG(LS_ERROR) << "Discarding frame with no attachments.";
    return;
  }

  CFDictionaryRef attachment =
      static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachmentsArray, 0));

  webrtc::MutexLock lock(&_capturer_lock);
  if (_capturer) {
    _capturer->OnNewIOSurface(ioSurface, (__bridge NSDictionary*)attachment);
  }
}

- (void)releaseCapturer {
  webrtc::MutexLock lock(&_capturer_lock);
  RTC_LOG(LS_INFO) << "ScreenCapturerSck " << _capturer << " " << __func__ << ".";
  _capturer = nullptr;
}

@end

#undef SCK_AVAILABLE
