//#filter substitution
/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.mma;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Log;

import org.mozilla.gecko.Experiments;
import org.mozilla.gecko.MmaConstants;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.switchboard.SwitchBoard;

import java.lang.ref.WeakReference;


public class MmaDelegate {

    public static final String LOADS_ARTICLES = "Loads articles";
    public static final String DOWNLOAD_VIDEOS_OR_ANY_OTHER_MEDIA = "Download videos or any other media";
    public static final String CLEAR_PRIVATE_DATA = "Clear Private Data";
    public static final String SAVE_BOOKMARK = "SaveBookmark";
    public static final String LOAD_BOOKMARK = "LoadBookmark";
    public static final String INTERACT_WITH_SEARCH_URL_AREA = "Interact with search url area";
    public static final String WHEN_USER_TAKE_A_SCREENSHOT = "When user take a screenshot";
    public static final String SAVE_PASSWORD = "SavePassword";
    public static final String VISITING_A_WEBSITE_WITH_MATCH_TO_PAST_HISTORY = "Visiting a website with match to past history";
    public static final String LAUNCH_BUT_NOT_DEFAULT_BROWSER = "Launch but not default browser";


    private static final String TAG = "MmaDelegate";
    private static final String KEY_PREF_BOOLEAN_MMA_ENABLED = "mma.enabled";
    private static final String[] PREFS = { KEY_PREF_BOOLEAN_MMA_ENABLED };


    private static boolean isGeckoPrefOn = false;
    private static MmaInterface mmaHelper = MmaConstants.getMma();
    private static WeakReference<Context> applicationContext;

    public static void init(Activity activity) {
        applicationContext = new WeakReference<>(activity.getApplicationContext());
        setupPrefHandler(activity);
    }

    public static void stop() {
        mmaHelper.stop();
    }

    private static void setupPrefHandler(final Activity activity) {
        PrefsHelper.PrefHandler handler = new PrefsHelper.PrefHandlerBase() {
            @Override
            public void prefValue(String pref, boolean value) {
                if (pref.equals(KEY_PREF_BOOLEAN_MMA_ENABLED)) {
                    Log.d(TAG, "prefValue() called with: pref = [" + pref + "], value = [" + value + "]");
                    if (value) {
                        mmaHelper.init(activity);
                        if (!isDefaultBrowser(activity)) {
                            mmaHelper.track(MmaDelegate.LAUNCH_BUT_NOT_DEFAULT_BROWSER);
                        }
                        isGeckoPrefOn = true;
                    } else {
                        isGeckoPrefOn = false;
                    }
                }
            }
        };
        PrefsHelper.addObserver(PREFS, handler);
    }


    public static void track(String event) {
        if (isMmaEnabled()) {
            mmaHelper.track(event);
        }
    }

    public static void track(String event, long value) {
        if (isMmaEnabled()) {
            mmaHelper.track(event, value);
        }
    }

    private static boolean isMmaEnabled() {
        if (applicationContext == null) {
            return false;
        }

        final Context context = applicationContext.get();
        if (context == null) {
            return false;
        }

        final boolean healthReport = GeckoPreferences.getBooleanPref(context, GeckoPreferences.PREFS_HEALTHREPORT_UPLOAD_ENABLED, true);
        final boolean inExperiment = SwitchBoard.isInExperiment(context, Experiments.LEANPLUM);

        return inExperiment && healthReport && isGeckoPrefOn;
    }


    private static boolean isDefaultBrowser(Context context) {
        final Intent viewIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://www.mozilla.org"));
        final ResolveInfo info = context.getPackageManager().resolveActivity(viewIntent, PackageManager.MATCH_DEFAULT_ONLY);
        if (info == null) {
            // No default is set
            return false;
        }

        final String packageName = info.activityInfo.packageName;
        return (TextUtils.equals(packageName, context.getPackageName()));
    }


}
