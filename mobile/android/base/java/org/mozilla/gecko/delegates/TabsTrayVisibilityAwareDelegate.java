/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.delegates;

import android.os.Bundle;
import android.support.annotation.CallSuper;

import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.tabs.TabsPanel;

public abstract class TabsTrayVisibilityAwareDelegate extends BrowserAppDelegate {
    private boolean tabsTrayVisible;

    @Override
    @CallSuper
    public void onCreate(BrowserApp browserApp, Bundle savedInstanceState) {
        tabsTrayVisible = false;
    }

    @Override
    @CallSuper
    public void onTabsTrayShown(BrowserApp browserApp, TabsPanel tabsPanel) {
        tabsTrayVisible = true;
    }

    @Override
    @CallSuper
    public void onTabsTrayHidden(BrowserApp browserApp, TabsPanel tabsPanel) {
        tabsTrayVisible = false;
    }

    protected boolean isTabsTrayVisible() {
        return tabsTrayVisible;
    }
}
