/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.health;

import android.content.SharedPreferences;

/**
 * StubbedHealthRecorder is an implementation of HealthRecorder that does (you guessed it!)
 * nothing.
 */
public class StubbedHealthRecorder implements HealthRecorder {
    public StubbedHealthRecorder() { }

    public void setCurrentSession(SessionInformation session) { }
    public void recordSessionEnd(String reason, SharedPreferences.Editor editor) { }

    public void recordGeckoStartupTime(long duration) { }
    public void recordJavaStartupTime(long duration) { }

    public void onAppLocaleChanged(String to) { }
    public void onEnvironmentChanged(final boolean startNewSession, final String sessionEndReason) { }

    public void close() { }
}
