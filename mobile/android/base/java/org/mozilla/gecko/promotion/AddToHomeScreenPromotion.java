/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.promotion;

import android.app.Activity;
import android.content.Context;
import android.database.Cursor;
import android.os.Bundle;
import android.support.annotation.CallSuper;
import android.util.Log;

import org.mozilla.gecko.switchboard.SwitchBoard;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.AboutPages;
import org.mozilla.gecko.BrowserApp;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.db.BrowserDB;
import org.mozilla.gecko.db.UrlAnnotations;
import org.mozilla.gecko.delegates.TabsTrayVisibilityAwareDelegate;
import org.mozilla.gecko.Experiments;
import org.mozilla.gecko.util.ThreadUtils;

import java.lang.ref.WeakReference;

import ch.boye.httpclientandroidlib.util.TextUtils;

/**
 * Promote "Add to home screen" if user visits website often.
 */
public class AddToHomeScreenPromotion extends TabsTrayVisibilityAwareDelegate implements Tabs.OnTabsChangedListener {
    public static class URLHistory {
        public final long visits;
        public final long lastVisit;

        private URLHistory(long visits, long lastVisit) {
            this.visits = visits;
            this.lastVisit = lastVisit;
        }
    }

    private static final String LOGTAG = "GeckoPromoteShortcut";

    private static final String EXPERIMENT_MINIMUM_TOTAL_VISITS = "minimumTotalVisits";
    private static final String EXPERIMENT_LAST_VISIT_MINIMUM_AGE = "lastVisitMinimumAgeMs";
    private static final String EXPERIMENT_LAST_VISIT_MAXIMUM_AGE = "lastVisitMaximumAgeMs";

    private WeakReference<Activity> activityReference;
    private boolean isEnabled;
    private int minimumVisits;
    private int lastVisitMinimumAgeMs;
    private int lastVisitMaximumAgeMs;

    @CallSuper
    @Override
    public void onCreate(BrowserApp browserApp, Bundle savedInstanceState) {
        super.onCreate(browserApp, savedInstanceState);
        activityReference = new WeakReference<Activity>(browserApp);

        initializeExperiment(browserApp);
    }

    @Override
    public void onResume(BrowserApp browserApp) {
        Tabs.registerOnTabsChangedListener(this);
    }

    @Override
    public void onPause(BrowserApp browserApp) {
        Tabs.unregisterOnTabsChangedListener(this);
    }

    private void initializeExperiment(Context context) {
        if (!SwitchBoard.isInExperiment(context, Experiments.PROMOTE_ADD_TO_HOMESCREEN)) {
            Log.v(LOGTAG, "Experiment not enabled");
            // Experiment is not enabled. No need to try to read values.
            return;
        }

        JSONObject values = SwitchBoard.getExperimentValuesFromJson(context, Experiments.PROMOTE_ADD_TO_HOMESCREEN);
        if (values == null) {
            // We didn't get any values for this experiment. Let's disable it instead of picking default
            // values that might be bad.
            return;
        }

        try {
            initializeWithValues(
                    values.getInt(EXPERIMENT_MINIMUM_TOTAL_VISITS),
                    values.getInt(EXPERIMENT_LAST_VISIT_MINIMUM_AGE),
                    values.getInt(EXPERIMENT_LAST_VISIT_MAXIMUM_AGE));
        } catch (JSONException e) {
            Log.w(LOGTAG, "Could not read experiment values", e);
        }
    }

    private void initializeWithValues(int minimumVisits, int lastVisitMinimumAgeMs, int lastVisitMaximumAgeMs) {
        this.isEnabled = true;

        this.minimumVisits = minimumVisits;
        this.lastVisitMinimumAgeMs = lastVisitMinimumAgeMs;
        this.lastVisitMaximumAgeMs = lastVisitMaximumAgeMs;
    }

    @Override
    public void onTabChanged(final Tab tab, Tabs.TabEvents msg, String data) {
        if (tab == null) {
            return;
        }

        if (!Tabs.getInstance().isSelectedTab(tab)) {
            // We only ever want to show this promotion for the current tab.
            return;
        }

        if (Tabs.TabEvents.LOADED != msg) {
            return;
        }

        if (tab.isPrivate()) {
            // Never show the prompt for private browsing tabs.
            return;
        }

        if (isTabsTrayVisible()) {
            // We only want to show this prompt if this tab is in the foreground and not on top
            // of the tabs tray.
            return;
        }

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                maybeShowPromotionForUrl(tab.getURL(), tab.getTitle());
            }
        });
    }

    private void maybeShowPromotionForUrl(String url, String title) {
        if (!isEnabled) {
            return;
        }

        final Context context = activityReference.get();
        if (context == null) {
            return;
        }

        if (!shouldShowPromotion(context, url, title)) {
            return;
        }

        HomeScreenPrompt.show(context, url, title);
    }

    private boolean shouldShowPromotion(Context context, String url, String title) {
        if (TextUtils.isEmpty(url) || TextUtils.isEmpty(title)) {
            // We require an URL and a title for the shortcut.
            return false;
        }

        if (AboutPages.isAboutPage(url)) {
            // No promotion for our internal sites.
            return false;
        }

        if (!url.startsWith("https://")) {
            // Only promote websites that are served over HTTPS.
            return false;
        }

        URLHistory history = getHistoryForURL(context, url);
        if (history == null) {
            // There's no history for this URL yet or we can't read it right now. Just ignore.
            return false;
        }

        if (history.visits < minimumVisits) {
            // This URL has not been visited often enough.
            return false;
        }

        if (history.lastVisit > System.currentTimeMillis() - lastVisitMinimumAgeMs) {
            // The last visit is too new. Do not show promotion. This is mostly to avoid that the
            // promotion shows up for a quick refreshs and in the worst case the last visit could
            // be the current visit (race).
            return false;
        }

        if (history.lastVisit < System.currentTimeMillis() - lastVisitMaximumAgeMs) {
            // The last visit is to old. Do not show promotion.
            return false;
        }

        if (hasAcceptedOrDeclinedHomeScreenShortcut(context, url)) {
            // The user has already created a shortcut in the past or actively declined to create one.
            // Let's not ask again for this url - We do not want to be annoying.
            return false;
        }

        return true;
    }

    protected boolean hasAcceptedOrDeclinedHomeScreenShortcut(Context context, String url) {
        final UrlAnnotations urlAnnotations = BrowserDB.from(context).getUrlAnnotations();
        return urlAnnotations.hasAcceptedOrDeclinedHomeScreenShortcut(context.getContentResolver(), url);
    }

    public static URLHistory getHistoryForURL(Context context, String url) {
        final GeckoProfile profile = GeckoProfile.get(context);
        final BrowserDB browserDB = BrowserDB.from(profile);

        Cursor cursor = null;
        try {
            cursor = browserDB.getHistoryForURL(context.getContentResolver(), url);

            if (cursor.moveToFirst()) {
                return new URLHistory(
                    cursor.getInt(cursor.getColumnIndex(BrowserContract.History.VISITS)),
                    cursor.getLong(cursor.getColumnIndex(BrowserContract.History.DATE_LAST_VISITED)));
            }
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }

        return null;
    }
}
