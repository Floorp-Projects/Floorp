/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaPlayerWrapper.h"

_Nullable Class mpNowPlayingInfoCenterClass;

_Nullable Class mpRemoteCommandCenterClass;

_Nullable Class mpRemoteCommandClass;

void* _Nullable handle;

#define MEDIAPLAYER_FRAMEWORK_PATH "/System/Library/Frameworks/MediaPlayer.framework/MediaPlayer"

bool MediaPlayerWrapperInit() {
  if (handle) {
    return true;
  }
  handle = dlopen(MEDIAPLAYER_FRAMEWORK_PATH, RTLD_LAZY | RTLD_LOCAL);
  if (!handle) {
    return false;
  }
  mpNowPlayingInfoCenterClass = (Class)dlsym(handle, "OBJC_CLASS_$_MPNowPlayingInfoCenter");
  mpRemoteCommandCenterClass = (Class)dlsym(handle, "OBJC_CLASS_$_MPRemoteCommandCenter");
  mpRemoteCommandClass = (Class)dlsym(handle, "OBJC_CLASS_$_MPRemoteCommand");
  if (!mpNowPlayingInfoCenterClass) {
    return false;
  }
  if (!mpRemoteCommandCenterClass) {
    return false;
  }
  if (!mpRemoteCommandClass) {
    return false;
  }
  return true;
}

void MediaPlayerWrapperClose() {
  if (handle) {
    dlclose(handle);
    mpNowPlayingInfoCenterClass = nil;
    mpRemoteCommandCenterClass = nil;
    mpRemoteCommandClass = nil;
    handle = nullptr;
  }
}
