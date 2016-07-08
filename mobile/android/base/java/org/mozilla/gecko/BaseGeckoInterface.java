/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.util.ActivityUtils;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.Activity;
import android.content.Context;
import android.graphics.RectF;
import android.hardware.SensorEventListener;
import android.location.LocationListener;
import android.view.View;
import android.widget.AbsoluteLayout;

public class BaseGeckoInterface implements GeckoAppShell.GeckoInterface {
    // Bug 908744: Implement GeckoEventListener
    // Bug 908752: Implement SensorEventListener
    // Bug 908755: Implement LocationListener
    // Bug 908756: Implement Tabs.OnTabsChangedListener
    // Bug 908760: Implement GeckoEventResponder

    private final Context mContext;
    private GeckoProfile mProfile;

    public BaseGeckoInterface(Context context) {
        mContext = context;
    }

    @Override
    public GeckoProfile getProfile() {
        // Fall back to default profile if we didn't load a specific one
        if (mProfile == null) {
            mProfile = GeckoProfile.get(mContext);
        }
        return mProfile;
    }

    @Override
    public Activity getActivity() {
        return (Activity)mContext;
    }

    @Override
    public String getDefaultUAString() {
        return HardwareUtils.isTablet() ? AppConstants.USER_AGENT_FENNEC_TABLET :
                                          AppConstants.USER_AGENT_FENNEC_MOBILE;
    }

    // Bug 908775: Implement this
    @Override
    public void doRestart() {}

    @Override
    public void setFullScreen(final boolean fullscreen) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                ActivityUtils.setFullScreen(getActivity(), fullscreen);
            }
        });
    }

    // Bug 908779: Implement this
    @Override
    public void addPluginView(final View view, final RectF rect, final boolean isFullScreen) {}

    // Bug 908781: Implement this
    @Override
    public void removePluginView(final View view, final boolean isFullScreen) {}

    // Bug 908783: Implement this
    @Override
    public void enableCameraView() {}

    // Bug 908785: Implement this
    @Override
    public void disableCameraView() {}

    // Bug 908786: Implement this
    @Override
    public void addAppStateListener(GeckoAppShell.AppStateListener listener) {}

    // Bug 908787: Implement this
    @Override
    public void removeAppStateListener(GeckoAppShell.AppStateListener listener) {}

    // Bug 908788: Implement this
    @Override
    public View getCameraView() {
        return null;
    }

    // Bug 908789: Implement this
    @Override
    public void notifyWakeLockChanged(String topic, String state) {}

    // Bug 908790: Implement this
    @Override
    public FormAssistPopup getFormAssistPopup() {
        return null;
    }

    @Override
    public boolean areTabsShown() {
        return false;
    }

    // Bug 908791: Implement this
    @Override
    public AbsoluteLayout getPluginContainer() {
        return null;
    }

    @Override
    public void notifyCheckUpdateResult(String result) {
        GeckoAppShell.notifyObservers("Update:CheckResult", result);
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
        return "chrome://browser/content/geckoview.xul";
    }
}
