/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils;

import android.app.Activity;
import android.app.Application;
import android.os.Bundle;
import android.text.TextUtils;

import com.adjust.sdk.Adjust;
import com.adjust.sdk.AdjustConfig;
import com.adjust.sdk.LogLevel;

import org.mozilla.focus.BuildConfig;
import org.mozilla.focus.FocusApplication;
import org.mozilla.focus.telemetry.GleanMetricsService;

public class AdjustHelper {
    public static void setupAdjustIfNeeded(FocusApplication application) {
        // RELEASE: Enable Adjust - This class has different implementations for all build types.

        //noinspection ConstantConditions
        if (TextUtils.isEmpty(BuildConfig.ADJUST_TOKEN)) {
            throw new IllegalStateException("No adjust token defined for release build");
        }

        if (!GleanMetricsService.isTelemetryEnabled(application)) {
            return;
        }

        final AdjustConfig config = new AdjustConfig(application,
                BuildConfig.ADJUST_TOKEN,
                AdjustConfig.ENVIRONMENT_PRODUCTION,
                true);

        config.setLogLevel(LogLevel.SUPRESS);

        Adjust.onCreate(config);

        application.registerActivityLifecycleCallbacks(new AdjustLifecycleCallbacks());
    }

    private static final class AdjustLifecycleCallbacks implements Application.ActivityLifecycleCallbacks {
        @Override
        public void onActivityResumed(Activity activity) {
            Adjust.onResume();
        }

        @Override
        public void onActivityPaused(Activity activity) {
            Adjust.onPause();
        }

        @Override
        public void onActivityCreated(Activity activity, Bundle savedInstanceState) {}

        @Override
        public void onActivityStarted(Activity activity) {}

        @Override
        public void onActivityStopped(Activity activity) {}

        @Override
        public void onActivitySaveInstanceState(Activity activity, Bundle outState) {}

        @Override
        public void onActivityDestroyed(Activity activity) {}
    }
}
