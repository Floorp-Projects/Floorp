/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   James Willcox <jwillcox@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <android/log.h>
#include "AndroidMediaLayer.h"
#include "AndroidBridge.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "AndroidMediaLayer" , ## args)


namespace mozilla {

AndroidMediaLayer::AndroidMediaLayer()
  : mInverted(false), mVisible(true) {
}

AndroidMediaLayer::~AndroidMediaLayer() {
  if (mContentData.window && AndroidBridge::Bridge()) {
    AndroidBridge::Bridge()->ReleaseNativeWindow(mContentData.window);
    mContentData.window = NULL;
  }

  if (mContentData.surface && AndroidBridge::Bridge()) {
    AndroidBridge::Bridge()->DestroySurface(mContentData.surface);
    mContentData.surface = NULL;
  }

  std::map<void*, SurfaceData*>::iterator it;

  for (it = mVideoSurfaces.begin(); it != mVideoSurfaces.end(); it++) {
    SurfaceData* data = it->second;

    if (AndroidBridge::Bridge()) {
      AndroidBridge::Bridge()->ReleaseNativeWindow(data->window);
      AndroidBridge::Bridge()->DestroySurface(data->surface);
    }

    delete data;
  }

  mVideoSurfaces.clear();
}

bool AndroidMediaLayer::EnsureContentSurface() {
  if (!mContentData.surface && AndroidBridge::Bridge()) {
    mContentData.surface = AndroidBridge::Bridge()->CreateSurface();
    if (mContentData.surface) {
      mContentData.window = AndroidBridge::Bridge()->AcquireNativeWindow(AndroidBridge::GetJNIEnv(), mContentData.surface);
      AndroidBridge::Bridge()->SetNativeWindowFormat(mContentData.window, 0, 0, AndroidBridge::WINDOW_FORMAT_RGBA_8888);
    }
  }

  return mContentData.surface && mContentData.window;
}

void* AndroidMediaLayer::GetNativeWindowForContent() {
  if (!EnsureContentSurface())
    return NULL;

  return mContentData.window;
}

void* AndroidMediaLayer::RequestNativeWindowForVideo() {
  if (!AndroidBridge::Bridge())
    return NULL;

  jobject surface = AndroidBridge::Bridge()->CreateSurface();
  if (surface) {
    void* window = AndroidBridge::Bridge()->AcquireNativeWindow(AndroidBridge::GetJNIEnv(), surface);
    if (window) {
      AndroidBridge::Bridge()->SetNativeWindowFormat(window, 0, 0, AndroidBridge::WINDOW_FORMAT_RGBA_8888);
      mVideoSurfaces[window] = new SurfaceData(surface, window);
      return window;
    } else {
      LOG("Failed to create native window from surface");

      // Cleanup
      AndroidBridge::Bridge()->DestroySurface(surface);
    }
  }

  return NULL;
}

void AndroidMediaLayer::ReleaseNativeWindowForVideo(void* aWindow) {
  if (mVideoSurfaces.find(aWindow) == mVideoSurfaces.end() || !AndroidBridge::Bridge())
    return;

  SurfaceData* data = mVideoSurfaces[aWindow];

  AndroidBridge::Bridge()->ReleaseNativeWindow(data->window);
  AndroidBridge::Bridge()->DestroySurface(data->surface);

  mVideoSurfaces.erase(aWindow);
  delete data;
}

void AndroidMediaLayer::SetNativeWindowDimensions(void* aWindow, const gfxRect& aDimensions) {
  if (mVideoSurfaces.find(aWindow) == mVideoSurfaces.end())
    return;

  SurfaceData* data = mVideoSurfaces[aWindow];
  data->dimensions = aDimensions;
}

void AndroidMediaLayer::UpdatePosition(const gfxRect& aRect) {
  if (!mVisible || !AndroidBridge::Bridge())
    return;

  std::map<void*, SurfaceData*>::iterator it;

  for (it = mVideoSurfaces.begin(); it != mVideoSurfaces.end(); it++) {
    SurfaceData* data = it->second;

    gfxRect videoRect(aRect.x + data->dimensions.x, aRect.y + data->dimensions.y,
                      data->dimensions.width, data->dimensions.height);

    AndroidBridge::Bridge()->ShowSurface(data->surface, videoRect, mInverted, false);
  }

  if (EnsureContentSurface()) {
    AndroidBridge::Bridge()->ShowSurface(mContentData.surface, aRect, mInverted, true);
  }
}

void AndroidMediaLayer::SetVisible(bool aVisible) {
  if (aVisible == mVisible || !AndroidBridge::Bridge())
    return;

  mVisible = aVisible;
  if (mVisible)
    return;

  // Hide all surfaces
  std::map<void*, SurfaceData*>::iterator it;

  if (EnsureContentSurface())
    AndroidBridge::Bridge()->HideSurface(mContentData.surface);

  for (it = mVideoSurfaces.begin(); it != mVideoSurfaces.end(); it++) {
    SurfaceData* data = it->second;
    AndroidBridge::Bridge()->HideSurface(data->surface);
  }
}

} /* mozilla */
