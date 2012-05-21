/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

namespace mozilla {
namespace widget {

// Start the audio session in the current process
nsresult StartAudioSession();

// Pass the information necessary to start an audio session in another process
nsresult GetAudioSessionData(nsID& aID,
                             nsString& aSessionName,
                             nsString& aIconPath);

// Receive the information necessary to start an audio session in a non-chrome
// process
nsresult RecvAudioSessionData(const nsID& aID,
                              const nsString& aSessionName,
                              const nsString& aIconPath);

// Stop the audio session in the current process
nsresult StopAudioSession();

} // namespace widget
} // namespace mozilla
