/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.geckoview.BuildConfig;

import android.app.Activity;
import android.content.Context;
import android.view.View;
import android.widget.AbsoluteLayout;

public class BaseGeckoInterface implements GeckoAppShell.GeckoInterface {
    private final Context mContext;

    public BaseGeckoInterface(Context context) {
        mContext = context;
    }

    @Override
    public void enableOrientationListener() {}

    @Override
    public void disableOrientationListener() {}

    // Bug 908786: Implement this
    @Override
    public void addAppStateListener(GeckoAppShell.AppStateListener listener) {}

    // Bug 908787: Implement this
    @Override
    public void removeAppStateListener(GeckoAppShell.AppStateListener listener) {}

    @Override
    public boolean areTabsShown() {
        return false;
    }

    // Bug 908792: Implement this
    @Override
    public void invalidateOptionsMenu() {}

    @Override
    public void createShortcut(String title, String URI) {
        // By default, do nothing.
    }

    @Override
    public void checkUriVisited(String uri) {
        // By default, no URIs are considered visited.
    }

    @Override
    public void markUriVisited(final String uri) {
        // By default, no URIs are marked as visited.
    }

    @Override
    public void setUriTitle(final String uri, final String title) {
        // By default, no titles are associated with URIs.
    }

    @Override
    public void setAccessibilityEnabled(boolean enabled) {
        // By default, take no action when accessibility is toggled on or off.
    }

    @Override
    public boolean openUriExternal(String targetURI, String mimeType, String packageName, String className, String action, String title) {
        // By default, never open external URIs.
        return false;
    }

    @Override
    public String[] getHandlersForMimeType(String mimeType, String action) {
        // By default, offer no handlers for any MIME type.
        return new String[] {};
    }

    @Override
    public String[] getHandlersForURL(String url, String action) {
        // By default, offer no handlers for any URL.
        return new String[] {};
    }

    @Override
    public String getDefaultChromeURI() {
        // By default, use the GeckoView-specific chrome URI.
        return "chrome://geckoview/content/geckoview.xul";
    }

    @Override
    public boolean isForegrounded() {
        return false;
    }
}
