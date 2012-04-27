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
