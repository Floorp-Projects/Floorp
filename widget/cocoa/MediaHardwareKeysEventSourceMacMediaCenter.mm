/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "MediaPlayerWrapper.h"
#include "MediaHardwareKeysEventSourceMacMediaCenter.h"

#include "mozilla/dom/MediaControlUtils.h"

using namespace mozilla::dom;

// avoid redefined macro in unified build
#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("MediaHardwareKeysEventSourceMacMediaCenter=%p, " msg, this, ##__VA_ARGS__))

extern _Nullable Class mpNowPlayingInfoCenterClass;

extern _Nullable Class mpRemoteCommandCenterClass;

extern _Nullable Class mpRemoteCommandClass;

namespace mozilla {
namespace widget {

MediaCenterEventHandler MediaHardwareKeysEventSourceMacMediaCenter::CreatePlayPauseHandler() {
  return Block_copy(^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent* event) {
    MPNowPlayingInfoCenter* center =
        (MPNowPlayingInfoCenter*)[mpNowPlayingInfoCenterClass defaultCenter];
    center.playbackState = center.playbackState == MPNowPlayingPlaybackStatePlaying
                               ? MPNowPlayingPlaybackStatePaused
                               : MPNowPlayingPlaybackStatePlaying;
    HandleEvent(MediaControlKeysEvent::ePlayPause);
    return MPRemoteCommandHandlerStatusSuccess;
  });
}

MediaCenterEventHandler MediaHardwareKeysEventSourceMacMediaCenter::CreateNextTrackHandler() {
  return Block_copy(^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent* event) {
    HandleEvent(MediaControlKeysEvent::eNextTrack);
    return MPRemoteCommandHandlerStatusSuccess;
  });
}

MediaCenterEventHandler MediaHardwareKeysEventSourceMacMediaCenter::CreatePreviousTrackHandler() {
  return Block_copy(^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent* event) {
    HandleEvent(MediaControlKeysEvent::ePrevTrack);
    return MPRemoteCommandHandlerStatusSuccess;
  });
}

MediaCenterEventHandler MediaHardwareKeysEventSourceMacMediaCenter::CreatePlayHandler() {
  return Block_copy(^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent* event) {
    MPNowPlayingInfoCenter* center =
        (MPNowPlayingInfoCenter*)[mpNowPlayingInfoCenterClass defaultCenter];
    if (center.playbackState != MPNowPlayingPlaybackStatePlaying) {
      center.playbackState = MPNowPlayingPlaybackStatePlaying;
    }
    HandleEvent(MediaControlKeysEvent::ePlay);
    return MPRemoteCommandHandlerStatusSuccess;
  });
}

MediaCenterEventHandler MediaHardwareKeysEventSourceMacMediaCenter::CreatePauseHandler() {
  return Block_copy(^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent* event) {
    MPNowPlayingInfoCenter* center =
        (MPNowPlayingInfoCenter*)[mpNowPlayingInfoCenterClass defaultCenter];
    if (center.playbackState != MPNowPlayingPlaybackStatePaused) {
      center.playbackState = MPNowPlayingPlaybackStatePaused;
    }
    HandleEvent(MediaControlKeysEvent::ePause);
    return MPRemoteCommandHandlerStatusSuccess;
  });
}

MediaHardwareKeysEventSourceMacMediaCenter::MediaHardwareKeysEventSourceMacMediaCenter() {
  if (!MediaPlayerWrapperInit()) {
    LOG("Failed to initalize MediaHardwareKeysEventSourceMacMediaCenter");
  }
  mPlayPauseHandler = CreatePlayPauseHandler();
  mNextTrackHandler = CreateNextTrackHandler();
  mPreviousTrackHandler = CreatePreviousTrackHandler();
  mPlayHandler = CreatePlayHandler();
  mPauseHandler = CreatePauseHandler();
  LOG("Create MediaHardwareKeysEventSourceMacMediaCenter");
}

MediaHardwareKeysEventSourceMacMediaCenter::~MediaHardwareKeysEventSourceMacMediaCenter() {
  LOG("Destroy MediaHardwareKeysEventSourceMacMediaCenter");
  EndListeningForEvents();
  MPNowPlayingInfoCenter* center =
      (MPNowPlayingInfoCenter*)[mpNowPlayingInfoCenterClass defaultCenter];
  center.playbackState = MPNowPlayingPlaybackStateStopped;
}

void MediaHardwareKeysEventSourceMacMediaCenter::BeginListeningForEvents() {
  MPNowPlayingInfoCenter* center =
      (MPNowPlayingInfoCenter*)[mpNowPlayingInfoCenterClass defaultCenter];
  center.playbackState = MPNowPlayingPlaybackStatePlaying;
  MPRemoteCommandCenter* commandCenter = [mpRemoteCommandCenterClass sharedCommandCenter];
  commandCenter.togglePlayPauseCommand.enabled = true;
  [commandCenter.togglePlayPauseCommand addTargetWithHandler:mPlayPauseHandler];
  commandCenter.nextTrackCommand.enabled = true;
  [commandCenter.nextTrackCommand addTargetWithHandler:mNextTrackHandler];
  commandCenter.previousTrackCommand.enabled = true;
  [commandCenter.previousTrackCommand addTargetWithHandler:mPreviousTrackHandler];
  commandCenter.playCommand.enabled = true;
  [commandCenter.playCommand addTargetWithHandler:mPlayHandler];
  commandCenter.pauseCommand.enabled = true;
  [commandCenter.pauseCommand addTargetWithHandler:mPauseHandler];
}

void MediaHardwareKeysEventSourceMacMediaCenter::EndListeningForEvents() {
  MPNowPlayingInfoCenter* center =
      (MPNowPlayingInfoCenter*)[mpNowPlayingInfoCenterClass defaultCenter];
  center.playbackState = MPNowPlayingPlaybackStatePaused;
  MPRemoteCommandCenter* commandCenter = [mpRemoteCommandCenterClass sharedCommandCenter];
  commandCenter.togglePlayPauseCommand.enabled = false;
  [commandCenter.togglePlayPauseCommand removeTarget:nil];
  commandCenter.nextTrackCommand.enabled = false;
  [commandCenter.nextTrackCommand removeTarget:nil];
  commandCenter.previousTrackCommand.enabled = false;
  [commandCenter.previousTrackCommand removeTarget:nil];
  commandCenter.playCommand.enabled = false;
  [commandCenter.playCommand removeTarget:nil];
  commandCenter.pauseCommand.enabled = false;
  [commandCenter.pauseCommand removeTarget:nil];
}

bool MediaHardwareKeysEventSourceMacMediaCenter::Open() {
  LOG("Open MediaHardwareKeysEventSourceMacMediaCenter");
  mOpened = true;
  BeginListeningForEvents();
  return true;
}

void MediaHardwareKeysEventSourceMacMediaCenter::Close() {
  LOG("Close MediaHardwareKeysEventSourceMacMediaCenter");
  EndListeningForEvents();
  mOpened = false;
  MediaControlKeysEventSource::Close();
}

bool MediaHardwareKeysEventSourceMacMediaCenter::IsOpened() const { return mOpened; }

void MediaHardwareKeysEventSourceMacMediaCenter::HandleEvent(MediaControlKeysEvent aEvent) {
  for (auto iter = mListeners.begin(); iter != mListeners.end(); ++iter) {
    (*iter)->OnKeyPressed(aEvent);
  }
}

void MediaHardwareKeysEventSourceMacMediaCenter::SetPlaybackState(PlaybackState aState) {
  MPNowPlayingInfoCenter* center =
      (MPNowPlayingInfoCenter*)[mpNowPlayingInfoCenterClass defaultCenter];
  if (aState == PlaybackState::ePlaying) {
    center.playbackState = MPNowPlayingPlaybackStatePlaying;
  } else if (aState == PlaybackState::ePaused) {
    center.playbackState = MPNowPlayingPlaybackStatePaused;
  } else if (aState == PlaybackState::eStopped) {
    center.playbackState = MPNowPlayingPlaybackStateStopped;
  }
  MediaControlKeysEventSource::SetPlaybackState(aState);
}

}
}
