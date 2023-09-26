/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <soundtouch/SoundTouch.h>

extern "C" namespace soundtouch
{

soundtouch::SoundTouch*
createSoundTouchObj()
{
  return new soundtouch::SoundTouch();
}

void
destroySoundTouchObj(soundtouch::SoundTouch* aObj)
{
  // SoundTouch runs deletes in its destructor, meaning they need to be run in
  // the DLL context. Gecko should send its SoundTouch obj pointers here to be
  // cleaned up.
  if (aObj) {
    delete aObj;
  }
}

}
