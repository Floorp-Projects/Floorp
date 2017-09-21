/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.customtabs.CustomTabsIntent;
import android.util.Log;

import org.mozilla.gecko.home.HomeConfig;
import org.mozilla.gecko.switchboard.SwitchBoard;
import org.mozilla.gecko.webapps.WebAppActivity;
import org.mozilla.gecko.webapps.WebAppIndexer;
import org.mozilla.gecko.customtabs.CustomTabsActivity;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.mozglue.SafeIntent;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.tabqueue.TabQueueHelper;
import org.mozilla.gecko.tabqueue.TabQueueService;

import static org.mozilla.gecko.BrowserApp.ACTIVITY_REQUEST_PREFERENCES;
import static org.mozilla.gecko.deeplink.DeepLinkContract.DEEP_LINK_SCHEME;
import static org.mozilla.gecko.deeplink.DeepLinkContract.LINK_BOOKMARK_LIST;
import static org.mozilla.gecko.deeplink.DeepLinkContract.LINK_DEFAULT_BROWSER;
import static org.mozilla.gecko.deeplink.DeepLinkContract.LINK_HISTORY_LIST;
import static org.mozilla.gecko.deeplink.DeepLinkContract.LINK_PREFERENCES_HOME;
import static org.mozilla.gecko.deeplink.DeepLinkContract.LINK_PREFERENCES;
import static org.mozilla.gecko.deeplink.DeepLinkContract.LINK_PREFERENCES_ACCESSIBILITY;
import static org.mozilla.gecko.deeplink.DeepLinkContract.LINK_PREFERENCES_GENERAL;
import static org.mozilla.gecko.deeplink.DeepLinkContract.LINK_PREFERENCES_NOTIFICATIONS;
import static org.mozilla.gecko.deeplink.DeepLinkContract.LINK_PREFERENCES_PRIAVACY;
import static org.mozilla.gecko.deeplink.DeepLinkContract.LINK_PREFERENCES_SEARCH;
import static org.mozilla.gecko.deeplink.DeepLinkContract.LINK_SAVE_AS_PDF;
import static org.mozilla.gecko.deeplink.DeepLinkContract.LINK_SIGN_UP;
import static org.mozilla.gecko.deeplink.DeepLinkContract.SUMO_DEFAULT_BROWSER;
import static org.mozilla.gecko.deeplink.DeepLinkContract.LINK_FXA_SIGNIN;

import org.mozilla.gecko.deeplink.DeepLinkContract;

/**
 * Activity that receives incoming Intents and dispatches them to the appropriate activities (e.g. browser, custom tabs, web app).
 */
public class LauncherActivity extends Activity {
    private static final String TAG = LauncherActivity.class.getSimpleName();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        GeckoAppShell.ensureCrashHandling();

        final SafeIntent safeIntent = new SafeIntent(getIntent());

        // Is this deep link?
        if (isDeepLink(safeIntent)) {
            dispatchDeepLink(safeIntent);

        } else if (isShutdownIntent(safeIntent)) {
            dispatchShutdownIntent();
        // Is this web app?
        } else if (isWebAppIntent(safeIntent)) {
            dispatchWebAppIntent();

        // If it's not a view intent, it won't be a custom tabs intent either. Just launch!
        } else if (!isViewIntentWithURL(safeIntent)) {
            dispatchNormalIntent();

        } else if (isCustomTabsIntent(safeIntent) && isCustomTabsEnabled(this) ) {
            dispatchCustomTabsIntent();

        // Can we dispatch this VIEW action intent to the tab queue service?
        } else if (!safeIntent.getBooleanExtra(BrowserContract.SKIP_TAB_QUEUE_FLAG, false)
                && TabQueueHelper.TAB_QUEUE_ENABLED
                && TabQueueHelper.isTabQueueEnabled(this)) {
            dispatchTabQueueIntent();

        // Dispatch this VIEW action intent to the browser.
        } else {
            dispatchNormalIntent();
        }

        finish();
    }

    private void dispatchShutdownIntent() {
        Intent intent = new Intent(getIntent());
        intent.setClassName(getApplicationContext(), AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);
        intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        startActivity(intent);
    }

    /**
     * Launch tab queue service to display overlay.
     */
    private void dispatchTabQueueIntent() {
        Intent intent = new Intent(getIntent());
        intent.setClass(getApplicationContext(), TabQueueService.class);
        startService(intent);
    }

    /**
     * Launch the browser activity.
     */
    private void dispatchNormalIntent() {
        Intent intent = new Intent(getIntent());
        intent.setClassName(getApplicationContext(), AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);

        filterFlags(intent);

        startActivity(intent);
    }

    private void dispatchUrlIntent(@NonNull String url) {
        Intent intent = new Intent(getIntent());
        intent.setData(Uri.parse(url));
        intent.setClassName(getApplicationContext(), AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);

        filterFlags(intent);

        startActivity(intent);
    }

    private void dispatchCustomTabsIntent() {
        Intent intent = new Intent(getIntent());
        intent.setClassName(getApplicationContext(), CustomTabsActivity.class.getName());

        filterFlags(intent);

        startActivity(intent);
    }

    private void dispatchWebAppIntent() {
        final Intent intent = new Intent(getIntent());
        final String manifestPath = getIntent().getStringExtra(WebAppActivity.MANIFEST_PATH);
        final int index = WebAppIndexer.getInstance().getIndexForManifest(manifestPath, this);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setClassName(this, WebAppIndexer.WEBAPP_CLASS + index);
        startActivity(intent);
    }

    private static void filterFlags(Intent intent) {
        // Explicitly remove the new task and clear task flags (Our browser activity is a single
        // task activity and we never want to start a second task here). See bug 1280112.
        intent.setFlags(intent.getFlags() & ~Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setFlags(intent.getFlags() & ~Intent.FLAG_ACTIVITY_CLEAR_TASK);

        // LauncherActivity is started with the "exclude from recents" flag (set in manifest). We do
        // not want to propagate this flag from the launcher activity to the browser.
        intent.setFlags(intent.getFlags() & ~Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS);
    }

    private static boolean isViewIntentWithURL(@NonNull final SafeIntent safeIntent) {
        return Intent.ACTION_VIEW.equals(safeIntent.getAction())
                && safeIntent.getDataString() != null;
    }

    private static boolean isCustomTabsEnabled(@NonNull final Context context) {
        return SwitchBoard.isInExperiment(context, Experiments.CUSTOM_TABS);
    }

    private static boolean isCustomTabsIntent(@NonNull final SafeIntent safeIntent) {
        return isViewIntentWithURL(safeIntent)
                && safeIntent.hasExtra(CustomTabsIntent.EXTRA_SESSION);
    }

    private static boolean isWebAppIntent(@NonNull final SafeIntent safeIntent) {
        return GeckoApp.ACTION_WEBAPP.equals(safeIntent.getAction());
    }

    private static boolean isShutdownIntent(@NonNull final SafeIntent safeIntent) {
        return GeckoApp.ACTION_SHUTDOWN.equals(safeIntent.getAction());
    }

    private boolean isDeepLink(SafeIntent intent) {
        if (intent == null || intent.getData() == null || intent.getData().getScheme() == null
                || intent.getAction() == null) {
            return false;
        }
        boolean schemeMatched = intent.getData().getScheme().equalsIgnoreCase(DEEP_LINK_SCHEME);
        boolean actionMatched = intent.getAction().equals(Intent.ACTION_VIEW);
        return schemeMatched && actionMatched;
    }

    private void dispatchDeepLink(SafeIntent intent) {
        if (intent == null || intent.getData() == null || intent.getData().getHost() == null) {
            return;
        }
        final String host = intent.getData().getHost();

        switch (host) {
            case LINK_DEFAULT_BROWSER:
                GeckoSharedPrefs.forApp(this).edit().putBoolean(GeckoPreferences.PREFS_DEFAULT_BROWSER, true).apply();

                if (AppConstants.Versions.feature24Plus) {
                    // We are special casing the link to set the default browser here: On old Android versions we
                    // link to a SUMO page but on new Android versions we can link to the default app settings where
                    // the user can actually set a default browser (Bug 1312686).
                    final Intent changeDefaultApps = new Intent("android.settings.MANAGE_DEFAULT_APPS_SETTINGS");
                    startActivity(changeDefaultApps);
                } else {
                    dispatchUrlIntent(SUMO_DEFAULT_BROWSER);
                }
                break;
            case LINK_SAVE_AS_PDF:
                EventDispatcher.getInstance().dispatch("SaveAs:PDF", null);
                break;
            case LINK_BOOKMARK_LIST:
                String bookmarks = AboutPages.getURLForBuiltinPanelType(HomeConfig.PanelType.BOOKMARKS);
                dispatchUrlIntent(bookmarks);
                break;
            case LINK_HISTORY_LIST:
                String history = AboutPages.getURLForBuiltinPanelType(HomeConfig.PanelType.COMBINED_HISTORY);
                dispatchUrlIntent(history);
                break;
            case LINK_SIGN_UP:
                dispatchUrlIntent(AboutPages.ACCOUNTS + "?action=signup");
                break;
            case LINK_PREFERENCES:
                Intent settingsIntent = new Intent(this, GeckoPreferences.class);

                // We want to know when the Settings activity returns, because
                // we might need to redisplay based on a locale change.
                startActivityForResult(settingsIntent, ACTIVITY_REQUEST_PREFERENCES);
                break;
            case LINK_PREFERENCES_GENERAL:
            case LINK_PREFERENCES_HOME:
            case LINK_PREFERENCES_PRIAVACY:
            case LINK_PREFERENCES_SEARCH:
            case LINK_PREFERENCES_NOTIFICATIONS:
            case LINK_PREFERENCES_ACCESSIBILITY:
                settingsIntent = new Intent(this, GeckoPreferences.class);
                GeckoPreferences.setResourceToOpen(settingsIntent, host);
                startActivityForResult(settingsIntent, ACTIVITY_REQUEST_PREFERENCES);
                break;
            case LINK_FXA_SIGNIN:
                dispatchAccountsDeepLink(intent);
                break;
            default:
                Log.w(TAG, "Unrecognized deep link");
        }
    }

    private void dispatchAccountsDeepLink(final SafeIntent safeIntent) {
        final Intent intent = new Intent(Intent.ACTION_VIEW);

        final Uri intentUri = safeIntent.getData();

        final String accountsToken = intentUri.getQueryParameter(DeepLinkContract.ACCOUNTS_TOKEN_PARAM);
        final String entryPoint = intentUri.getQueryParameter(DeepLinkContract.ACCOUNTS_ENTRYPOINT_PARAM);

        String dispatchUri = AboutPages.ACCOUNTS + "?action=signin&";

        // If token is missing from the deep-link, we'll still open the accounts page.
        if (accountsToken != null) {
            dispatchUri = dispatchUri.concat(DeepLinkContract.ACCOUNTS_TOKEN_PARAM + "=" + accountsToken + "&");
        }

        // Pass through the entrypoint.
        if (entryPoint != null) {
            dispatchUri = dispatchUri.concat(DeepLinkContract.ACCOUNTS_ENTRYPOINT_PARAM + "=" + entryPoint + "&");
        }

        // Pass through any utm_* parameters.
        for (String queryParam : intentUri.getQueryParameterNames()) {
            if (queryParam.startsWith("utm_")) {
                dispatchUri = dispatchUri.concat(queryParam + "=" + intentUri.getQueryParameter(queryParam) + "&");
            }
        }

        try {
            intent.setData(Uri.parse(dispatchUri));
        } catch (Exception e) {
            Log.w(TAG, "Could not parse accounts deep link.");
        }

        intent.setClassName(getApplicationContext(), AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);

        filterFlags(intent);
        startActivity(intent);
    }

}
