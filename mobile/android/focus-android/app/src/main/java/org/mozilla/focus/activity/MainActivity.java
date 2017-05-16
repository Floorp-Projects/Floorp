/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.FragmentManager;
import android.support.v7.app.AppCompatActivity;
import android.util.AttributeSet;
import android.view.View;
import android.view.WindowManager;

import org.mozilla.focus.R;
import org.mozilla.focus.fragment.BrowserFragment;
import org.mozilla.focus.fragment.FirstrunFragment;
import org.mozilla.focus.fragment.HomeFragment;
import org.mozilla.focus.fragment.UrlInputFragment;
import org.mozilla.focus.notification.BrowsingNotificationService;
import org.mozilla.focus.telemetry.TelemetryWrapper;
import org.mozilla.focus.utils.SafeIntent;
import org.mozilla.focus.utils.Settings;
import org.mozilla.focus.web.BrowsingSession;
import org.mozilla.focus.web.IWebView;
import org.mozilla.focus.web.WebViewProvider;

public class MainActivity extends AppCompatActivity {
    public static final String ACTION_ERASE = "erase";
    public static final String EXTRA_FINISH = "finish";
    public static final String EXTRA_TEXT_SELECTION = "text_selection";
    private static final String EXTRA_SHORTCUT = "shortcut";

    private String pendingUrl;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (Settings.getInstance(this).shouldUseSecureMode()) {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        }

        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);

        setContentView(R.layout.activity_main);

        SafeIntent intent = new SafeIntent(getIntent());

        if ((intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY) != 0
                && !BrowsingSession.getInstance().isActive()) {
            // This Intent was launched from history (recent apps). Android will redeliver the
            // original Intent (which might be a VIEW intent). However if there's no active browsing
            // session then we do not want to re-process the Intent and potentially re-open a website
            // from a session that the user already "erased".
            intent = new SafeIntent(new Intent(Intent.ACTION_MAIN));
            setIntent(intent.getUnsafe());
        }

        if (savedInstanceState == null) {
            WebViewProvider.performCleanup(this);

            if (Intent.ACTION_VIEW.equals(intent.getAction())) {
                final String url = intent.getDataString();

                BrowsingSession.getInstance().loadCustomTabConfig(intent);

                if (Settings.getInstance(this).shouldShowFirstrun()) {
                    pendingUrl = url;
                    showFirstrun();
                } else {
                    showBrowserScreen(url);
                }
            } else {
                if (Settings.getInstance(this).shouldShowFirstrun()) {
                    showFirstrun();
                } else {
                    showHomeScreen();
                }
            }
        }

        WebViewProvider.preload(this);
    }

    @Override
    protected void onResume() {
        super.onResume();

        BrowsingNotificationService.foreground(this);

        TelemetryWrapper.startSession();

        if (Settings.getInstance(this).shouldUseSecureMode()) {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        } else {
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
        }
    }

    @Override
    protected void onPause() {
        if (isFinishing()) {
            WebViewProvider.performCleanup(this);
        }

        super.onPause();

        BrowsingNotificationService.background(this);

        TelemetryWrapper.stopSession();
    }

    @Override
    protected void onStop() {
        super.onStop();

        TelemetryWrapper.stopMainActivity();
    }

    @Override
    protected void onNewIntent(Intent unsafeIntent) {
        final SafeIntent intent = new SafeIntent(unsafeIntent);
        if (Intent.ACTION_VIEW.equals(intent.getAction())) {
            // We can't update our fragment right now because we need to wait until the activity is
            // resumed. So just remember this URL and load it in onResumeFragments().
            pendingUrl = intent.getDataString();
        }

        // We do not care about the previous intent anymore. But let's remember this one.
        setIntent(unsafeIntent);
        BrowsingSession.getInstance().loadCustomTabConfig(intent);
    }

    @Override
    protected void onResumeFragments() {
        super.onResumeFragments();

        final SafeIntent intent = new SafeIntent(getIntent());

        if (ACTION_ERASE.equals(intent.getAction())) {
            processEraseAction(intent);

            // We do not want to erase again the next time we resume the app.
            setIntent(new Intent(Intent.ACTION_MAIN));
        }

        if (pendingUrl != null && !Settings.getInstance(this).shouldShowFirstrun()) {
            // We have received an URL in onNewIntent(). Let's load it now.
            // Unless we're trying to show the firstrun screen, in which case we leave it pending until
            // firstrun is dismissed.
            showBrowserScreen(pendingUrl);
            pendingUrl = null;
        }
    }

    private void processEraseAction(final SafeIntent intent) {
        final boolean finishActivity = intent.getBooleanExtra(EXTRA_FINISH, false);
        final boolean fromShortcut = intent.getBooleanExtra(EXTRA_SHORTCUT, false);

        final BrowserFragment browserFragment = (BrowserFragment) getSupportFragmentManager()
                .findFragmentByTag(BrowserFragment.FRAGMENT_TAG);

        if (browserFragment != null) {
            // We are currently displaying a browser fragment. Let the fragment handle the erase and
            // play its animation.
            browserFragment.eraseAndShowHomeScreen();
        } else {
            // There's no fragment available currently. Let's delete manually and notify the service
            // that the session should have ended (normally the fragment would do both).
            WebViewProvider.performCleanup(this);
            BrowsingNotificationService.stop(this);
        }

        // The service will track the foreground/background state of our activity. If we are erasing
        // while the activity is in the background then we want to finish it immediately again.
        if (finishActivity) {
            finishAndRemoveTask();
            overridePendingTransition(0, 0); // This activity should be visible - avoid any animations.
        }

        if (fromShortcut) {
            TelemetryWrapper.eraseShortcutEvent();
        }
    }

    private void showHomeScreen() {
        // We add the home fragment to the layout if it doesn't exist yet. I tried adding the fragment
        // to the layout directly but then I wasn't able to remove it later. It was still visible but
        // without an activity attached. So let's do it manually.
        final FragmentManager fragmentManager = getSupportFragmentManager();
        if (fragmentManager.findFragmentByTag(HomeFragment.FRAGMENT_TAG) == null) {
            fragmentManager
                    .beginTransaction()
                    .replace(R.id.container, HomeFragment.create(), HomeFragment.FRAGMENT_TAG)
                    .commit();
        }
    }

    private void showFirstrun() {
        final FragmentManager fragmentManager = getSupportFragmentManager();
        if (fragmentManager.findFragmentByTag(FirstrunFragment.FRAGMENT_TAG) == null) {
            fragmentManager
                    .beginTransaction()
                    .replace(R.id.container, FirstrunFragment.create(), FirstrunFragment.FRAGMENT_TAG)
                    .commit();
        }
    }

    private void showBrowserScreen(String url) {
        getSupportFragmentManager()
                .beginTransaction()
                .replace(R.id.container,
                        BrowserFragment.create(url), BrowserFragment.FRAGMENT_TAG)
                .commit();

        final SafeIntent intent = new SafeIntent(getIntent());

        if (intent.getBooleanExtra(EXTRA_TEXT_SELECTION, false)) {
            TelemetryWrapper.textSelectionIntentEvent();
        } else if (BrowsingSession.getInstance().isCustomTab()) {
            TelemetryWrapper.customTabIntentEvent();
        } else {
            TelemetryWrapper.browseIntentEvent();
        }
    }

    @Override
    public View onCreateView(String name, Context context, AttributeSet attrs) {
        if (name.equals(IWebView.class.getName())) {
            View v = WebViewProvider.create(this, attrs);
            return v;
        }

        return super.onCreateView(name, context, attrs);
    }

    @Override
    public void onBackPressed() {
        final FragmentManager fragmentManager = getSupportFragmentManager();

        final UrlInputFragment urlInputFragment = (UrlInputFragment) fragmentManager.findFragmentByTag(UrlInputFragment.FRAGMENT_TAG);
        if (urlInputFragment != null &&
                urlInputFragment.isVisible() &&
                urlInputFragment.onBackPressed()) {
            // The URL input fragment has handled the back press. It does its own animations so
            // we do not try to remove it from outside.
            return;
        }

        final BrowserFragment browserFragment = (BrowserFragment) fragmentManager.findFragmentByTag(BrowserFragment.FRAGMENT_TAG);
        if (browserFragment != null &&
                browserFragment.isVisible() &&
                browserFragment.onBackPressed()) {
            // The Browser fragment handles back presses on its own because it might just go back
            // in the browsing history.
            return;
        }

        super.onBackPressed();
    }

    public void firstrunFinished() {
        if (pendingUrl != null) {
            // We have received an URL in onNewIntent(). Let's load it now.
            showBrowserScreen(pendingUrl);
            pendingUrl = null;
        } else {
            showHomeScreen();
        }
    }
}
