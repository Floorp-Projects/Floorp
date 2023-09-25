/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SoundTouch.h"

// Exposed C API that is used by RLBox

extern "C" {

void SetSampleRate(soundtouch::SoundTouch* mTimeStretcher, uint srate);
void SetChannels(soundtouch::SoundTouch* mTimeStretcher, uint numChannels);
void SetPitch(soundtouch::SoundTouch* mTimeStretcher, double newPitch);
void SetSetting(soundtouch::SoundTouch* mTimeStretcher, int settingId,
                int value);
void SetTempo(soundtouch::SoundTouch* mTimeStretcher, double newTempo);
void SetRate(soundtouch::SoundTouch* mTimeStretcher, double newRate);
uint NumChannels(soundtouch::SoundTouch* mTimeStretcher);
uint NumSamples(soundtouch::SoundTouch* mTimeStretcher);
uint NumUnprocessedSamples(soundtouch::SoundTouch* mTimeStretcher);
void PutSamples(soundtouch::SoundTouch* mTimeStretcher,
                const soundtouch::SAMPLETYPE* samples, uint numSamples);
uint ReceiveSamples(soundtouch::SoundTouch* mTimeStretcher,
                    soundtouch::SAMPLETYPE* output, uint maxSamples);
void Flush(soundtouch::SoundTouch* mTimeStretcher);
}
