/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.webkit.WebView;

import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.utils.AppConstants;
import org.mozilla.focus.utils.Browsers;

/**
 * Helper activity that will open the Google Play store by following a redirect URL.
 */
public class InstallFirefoxActivity extends Activity {
    private static final String REDIRECT_URL = "https://app.adjust.com/gs1ao4";

    private WebView webView;

    public static ActivityInfo resolveAppStore(Context context) {
        final ResolveInfo resolveInfo = context.getPackageManager()
                .resolveActivity(createStoreIntent(), 0);

        if (resolveInfo == null || resolveInfo.activityInfo == null) {
            return null;
        }

        if (!resolveInfo.activityInfo.exported) {
            // We are not allowed to launch this activity.
            return null;
        }

        return resolveInfo.activityInfo;
    }

    private static Intent createStoreIntent() {
        return new Intent(Intent.ACTION_VIEW,
                Uri.parse("market://details?id=" + Browsers.KnownBrowser.FIREFOX.packageName));
    }

    public static void open(Context context) {
        if (AppConstants.INSTANCE.isKlarBuild()) {
            // Redirect to Google Play directly
            context.startActivity(createStoreIntent());
        } else {
            // Start this activity to load the redirect URL in a WebView.
            final Intent intent = new Intent(context, InstallFirefoxActivity.class);
            context.startActivity(intent);
        }

        TelemetryWrapper.installFirefoxEvent();
    }

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        webView = new WebView(this);

        setContentView(webView);

        webView.loadUrl(REDIRECT_URL);
    }

    @Override
    protected void onPause() {
        super.onPause();

        if (webView != null) {
            webView.onPause();
        }

        finish();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        if (webView != null) {
            webView.destroy();
        }
    }
}
