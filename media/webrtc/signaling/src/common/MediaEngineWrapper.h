/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
  * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef  MEDIA_ENGINE_WRAPPER_H_
#define MEDIA_ENGINE_WRAPPER_H_

#include <mozilla/Scoped.h>



namespace mozilla
{
/**
 * A Custom scoped template to release a resoure of Type T
 * with a function of Type F
 * ScopedCustomReleasePtr<webrtc::VoENetwork> ptr =
 *               webrtc::VoENetwork->GetInterface(voiceEngine);
 *
 */
template<typename T>
struct ScopedCustomReleaseTraits0 : public ScopedFreePtrTraits<T>
{
  static void release(T* ptr)
  {
    if(ptr)
    {
      (ptr)->Release();
    }
  }
};

SCOPED_TEMPLATE(ScopedCustomReleasePtr, ScopedCustomReleaseTraits0)
}//namespace


#endif
