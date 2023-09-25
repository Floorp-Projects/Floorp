/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SoundTouch.h"

// Exposed C API that is used by RLBox

extern "C" {

SOUNDTOUCH_API
void SetSampleRate(soundtouch::SoundTouch* mTimeStretcher, uint srate);
SOUNDTOUCH_API
void SetChannels(soundtouch::SoundTouch* mTimeStretcher, uint numChannels);
SOUNDTOUCH_API
void SetPitch(soundtouch::SoundTouch* mTimeStretcher, double newPitch);
SOUNDTOUCH_API
void SetSetting(soundtouch::SoundTouch* mTimeStretcher, int settingId,
                int value);
SOUNDTOUCH_API
void SetTempo(soundtouch::SoundTouch* mTimeStretcher, double newTempo);
SOUNDTOUCH_API
void SetRate(soundtouch::SoundTouch* mTimeStretcher, double newRate);
SOUNDTOUCH_API
uint NumChannels(soundtouch::SoundTouch* mTimeStretcher);
SOUNDTOUCH_API
uint NumSamples(soundtouch::SoundTouch* mTimeStretcher);
SOUNDTOUCH_API
uint NumUnprocessedSamples(soundtouch::SoundTouch* mTimeStretcher);
SOUNDTOUCH_API
void PutSamples(soundtouch::SoundTouch* mTimeStretcher,
                const soundtouch::SAMPLETYPE* samples, uint numSamples);
SOUNDTOUCH_API
uint ReceiveSamples(soundtouch::SoundTouch* mTimeStretcher,
                    soundtouch::SAMPLETYPE* output, uint maxSamples);
SOUNDTOUCH_API
void Flush(soundtouch::SoundTouch* mTimeStretcher);
}
