/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_GeckoVRManager_h_
#define mozilla_GeckoVRManager_h_

#include "GeneratedJNINatives.h"
#include "mozilla/jni/Utils.h"

#if defined(MOZ_ANDROID_GOOGLE_VR)
#include "gfxVRGVRAPI.h"
extern bool SetupGVRJNI(JNIEnv* env);
#endif // defined(MOZ_ANDROID_GOOGLE_VR)

namespace mozilla {

class GeckoVRManager
    : public mozilla::java::GeckoVRManager::Natives<mozilla::GeckoVRManager>
{
  typedef mozilla::java::GeckoVRManager::Natives<mozilla::GeckoVRManager> Super;
public:
  static void Init()
  {
    Super::Init();
#if defined(MOZ_ANDROID_GOOGLE_VR)
    SetupGVRJNI(jni::GetGeckoThreadEnv());
#endif // defined(MOZ_ANDROID_GOOGLE_VR)
  }

  static void SetGVRPresentingContext(const int64_t aContext)
  {
#if defined(MOZ_ANDROID_GOOGLE_VR)
    mozilla::gfx::SetGVRPresentingContext((void*)aContext);
#endif // defined(MOZ_ANDROID_GOOGLE_VR)
  }

  static void CleanupGVRNonPresentingContext()
  {
#if defined(MOZ_ANDROID_GOOGLE_VR)
    mozilla::gfx::CleanupGVRNonPresentingContext();
#endif // defined(MOZ_ANDROID_GOOGLE_VR)
  }

  static void SetGVRPaused(const bool aPaused)
  {
#if defined(MOZ_ANDROID_GOOGLE_VR)
    mozilla::gfx::SetGVRPaused(aPaused);
#endif // defined(MOZ_ANDROID_GOOGLE_VR)
  }

  static void* CreateGVRNonPresentingContext()
  {
    return (void*)mozilla::java::GeckoVRManager::CreateGVRNonPresentingContext();
  }

  static void DestroyGVRNonPresentingContext()
  {
    mozilla::java::GeckoVRManager::DestroyGVRNonPresentingContext();
  }

  static void EnableVRMode()
  {
    mozilla::java::GeckoVRManager::EnableVRMode();
  }

  static void DisableVRMode()
  {
    mozilla::java::GeckoVRManager::DisableVRMode();
  }

  static bool IsGVRPresent()
  {
    return mozilla::java::GeckoVRManager::IsGVRPresent();
  }

};

} // namespace mozilla

#endif // mozilla_GeckoVRManager_h_
