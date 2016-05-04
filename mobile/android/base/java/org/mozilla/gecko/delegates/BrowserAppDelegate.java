/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.delegates;

import android.content.Intent;
import android.os.Bundle;

import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.tabs.TabsPanel;

/**
 * Abstract class for extending the behavior of BrowserApp without adding additional code to the
 * already huge class.
 */
public abstract class BrowserAppDelegate {
    /**
     * Called when the BrowserApp activity is first created.
     */
    public void onCreate(BrowserApp browserApp, Bundle savedInstanceState) {}

    /**
     * Called after the BrowserApp activity has been stopped, prior to it being started again.
     */
    public void onRestart(BrowserApp browserApp) {}

    /**
     * Called when the BrowserApp activity is becoming visible to the user.
     */
    public void onStart(BrowserApp browserApp) {}

    /**
     * Called when the BrowserApp activity will start interacting with the user.
     */
    public void onResume(BrowserApp browserApp) {}

    /**
     * Called when the system is about to start resuming a previous activity.
     */
    public void onPause(BrowserApp browserApp) {}

    /**
     * Called when BrowserApp activity is no longer visible to the user.
     */
    public void onStop(BrowserApp browserApp) {}

    /**
     * The final call before the BrowserApp activity is destroyed.
     */
    public void onDestroy(BrowserApp browserApp) {}

    /**
     * Called when BrowserApp already exists and a new Intent to re-launch it was fired.
     */
    public void onNewIntent(BrowserApp browserApp, Intent intent) {}

    /**
     * Called when the tabs tray is opened.
     */
    public void onTabsTrayShown(BrowserApp browserApp, TabsPanel tabsPanel) {}

    /**
     * Called when the tabs tray is closed.
     */
    public void onTabsTrayHidden(BrowserApp browserApp, TabsPanel tabsPanel) {}

    /**
     * Called when an activity started using startActivityForResult() returns.
     *
     * Delegates should only use request and result codes declared in BrowserApp itself (as opposed
     * to declarations in the delegate), in order to avoid conflicts.
     */
    public void onActivityResult(BrowserApp browserApp, int requestCode, int resultCode, Intent data) {}
}

