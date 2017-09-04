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
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.text.TextUtils;
import android.util.Log;

import org.mozilla.gecko.Experiments;
import org.mozilla.gecko.GeckoThread;
import org.mozilla.gecko.MmaConstants;
import org.mozilla.gecko.PrefsHelper;
import org.mozilla.gecko.R;
import org.mozilla.gecko.Tab;
import org.mozilla.gecko.Tabs;
import org.mozilla.gecko.fxa.FirefoxAccounts;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.push.PushManager;
import org.mozilla.gecko.switchboard.SwitchBoard;
import org.mozilla.gecko.util.ContextUtils;

import java.lang.ref.WeakReference;
import java.util.HashMap;
import java.util.Map;


public class MmaDelegate {

    public static final String READER_AVAILABLE = "E_Reader_Available";
    public static final String DOWNLOAD_MEDIA_SAVED_IMAGE = "E_Download_Media_Saved_Image";
    public static final String CLEARED_PRIVATE_DATA = "E_Cleared_Private_Data";
    public static final String SAVED_BOOKMARK = "E_Saved_Bookmark";
    public static final String OPENED_BOOKMARK = "E_Opened_Bookmark";
    public static final String INTERACT_WITH_SEARCH_URL_AREA = "E_Interact_With_Search_URL_Area";
    public static final String SCREENSHOT = "E_Screenshot";
    public static final String SAVED_LOGIN_AND_PASSWORD = "E_Saved_Login_And_Password";
    public static final String LAUNCH_BUT_NOT_DEFAULT_BROWSER = "E_Launch_But_Not_Default_Browser";
    public static final String LAUNCH_BROWSER = "E_Launch_Browser";
    public static final String NEW_TAB = "E_Opened_New_Tab";


    public static final String USER_ATT_FOCUS_INSTALLED = "Focus Installed";
    public static final String USER_ATT_KLAR_INSTALLED = "Klar Installed";
    public static final String USER_ATT_POCKET_INSTALLED = "Pocket Installed";
    public static final String USER_ATT_DEFAULT_BROWSER = "Default Browser";
    public static final String USER_ATT_SIGNED_IN = "Signed In Sync";

    public static final String PACKAGE_NAME_KLAR = "org.mozilla.klar";
    public static final String PACKAGE_NAME_FOCUS = "org.mozilla.focus";
    public static final String PACKAGE_NAME_POCKET = "com.ideashower.readitlater.pro";

    private static final String TAG = "MmaDelegate";
    private static final String KEY_PREF_BOOLEAN_MMA_ENABLED = "mma.enabled";
    private static final String[] PREFS = { KEY_PREF_BOOLEAN_MMA_ENABLED };


    @Deprecated
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
                        // isGeckoPrefOn needs to be set before mmaHelper.init() cause we need it for isMmaEnabled()
                        isGeckoPrefOn = true;

                        // Since user attributes are gathered in Fennec, not in MMA implementation,
                        // we gather the information here then pass to mmaHelper.init()
                        // Note that generateUserAttribute always return a non null HashMap.
                        Map<String, Object> attributes = gatherUserAttributes(activity);
                        mmaHelper.setGcmSenderId(PushManager.getSenderIds());
                        mmaHelper.init(activity, attributes);

                        if (!isDefaultBrowser(activity)) {
                            mmaHelper.event(MmaDelegate.LAUNCH_BUT_NOT_DEFAULT_BROWSER);
                        }
                        mmaHelper.event(MmaDelegate.LAUNCH_BROWSER);
                    } else {
                        isGeckoPrefOn = false;
                    }
                }
            }
        };
        PrefsHelper.addObserver(PREFS, handler);
    }

    /* This method must be called at background thread to avoid performance issues in some API level */
    @NonNull
    private static Map<String, Object> gatherUserAttributes(final Context context) {

        final Map<String, Object> attributes = new HashMap<>();

        attributes.put(USER_ATT_FOCUS_INSTALLED, ContextUtils.isPackageInstalled(context, PACKAGE_NAME_FOCUS));
        attributes.put(USER_ATT_KLAR_INSTALLED, ContextUtils.isPackageInstalled(context, PACKAGE_NAME_KLAR));
        attributes.put(USER_ATT_POCKET_INSTALLED, ContextUtils.isPackageInstalled(context, PACKAGE_NAME_POCKET));
        attributes.put(USER_ATT_DEFAULT_BROWSER, isDefaultBrowser(context));
        attributes.put(USER_ATT_SIGNED_IN, FirefoxAccounts.firefoxAccountsExist(context));

        return attributes;
    }

    public static void track(String event) {
        if (applicationContext != null && isMmaEnabled(applicationContext.get())) {
            mmaHelper.event(event);
        }
    }


    public static void track(String event, long value) {
        if (applicationContext != null && isMmaEnabled(applicationContext.get())) {
            mmaHelper.event(event, value);
        }
    }

    // isMmaEnabled should use pass-in context. The context comes from
    // 1. track(), it's called from UI (where we inject events). Context is from weak reference to Activity (assigned in init())
    // 2. handleGcmMessage(), it's called from GcmListenerService (where we handle GCM messages). Context is from the Service
    private static boolean isMmaEnabled(Context context) {

        if (context == null) {
            return false;
        }
        final boolean healthReport = GeckoPreferences.getBooleanPref(context, GeckoPreferences.PREFS_HEALTHREPORT_UPLOAD_ENABLED, true);
        final boolean inExperiment = SwitchBoard.isInExperiment(context, Experiments.LEANPLUM);
        final Tab selectedTab = Tabs.getInstance().getSelectedTab();

        // if selected tab is private, mma should be disabled.
        final boolean isInPrivateBrowsing = selectedTab != null && selectedTab.isPrivate();
        // only check Gecko Pref when Gecko is running
        // FIXME : Remove isGeckoPrefOn in bug 1396714
        final boolean skipGeckoPrefCheck = !GeckoThread.isRunning();
        return inExperiment && (skipGeckoPrefCheck || isGeckoPrefOn) && healthReport && !isInPrivateBrowsing;
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

    // Always use pass-in context. Do not use applicationContext here. applicationContext will be null if MmaDelegate.init() is not called.
    public static boolean handleGcmMessage(@NonNull Context context, String from, @NonNull Bundle bundle) {
        if (isMmaEnabled(context)) {
            mmaHelper.setCustomIcon(R.drawable.ic_status_logo);
            return mmaHelper.handleGcmMessage(context, from, bundle);
        } else {
            return false;
        }
    }

    public static String getMmaSenderId() {
        return mmaHelper.getMmaSenderId();
    }
}
