/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.prompts.PromptService;
import org.mozilla.gecko.util.ActivityUtils;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.Activity;
import android.content.Context;
import android.graphics.RectF;
import android.hardware.SensorEventListener;
import android.location.LocationListener;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
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

    // Bug 908770: Implement this
    @Override
    public PromptService getPromptService() {
        return null;
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

    // Bug 908772: Implement this
    @Override
    public LocationListener getLocationListener() {
        return null;
    }

    // Bug 908773: Implement this
    @Override
    public SensorEventListener getSensorEventListener() {
        return null;
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
        GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Update:CheckResult", result));
    }

    // Bug 908792: Implement this
    @Override
    public void invalidateOptionsMenu() {}
}
