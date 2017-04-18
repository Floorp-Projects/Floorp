/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tabqueue;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.GeckoSharedPrefs;
import org.mozilla.gecko.R;
import org.mozilla.gecko.preferences.GeckoPreferences;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.graphics.PixelFormat;
import android.support.v4.app.NotificationCompat;
import android.support.v4.content.ContextCompat;
import android.text.TextUtils;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;

import org.json.JSONArray;
import org.json.JSONException;

import java.util.ArrayList;
import java.util.List;

public class TabQueueHelper {
    private static final String LOGTAG = "Gecko" + TabQueueHelper.class.getSimpleName();

    // Disable Tab Queue for API level 10 (GB) - Bug 1206055
    public static final boolean TAB_QUEUE_ENABLED = true;

    public static final String FILE_NAME = "tab_queue_url_list.json";
    public static final String LOAD_URLS_ACTION = "TAB_QUEUE_LOAD_URLS_ACTION";
    public static final int TAB_QUEUE_NOTIFICATION_ID = R.id.tabQueueNotification;

    public static final String PREF_TAB_QUEUE_COUNT = "tab_queue_count";
    public static final String PREF_TAB_QUEUE_LAUNCHES = "tab_queue_launches";
    public static final String PREF_TAB_QUEUE_TIMES_PROMPT_SHOWN = "tab_queue_times_prompt_shown";

    public static final int MAX_TIMES_TO_SHOW_PROMPT = 3;
    public static final int EXTERNAL_LAUNCHES_BEFORE_SHOWING_PROMPT = 3;
    private static final int MAX_NOTIFICATION_DISPLAY_COUNT = 8;

    // result codes for returning from the prompt
    public static final int TAB_QUEUE_YES = 201;
    public static final int TAB_QUEUE_NO = 202;

    /**
     * Checks if the specified context can draw on top of other apps. As of API level 23, an app
     * cannot draw on top of other apps unless it declares the SYSTEM_ALERT_WINDOW permission in
     * its manifest, AND the user specifically grants the app this capability.
     *
     * @return true if the specified context can draw on top of other apps, false otherwise.
     */
    public static boolean canDrawOverlays(Context context) {
        if (AppConstants.Versions.preMarshmallow) {
            return true; // We got the permission at install time.
        }

        // It would be nice to just use Settings.canDrawOverlays() - but this helper is buggy for
        // apps using sharedUserId (See bug 1244722).
        // Instead we'll add and remove an invisible view. If this is successful then we seem to
        // have permission to draw overlays.

        View view = new View(context);
        view.setVisibility(View.INVISIBLE);

        WindowManager.LayoutParams layoutParams = new WindowManager.LayoutParams(
                1, 1,
                WindowManager.LayoutParams.TYPE_PHONE,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE,
                PixelFormat.TRANSLUCENT);

        WindowManager windowManager = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);

        try {
            windowManager.addView(view, layoutParams);
            windowManager.removeView(view);
            return true;
        } catch (final SecurityException | WindowManager.BadTokenException e) {
            return false;
        }
    }

    /**
     * Check if we should show the tab queue prompt
     *
     * @param context
     * @return true if we should display the prompt, false if not.
     */
    public static boolean shouldShowTabQueuePrompt(Context context) {
        final SharedPreferences prefs = GeckoSharedPrefs.forApp(context);

        int numberOfTimesTabQueuePromptSeen = prefs.getInt(PREF_TAB_QUEUE_TIMES_PROMPT_SHOWN, 0);

        // Exit early if the feature is already enabled or the user has seen the
        // prompt more than MAX_TIMES_TO_SHOW_PROMPT times.
        if (isTabQueueEnabled(prefs) || numberOfTimesTabQueuePromptSeen >= MAX_TIMES_TO_SHOW_PROMPT) {
            return false;
        }

        final int viewActionIntentLaunches = prefs.getInt(PREF_TAB_QUEUE_LAUNCHES, 0) + 1;
        if (viewActionIntentLaunches < EXTERNAL_LAUNCHES_BEFORE_SHOWING_PROMPT) {
            // Allow a few external links to open before we prompt the user.
            prefs.edit().putInt(PREF_TAB_QUEUE_LAUNCHES, viewActionIntentLaunches).apply();
        } else if (viewActionIntentLaunches == EXTERNAL_LAUNCHES_BEFORE_SHOWING_PROMPT) {
            // Reset to avoid repeatedly showing the prompt if the user doesn't interact with it and
            // we get more external VIEW action intents in.
            final SharedPreferences.Editor editor = prefs.edit();
            editor.remove(TabQueueHelper.PREF_TAB_QUEUE_LAUNCHES);

            int timesPromptShown = prefs.getInt(TabQueueHelper.PREF_TAB_QUEUE_TIMES_PROMPT_SHOWN, 0) + 1;
            editor.putInt(TabQueueHelper.PREF_TAB_QUEUE_TIMES_PROMPT_SHOWN, timesPromptShown);
            editor.apply();

            // Show the prompt
            return true;
        }

        return false;
    }

    /**
     * Reads file and converts any content to JSON, adds passed in URL to the data and writes back to the file,
     * creating the file if it doesn't already exist.  This should not be run on the UI thread.
     *
     * @param profile
     * @param url      URL to add
     * @param filename filename to add URL to
     * @return the number of tabs currently queued
     */
    public static int queueURL(final GeckoProfile profile, final String url, final String filename) {
        ThreadUtils.assertNotOnUiThread();

        JSONArray jsonArray = profile.readJSONArrayFromFile(filename);

        jsonArray.put(url);

        profile.writeFile(filename, jsonArray.toString());

        return jsonArray.length();
    }

    /**
     * Remove a url from the file, if it exists.
     * If the url exists multiple times, all instances of it will be removed.
     * This should not be run on the UI thread.
     *
     * @param context
     * @param urlToRemove URL to remove
     * @param filename    filename to remove URL from
     * @return the number of queued urls
     */
    public static int removeURLFromFile(final Context context, final String urlToRemove, final String filename) {
        ThreadUtils.assertNotOnUiThread();

        final GeckoProfile profile = GeckoProfile.get(context);

        JSONArray jsonArray = profile.readJSONArrayFromFile(filename);
        JSONArray newArray = new JSONArray();
        String url;

        // Since JSONArray.remove was only added in API 19, we have to use two arrays in order to remove.
        for (int i = 0; i < jsonArray.length(); i++) {
            try {
                url = jsonArray.getString(i);
            } catch (JSONException e) {
                url = "";
            }
            if (!TextUtils.isEmpty(url) && !urlToRemove.equals(url)) {
                newArray.put(url);
            }
        }

        profile.writeFile(filename, newArray.toString());

        final SharedPreferences prefs = GeckoSharedPrefs.forApp(context);
        prefs.edit().putInt(PREF_TAB_QUEUE_COUNT, newArray.length()).apply();

        return newArray.length();
    }

    /**
     * Get up to eight of the last queued URLs for displaying in the notification.
     */
    public static List<String> getLastURLs(final Context context, final String filename) {
        final GeckoProfile profile = GeckoProfile.get(context);
        final JSONArray jsonArray = profile.readJSONArrayFromFile(filename);
        final int tabCount = Math.min(MAX_NOTIFICATION_DISPLAY_COUNT, jsonArray.length());
        final List<String> urls = new ArrayList<>(tabCount);

        for (int i = 0; i < tabCount; i++) {
            try {
                urls.add(jsonArray.getString(i));
            } catch (JSONException e) {
                Log.w(LOGTAG, "Unable to parse URL from tab queue array", e);
            }
        }

        return urls;
    }

    /**
     * Displays a notification showing the total number of tabs queue.  If there is already a notification displayed, it
     * will be replaced.
     *
     * @param context
     * @param tabsQueued
     */
    public static void showNotification(final Context context, final int tabsQueued, final List<String> urls) {
        ThreadUtils.assertNotOnUiThread();

        Intent resultIntent = new Intent();
        resultIntent.setClassName(context, AppConstants.MOZ_ANDROID_BROWSER_INTENT_CLASS);
        resultIntent.setAction(TabQueueHelper.LOAD_URLS_ACTION);

        PendingIntent pendingIntent = PendingIntent.getActivity(context, 0, resultIntent, PendingIntent.FLAG_CANCEL_CURRENT);

        final String text;
        final Resources resources = context.getResources();
        if (tabsQueued == 1) {
            text = resources.getString(R.string.tab_queue_notification_text_singular);
        } else {
            text = resources.getString(R.string.tab_queue_notification_text_plural, tabsQueued);
        }

        NotificationCompat.InboxStyle inboxStyle = new NotificationCompat.InboxStyle();
        inboxStyle.setBigContentTitle(text);
        for (String url : urls) {
            inboxStyle.addLine(url);
        }
        inboxStyle.setSummaryText(resources.getString(R.string.tab_queue_notification_title));

        NotificationCompat.Builder builder = new NotificationCompat.Builder(context)
                                                     .setSmallIcon(R.drawable.ic_status_logo)
                                                     .setContentTitle(text)
                                                     .setContentText(resources.getString(R.string.tab_queue_notification_title))
                                                     .setStyle(inboxStyle)
                                                     .setColor(ContextCompat.getColor(context, R.color.fennec_ui_orange))
                                                     .setNumber(tabsQueued)
                                                     .setContentIntent(pendingIntent);

        NotificationManager notificationManager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        notificationManager.notify(TabQueueHelper.TAB_QUEUE_NOTIFICATION_ID, builder.build());
    }

    public static boolean shouldOpenTabQueueUrls(final Context context) {
        ThreadUtils.assertNotOnUiThread();

        // TODO: Use profile shared prefs when bug 1147925 gets fixed.
        final SharedPreferences prefs = GeckoSharedPrefs.forApp(context);

        int tabsQueued = prefs.getInt(PREF_TAB_QUEUE_COUNT, 0);

        return isTabQueueEnabled(prefs) && tabsQueued > 0;
    }

    public static int getTabQueueLength(final Context context) {
        ThreadUtils.assertNotOnUiThread();

        // TODO: Use profile shared prefs when bug 1147925 gets fixed.
        final SharedPreferences prefs = GeckoSharedPrefs.forApp(context);
        return prefs.getInt(PREF_TAB_QUEUE_COUNT, 0);
    }

    public static void openQueuedUrls(final Context context, final GeckoProfile profile, final String filename, boolean shouldPerformJavaScriptCallback) {
        ThreadUtils.assertNotOnUiThread();

        removeNotification(context);

        // exit early if we don't have any tabs queued
        if (getTabQueueLength(context) < 1) {
            return;
        }

        JSONArray jsonArray = profile.readJSONArrayFromFile(filename);

        final int len = jsonArray.length();
        if (len > 0) {
            final String[] urls = new String[len];
            for (int i = 0; i < len; i++) {
                urls[i] = jsonArray.optString(i);
            }
            final GeckoBundle data = new GeckoBundle(2);
            data.putStringArray("urls", urls);
            data.putBoolean("shouldNotifyTabsOpenedToJava", shouldPerformJavaScriptCallback);
            EventDispatcher.getInstance().dispatch("Tabs:OpenMultiple", data);
        }

        try {
            profile.deleteFileFromProfileDir(filename);
        } catch (IllegalArgumentException e) {
            Log.e(LOGTAG, "Error deleting Tab Queue data file.", e);
        }

        // TODO: Use profile shared prefs when bug 1147925 gets fixed.
        final SharedPreferences prefs = GeckoSharedPrefs.forApp(context);
        prefs.edit().remove(PREF_TAB_QUEUE_COUNT).apply();
    }

    protected static void removeNotification(Context context) {
        NotificationManager notificationManager = (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        notificationManager.cancel(TAB_QUEUE_NOTIFICATION_ID);
    }

    public static boolean processTabQueuePromptResponse(int resultCode, Context context) {
        final SharedPreferences prefs = GeckoSharedPrefs.forApp(context);
        final SharedPreferences.Editor editor = prefs.edit();

        switch (resultCode) {
            case TAB_QUEUE_YES:
                editor.putBoolean(GeckoPreferences.PREFS_TAB_QUEUE, true);

                // By making this one more than EXTERNAL_LAUNCHES_BEFORE_SHOWING_PROMPT we ensure the prompt
                // will never show again without having to keep track of an extra pref.
                editor.putInt(TabQueueHelper.PREF_TAB_QUEUE_LAUNCHES,
                                     TabQueueHelper.EXTERNAL_LAUNCHES_BEFORE_SHOWING_PROMPT + 1);
                break;

            case TAB_QUEUE_NO:
                // The user clicked the 'no' button, so let's make sure the user never sees the prompt again by
                // maxing out the pref used to count the VIEW action intents received and times they've seen the prompt.

                editor.putInt(TabQueueHelper.PREF_TAB_QUEUE_LAUNCHES,
                                     TabQueueHelper.EXTERNAL_LAUNCHES_BEFORE_SHOWING_PROMPT + 1);

                editor.putInt(TabQueueHelper.PREF_TAB_QUEUE_TIMES_PROMPT_SHOWN,
                                     TabQueueHelper.MAX_TIMES_TO_SHOW_PROMPT + 1);
                break;

            default:
                // We shouldn't ever get here.
                Log.w(LOGTAG, "Unrecognized result code received from the tab queue prompt: " + resultCode);
        }

        editor.apply();

        return resultCode == TAB_QUEUE_YES;
    }

    public static boolean isTabQueueEnabled(Context context) {
        return isTabQueueEnabled(GeckoSharedPrefs.forApp(context));
    }

    public static boolean isTabQueueEnabled(SharedPreferences prefs) {
        return prefs.getBoolean(GeckoPreferences.PREFS_TAB_QUEUE, false);
    }
}
