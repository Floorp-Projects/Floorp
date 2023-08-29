/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RLBoxSoundTouchFactory.h"

// Exposed C API that is used by RLBox

using namespace soundtouch;

extern "C" {

void SetSampleRate(SoundTouch* mTimeStretcher, uint srate) {
  mTimeStretcher->setSampleRate(srate);
}

void SetChannels(SoundTouch* mTimeStretcher, uint numChannels) {
  mTimeStretcher->setChannels(numChannels);
}

void SetPitch(SoundTouch* mTimeStretcher, double newPitch) {
  mTimeStretcher->setPitch(newPitch);
}

void SetSetting(SoundTouch* mTimeStretcher, int settingId, int value) {
  mTimeStretcher->setSetting(settingId, value);
}

void SetTempo(SoundTouch* mTimeStretcher, double newTempo) {
  mTimeStretcher->setTempo(newTempo);
}

void SetRate(SoundTouch* mTimeStretcher, double newRate) {
  mTimeStretcher->setRate(newRate);
}

uint NumChannels(SoundTouch* mTimeStretcher) {
  return mTimeStretcher->numChannels();
}

uint NumSamples(SoundTouch* mTimeStretcher) {
  return mTimeStretcher->numSamples();
}

uint NumUnprocessedSamples(SoundTouch* mTimeStretcher) {
  return mTimeStretcher->numUnprocessedSamples();
}

void PutSamples(SoundTouch* mTimeStretcher, const SAMPLETYPE* samples,
                uint numSamples) {
  mTimeStretcher->putSamples(samples, numSamples);
}

uint ReceiveSamples(SoundTouch* mTimeStretcher, SAMPLETYPE* output,
                    uint maxSamples) {
  return mTimeStretcher->receiveSamples(output, maxSamples);
}

void Flush(SoundTouch* mTimeStretcher) { return mTimeStretcher->flush(); }
}
