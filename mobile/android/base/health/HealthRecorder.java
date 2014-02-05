/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.health;

import android.content.SharedPreferences;

import org.json.JSONObject;

/**
 * HealthRecorder is an interface into the Firefox Health Report storage system.
 */
public interface HealthRecorder {
    /**
     * Returns whether the Health Recorder is actively recording events.
     */
    public boolean isEnabled();

    public void setCurrentSession(SessionInformation session);
    public void checkForOrphanSessions();

    public void recordGeckoStartupTime(long duration);
    public void recordJavaStartupTime(long duration);
    public void recordSearch(final String engineID, final String location);
    public void recordSessionEnd(String reason, SharedPreferences.Editor editor);
    public void recordSessionEnd(String reason, SharedPreferences.Editor editor, final int environment);

    public void onAppLocaleChanged(String to);
    public void onAddonChanged(String id, JSONObject json);
    public void onAddonUninstalling(String id);
    public void onEnvironmentChanged();
    public void onEnvironmentChanged(final boolean startNewSession, final String sessionEndReason);

    public void close();
}
