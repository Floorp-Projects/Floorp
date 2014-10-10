/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.health;

import android.content.SharedPreferences;

import org.json.JSONObject;

/**
 * StubbedHealthRecorder is an implementation of HealthRecorder that does (you guessed it!)
 * nothing.
 */
public class StubbedHealthRecorder implements HealthRecorder {
    @Override
    public boolean isEnabled() { return false; }

    @Override
    public void setCurrentSession(SessionInformation session) { }
    @Override
    public void checkForOrphanSessions() { }

    @Override
    public void recordGeckoStartupTime(long duration) { }
    @Override
    public void recordJavaStartupTime(long duration) { }
    @Override
    public void recordSearch(final String engineID, final String location) { }
    @Override
    public void recordSessionEnd(String reason, SharedPreferences.Editor editor) { }
    @Override
    public void recordSessionEnd(String reason, SharedPreferences.Editor editor, final int environment) { }

    @Override
    public void onAppLocaleChanged(String to) { }
    @Override
    public void onAddonChanged(String id, JSONObject json) { }
    @Override
    public void onAddonUninstalling(String id) { }
    @Override
    public void onEnvironmentChanged() { }
    @Override
    public void onEnvironmentChanged(final boolean startNewSession, final String sessionEndReason) { }

    @Override
    public void close() { }

    @Override
    public void processDelayed() { }
}
