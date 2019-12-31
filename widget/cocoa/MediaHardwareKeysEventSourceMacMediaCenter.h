/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WIDGET_COCOA_MEDIAHARDWAREKEYSEVENTSOURCEMACMEDIACENTER_H_
#define WIDGET_COCOA_MEDIAHARDWAREKEYSEVENTSOURCEMACMEDIACENTER_H_

#include "mozilla/dom/MediaControlKeysEvent.h"

#ifdef __OBJC__
@class MPRemoteCommandEvent;
#else
typedef struct objc_object MPRemoteCommandEvent;
#endif
enum MPRemoteCommandHandlerStatus : long;

namespace mozilla {
namespace widget {

typedef MPRemoteCommandHandlerStatus (^MediaCenterEventHandler)(MPRemoteCommandEvent* event);

class MediaHardwareKeysEventSourceMacMediaCenter final
    : public mozilla::dom::MediaControlKeysEventSource {
 public:
  NS_INLINE_DECL_REFCOUNTING(MediaHardwareKeysEventSourceMacMediaCenter, override)
  MediaHardwareKeysEventSourceMacMediaCenter();

  MediaCenterEventHandler CreatePlayPauseHandler();
  MediaCenterEventHandler CreateNextTrackHandler();
  MediaCenterEventHandler CreatePreviousTrackHandler();
  MediaCenterEventHandler CreatePlayHandler();
  MediaCenterEventHandler CreatePauseHandler();

  virtual bool Open() override;
  void Close() override;
  virtual bool IsOpened() const override;

 private:
  ~MediaHardwareKeysEventSourceMacMediaCenter();
  void BeginListeningForEvents();
  void EndListeningForEvents();
  void HandleEvent(dom::MediaControlKeysEvent aEvent);

  bool mOpened = false;

  MediaCenterEventHandler mPlayPauseHandler;
  MediaCenterEventHandler mNextTrackHandler;
  MediaCenterEventHandler mPreviousTrackHandler;
  MediaCenterEventHandler mPauseHandler;
  MediaCenterEventHandler mPlayHandler;
};

}  // namespace widget
}  // namespace mozilla

#endif  // WIDGET_COCOA_MEDIAHARDWAREKEYSEVENTSOURCEMACMEDIACENTER_H_
