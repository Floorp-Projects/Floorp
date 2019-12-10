/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WIDGET_COCOA_MEDIAHARDWAREKEYSEVENTSOURCEMAC_H_
#define WIDGET_COCOA_MEDIAHARDWAREKEYSEVENTSOURCEMAC_H_

#import <ApplicationServices/ApplicationServices.h>
#import <CoreFoundation/CoreFoundation.h>

#include "mozilla/dom/MediaControlKeysEvent.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace widget {

class MediaHardwareKeysEventSourceMac final
    : public mozilla::dom::MediaControlKeysEventSource {
 public:
  MediaHardwareKeysEventSourceMac() = default;

  static CGEventRef EventTapCallback(CGEventTapProxy proxy, CGEventType type,
                                     CGEventRef event, void* refcon);

  bool Open() override;
  void Close() override;
  bool IsOpened() const override;

 private:
  ~MediaHardwareKeysEventSourceMac() = default;

  bool StartEventTap();
  void StopEventTap();

  // They are used to intercept mac hardware media keys.
  CFMachPortRef mEventTap = nullptr;
  CFRunLoopSourceRef mEventTapSource = nullptr;
};

}  // namespace widget
}  // namespace mozilla

#endif
