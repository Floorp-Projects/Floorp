/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DirectManipulationOwner_h__
#define DirectManipulationOwner_h__

#include <windows.h>
#include "Units.h"

class nsWindow;
class IDirectManipulationManager;
class IDirectManipulationUpdateManager;
class IDirectManipulationViewport;

namespace mozilla {
namespace widget {

class DManipEventHandler;

class DirectManipulationOwner {
 public:
  typedef mozilla::LayoutDeviceIntRect LayoutDeviceIntRect;

  explicit DirectManipulationOwner(nsWindow* aWindow);
  ~DirectManipulationOwner();
  void Init(const LayoutDeviceIntRect& aBounds);
  void ResizeViewport(const LayoutDeviceIntRect& aBounds);
  void Destroy();

  void SetContact(UINT aContactId);

  void Update();

 private:
  nsWindow* mWindow;
#if !defined(__MINGW32__) && !defined(__MINGW64__)
  DWORD mDmViewportHandlerCookie;
  RefPtr<IDirectManipulationManager> mDmManager;
  RefPtr<IDirectManipulationUpdateManager> mDmUpdateManager;
  RefPtr<IDirectManipulationViewport> mDmViewport;
  RefPtr<DManipEventHandler> mDmHandler;
#endif
};

}  // namespace widget
}  // namespace mozilla

#endif  // #ifndef DirectManipulationOwner_h__
