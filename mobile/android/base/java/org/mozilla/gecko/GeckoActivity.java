/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.ComponentName;
import android.content.Intent;
import android.support.v4.app.FragmentActivity;

public class GeckoActivity extends FragmentActivity implements GeckoActivityStatus {
    // has this activity recently started another Gecko activity?
    private boolean mGeckoActivityOpened;

    /**
     * Display any resources that show strings or encompass locale-specific
     * representations.
     *
     * onLocaleReady must always be called on the UI thread.
     */
    public void onLocaleReady(final String locale) {
    }

    @Override
    public void onPause() {
        super.onPause();

        if (getApplication() instanceof GeckoApplication) {
            ((GeckoApplication) getApplication()).onActivityPause(this);
        }
    }

    @Override
    public void onResume() {
        super.onResume();

        if (getApplication() instanceof GeckoApplication) {
            ((GeckoApplication) getApplication()).onActivityResume(this);
            mGeckoActivityOpened = false;
        }
    }

    @Override
    public void onCreate(android.os.Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (AppConstants.MOZ_ANDROID_ANR_REPORTER) {
            ANRReporter.register(getApplicationContext());
        }
    }

    @Override
    public void onDestroy() {
        if (AppConstants.MOZ_ANDROID_ANR_REPORTER) {
            ANRReporter.unregister();
        }
        super.onDestroy();
    }

    @Override
    public void startActivity(Intent intent) {
        mGeckoActivityOpened = checkIfGeckoActivity(intent);
        super.startActivity(intent);
    }

    @Override
    public void startActivityForResult(Intent intent, int request) {
        mGeckoActivityOpened = checkIfGeckoActivity(intent);
        super.startActivityForResult(intent, request);
    }

    private static boolean checkIfGeckoActivity(Intent intent) {
        // Whenever we call our own activity, the component and its package name is set.
        // If we call an activity from another package, or an open intent (leaving android to resolve)
        // component has a different package name or it is null.
        ComponentName component = intent.getComponent();
        return (component != null &&
                AppConstants.ANDROID_PACKAGE_NAME.equals(component.getPackageName()));
    }

    @Override
    public boolean isGeckoActivityOpened() {
        return mGeckoActivityOpened;
    }

    public boolean isApplicationInBackground() {
        return ((GeckoApplication) getApplication()).isApplicationInBackground();
    }

    @Override
    public void onLowMemory() {
        MemoryMonitor.getInstance().onLowMemory();
        super.onLowMemory();
    }

    @Override
    public void onTrimMemory(int level) {
        MemoryMonitor.getInstance().onTrimMemory(level);
        super.onTrimMemory(level);
    }
}
