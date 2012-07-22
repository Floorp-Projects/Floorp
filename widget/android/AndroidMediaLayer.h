/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidMediaLayer_h_
#define AndroidMediaLayer_h_

#include <map>
#include <jni.h>
#include "gfxRect.h"
#include "nsISupports.h"

namespace mozilla {

class AndroidMediaLayer
{
public:
  AndroidMediaLayer();
  virtual ~AndroidMediaLayer();

  NS_INLINE_DECL_REFCOUNTING(AndroidMediaLayer)
  
  void* GetNativeWindowForContent();

  void* RequestNativeWindowForVideo();
  void  ReleaseNativeWindowForVideo(void* aWindow);

  void SetNativeWindowDimensions(void* aWindow, const gfxRect& aDimensions);

  void UpdatePosition(const gfxRect& aRect);

  bool Inverted() {
    return mInverted;
  }

  void SetInverted(bool aInverted) {
    mInverted = aInverted;
  }

  bool IsVisible() {
    return mVisible;
  }

  void SetVisible(bool aVisible);

private:
  bool mInverted;
  bool mVisible;

  class SurfaceData {
    public:
      SurfaceData() :
        surface(NULL), window(NULL) {
      }

      SurfaceData(jobject aSurface, void* aWindow) :
        surface(aSurface), window(aWindow) {
      }

      jobject surface;
      void* window;
      gfxRect dimensions;
  };

  bool EnsureContentSurface();

  SurfaceData mContentData;
  std::map<void*, SurfaceData*> mVideoSurfaces;
};

} /* mozilla */
#endif /* AndroidMediaLayer_h_ */
