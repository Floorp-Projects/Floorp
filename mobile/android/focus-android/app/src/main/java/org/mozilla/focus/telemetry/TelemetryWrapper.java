/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.telemetry;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.os.StrictMode;
import android.preference.PreferenceManager;
import android.support.annotation.CheckResult;

import org.mozilla.focus.BuildConfig;
import org.mozilla.focus.R;
import org.mozilla.focus.search.SearchEngine;
import org.mozilla.focus.search.SearchEngineManager;
import org.mozilla.focus.session.SessionManager;
import org.mozilla.focus.utils.AppConstants;
import org.mozilla.telemetry.Telemetry;
import org.mozilla.telemetry.TelemetryHolder;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.event.TelemetryEvent;
import org.mozilla.telemetry.measurement.DefaultSearchMeasurement;
import org.mozilla.telemetry.measurement.SearchesMeasurement;
import org.mozilla.telemetry.net.HttpURLConnectionTelemetryClient;
import org.mozilla.telemetry.net.TelemetryClient;
import org.mozilla.telemetry.ping.TelemetryCorePingBuilder;
import org.mozilla.telemetry.ping.TelemetryEventPingBuilder;
import org.mozilla.telemetry.schedule.TelemetryScheduler;
import org.mozilla.telemetry.schedule.jobscheduler.JobSchedulerTelemetryScheduler;
import org.mozilla.telemetry.serialize.JSONPingSerializer;
import org.mozilla.telemetry.serialize.TelemetryPingSerializer;
import org.mozilla.telemetry.storage.FileTelemetryStorage;
import org.mozilla.telemetry.storage.TelemetryStorage;

import java.util.List;

public final class TelemetryWrapper {
    private static final String TELEMETRY_APP_NAME_FOCUS = "Focus";
    private static final String TELEMETRY_APP_NAME_KLAR = "Klar";

    private TelemetryWrapper() {}

    private static class Category {
        private static final String ACTION = "action";
    }

    private static class Method {
        private static final String TYPE_URL = "type_url";
        private static final String TYPE_QUERY = "type_query";
        private static final String TYPE_SELECT_QUERY = "select_query";
        private static final String CLICK = "click";
        private static final String CANCEL = "cancel";
        private static final String LONG_PRESS = "long_press";
        private static final String CHANGE = "change";
        private static final String FOREGROUND = "foreground";
        private static final String BACKGROUND = "background";
        private static final String SHARE = "share";
        private static final String SAVE = "save";
        private static final String COPY = "copy";
        private static final String OPEN = "open";
        private static final String INSTALL = "install";
        private static final String INTENT_URL = "intent_url";
        private static final String INTENT_CUSTOM_TAB = "intent_custom_tab";
        private static final String TEXT_SELECTION_INTENT = "text_selection_intent";
        private static final String SHOW = "show";
        private static final String HIDE = "hide";
        private static final String SHARE_INTENT = "share_intent";
    }

    private static class Object {
        private static final String SEARCH_BAR = "search_bar";
        private static final String ERASE_BUTTON = "erase_button";
        private static final String SETTING = "setting";
        private static final String APP = "app";
        private static final String MENU = "menu";
        private static final String BACK_BUTTON = "back_button";
        private static final String NOTIFICATION = "notification";
        private static final String NOTIFICATION_ACTION = "notification_action";
        private static final String SHORTCUT = "shortcut";
        private static final String BLOCKING_SWITCH = "blocking_switch";
        private static final String BROWSER = "browser";
        private static final String BROWSER_CONTEXTMENU = "browser_contextmenu";
        private static final String CUSTOM_TAB_CLOSE_BUTTON = "custom_tab_close_but";
        private static final String CUSTOM_TAB_ACTION_BUTTON = "custom_tab_action_bu";
        private static final String FIRSTRUN = "firstrun";
        private static final String DOWNLOAD_DIALOG = "download_dialog";
        private static final String ADD_TO_HOMESCREEN_DIALOG = "add_to_homescreen_dialog";
        private static final String HOMESCREEN_SHORTCUT = "homescreen_shortcut";
        private static final String TABS_TRAY = "tabs_tray";
        private static final String RECENT_APPS = "recent_apps";
        private static final String APP_ICON = "app_icon";
    }

    private static class Value {
        private static final String DEFAULT = "default";
        private static final String FIREFOX = "firefox";
        private static final String SELECTION = "selection";
        private static final String ERASE = "erase";
        private static final String ERASE_AND_OPEN = "erase_open";
        private static final String ERASE_TO_HOME = "erase_home";
        private static final String ERASE_TO_APP = "erase_app";
        private static final String IMAGE = "image";
        private static final String LINK = "link";
        private static final String CUSTOM_TAB = "custom_tab";
        private static final String SKIP = "skip";
        private static final String FINISH = "finish";
        private static final String OPEN = "open";
        private static final String DOWNLOAD = "download";
        private static final String URL = "url";
        private static final String SEARCH = "search";
        private static final String CANCEL = "cancel";
        private static final String ADD_TO_HOMESCREEN = "add_to_homescreen";
        private static final String TAB = "tab";
        private static final String WHATS_NEW = "whats_new";
        private static final String RESUME = "resume";
    }

    private static class Extra {
        private static final String TO = "to";
        private static final String TOTAL = "total";
        private static final String SELECTED = "selected";
        private static final String HIGHLIGHTED = "highlighted";
    }

    public static boolean isTelemetryEnabled(Context context) {
        // The first access to shared preferences will require a disk read.
        final StrictMode.ThreadPolicy threadPolicy = StrictMode.allowThreadDiskReads();
        try {
            final Resources resources = context.getResources();
            final SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);

            return preferences.getBoolean(resources.getString(R.string.pref_key_telemetry), isEnabledByDefault())
                    && !AppConstants.isDevBuild();
        } finally {
            StrictMode.setThreadPolicy(threadPolicy);
        }
    }

    public static void setTelemetryEnabled(Context context, boolean enabled) {
        final Resources resources = context.getResources();
        final SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);

        preferences.edit()
                .putBoolean(resources.getString(R.string.pref_key_telemetry), enabled)
                .apply();

        TelemetryHolder.get()
                .getConfiguration()
                .setUploadEnabled(enabled)
                .setCollectionEnabled(enabled);
    }

    private static boolean isEnabledByDefault() {
        return !AppConstants.isKlarBuild();
    }

    public static void init(Context context) {
        // When initializing the telemetry library it will make sure that all directories exist and
        // are readable/writable.
        final StrictMode.ThreadPolicy threadPolicy = StrictMode.allowThreadDiskWrites();
        try {
            final Resources resources = context.getResources();

            final boolean telemetryEnabled = isTelemetryEnabled(context);

            final TelemetryConfiguration configuration = new TelemetryConfiguration(context)
                    .setServerEndpoint("https://incoming.telemetry.mozilla.org")
                    .setAppName(AppConstants.isKlarBuild() ? TELEMETRY_APP_NAME_KLAR : TELEMETRY_APP_NAME_FOCUS)
                    .setUpdateChannel(BuildConfig.BUILD_TYPE)
                    .setPreferencesImportantForTelemetry(
                            resources.getString(R.string.pref_key_search_engine),
                            resources.getString(R.string.pref_key_privacy_block_ads),
                            resources.getString(R.string.pref_key_privacy_block_analytics),
                            resources.getString(R.string.pref_key_privacy_block_social),
                            resources.getString(R.string.pref_key_privacy_block_other),
                            resources.getString(R.string.pref_key_performance_block_webfonts),
                            resources.getString(R.string.pref_key_locale),
                            resources.getString(R.string.pref_key_secure),
                            resources.getString(R.string.pref_key_default_browser))
                    .setSettingsProvider(new TelemetrySettingsProvider(context))
                    .setCollectionEnabled(telemetryEnabled)
                    .setUploadEnabled(telemetryEnabled);

            final TelemetryPingSerializer serializer = new JSONPingSerializer();
            final TelemetryStorage storage = new FileTelemetryStorage(configuration, serializer);
            final TelemetryClient client = new HttpURLConnectionTelemetryClient();
            final TelemetryScheduler scheduler = new JobSchedulerTelemetryScheduler();

            TelemetryHolder.set(new Telemetry(configuration, storage, client, scheduler)
                    .addPingBuilder(new TelemetryCorePingBuilder(configuration))
                    .addPingBuilder(new TelemetryEventPingBuilder(configuration))
                    .setDefaultSearchProvider(createDefaultSearchProvider(context)));
        } finally {
            StrictMode.setThreadPolicy(threadPolicy);
        }
    }

    private static DefaultSearchMeasurement.DefaultSearchEngineProvider createDefaultSearchProvider(final Context context) {
        return new DefaultSearchMeasurement.DefaultSearchEngineProvider() {
            @Override
            public String getDefaultSearchEngineIdentifier() {
                return SearchEngineManager.getInstance()
                        .getDefaultSearchEngine(context)
                        .getIdentifier();
            }
        };
    }

    /**
     * Add the position of the current session and total number of sessions as extras to the event
     * and return it.
     */
    @CheckResult
    private static TelemetryEvent withSessionCounts(TelemetryEvent event) {
        final SessionManager sessionManager = SessionManager.getInstance();

        event.extra(Extra.SELECTED, String.valueOf(sessionManager.getPositionOfCurrentSession()));
        event.extra(Extra.TOTAL, String.valueOf(sessionManager.getNumberOfSessions()));

        return event;
    }

    public static void startSession() {
        TelemetryHolder.get().recordSessionStart();

        TelemetryEvent.create(Category.ACTION, Method.FOREGROUND, Object.APP).queue();
    }

    public static void stopSession() {
        TelemetryHolder.get().recordSessionEnd();

        TelemetryEvent.create(Category.ACTION, Method.BACKGROUND, Object.APP).queue();
    }

    public static void stopMainActivity() {
        TelemetryHolder.get()
                .queuePing(TelemetryCorePingBuilder.TYPE)
                .queuePing(TelemetryEventPingBuilder.TYPE)
                .scheduleUpload();
    }

    public static void urlBarEvent(boolean isUrl) {
        if (isUrl) {
            TelemetryWrapper.browseEvent();
        } else {
            TelemetryWrapper.searchEnterEvent();
        }
    }

    private static void browseEvent() {
        TelemetryEvent.create(Category.ACTION, Method.TYPE_URL, Object.SEARCH_BAR).queue();
    }

    /* package */ static void browseIntentEvent() {
        TelemetryEvent.create(Category.ACTION, Method.INTENT_URL, Object.APP).queue();
    }

    /* package */ static void shareIntentEvent(boolean isSearch) {
        if (isSearch) {
            TelemetryEvent.create(Category.ACTION, Method.SHARE_INTENT, Object.APP, Value.SEARCH).queue();
        } else {
            TelemetryEvent.create(Category.ACTION, Method.SHARE_INTENT, Object.APP, Value.URL).queue();
        }
    }

    /**
     * Sends a list of the custom tab options that a custom-tab intent made use of.
     */
    /* package */ static void customTabsIntentEvent(final List<String> options) {
        final TelemetryEvent event = TelemetryEvent.create(Category.ACTION, Method.INTENT_CUSTOM_TAB, Object.APP);

        // We can send at most 10 extras per event - we just ignore the rest if there are too many
        final int extrasCount;
        if (options.size() > 10) {
            extrasCount = 10;
        } else {
            extrasCount = options.size();
        }

        for (final String option : options.subList(0, extrasCount)) {
            event.extra(option, "true");
        }

        event.queue();
    }

    public static void downloadDialogDownloadEvent(boolean sentToDownload) {
        if (sentToDownload) {
            TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.DOWNLOAD_DIALOG, Value.DOWNLOAD).queue();
        } else {
            TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.DOWNLOAD_DIALOG, Value.CANCEL).queue();
        }
    }

    public static void closeCustomTabEvent() {
        withSessionCounts(TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.CUSTOM_TAB_CLOSE_BUTTON))
                .queue();
    }

    public static void customTabActionButtonEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.CUSTOM_TAB_ACTION_BUTTON).queue();
    }

    public static void customTabMenuEvent() {
        TelemetryEvent.create(Category.ACTION, Method.OPEN, Object.MENU, Value.CUSTOM_TAB).queue();
    }

    /* package */ static void textSelectionIntentEvent() {
        TelemetryEvent.create(Category.ACTION, Method.TEXT_SELECTION_INTENT, Object.APP).queue();
    }

    private static void searchEnterEvent() {
        Telemetry telemetry = TelemetryHolder.get();

        TelemetryEvent.create(Category.ACTION, Method.TYPE_QUERY, Object.SEARCH_BAR).queue();

        SearchEngine searchEngine = SearchEngineManager.getInstance().getDefaultSearchEngine(
            telemetry.getConfiguration().getContext());

        telemetry.recordSearch(SearchesMeasurement.LOCATION_ACTIONBAR, searchEngine.getIdentifier());
    }

    public static void searchSelectEvent() {
        Telemetry telemetry = TelemetryHolder.get();

        TelemetryEvent.create(Category.ACTION, Method.TYPE_SELECT_QUERY, Object.SEARCH_BAR).queue();

        SearchEngine searchEngine = SearchEngineManager.getInstance().getDefaultSearchEngine(
                telemetry.getConfiguration().getContext());

        telemetry.recordSearch(SearchesMeasurement.LOCATION_SUGGESTION, searchEngine.getIdentifier());
    }

    public static void eraseEvent() {
        withSessionCounts(TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.ERASE_BUTTON))
                .queue();
    }

    public static void eraseBackToHomeEvent() {
        withSessionCounts(TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.BACK_BUTTON, Value.ERASE_TO_HOME))
                .queue();
    }

    public static void eraseBackToAppEvent() {
        withSessionCounts(TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.BACK_BUTTON, Value.ERASE_TO_APP))
                .queue();
    }

    public static void eraseNotificationEvent() {
        withSessionCounts(TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.NOTIFICATION, Value.ERASE))
                .queue();
    }

    public static void eraseAndOpenNotificationActionEvent() {
        withSessionCounts(TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.NOTIFICATION_ACTION, Value.ERASE_AND_OPEN))
                .queue();
    }

    public static void openNotificationActionEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.NOTIFICATION_ACTION, Value.OPEN).queue();
    }

    /* package */ static void openHomescreenShortcutEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.HOMESCREEN_SHORTCUT, Value.OPEN).queue();
    }

    public static void addToHomescreenShortcutEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.ADD_TO_HOMESCREEN_DIALOG, Value.ADD_TO_HOMESCREEN).queue();
    }

    public static void cancelAddToHomescreenShortcutEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.ADD_TO_HOMESCREEN_DIALOG, Value.CANCEL).queue();
    }

    public static void eraseShortcutEvent() {
        withSessionCounts(TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.SHORTCUT, Value.ERASE))
                .queue();
    }

    public static void eraseTaskRemoved() {
        withSessionCounts(TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.RECENT_APPS, Value.ERASE))
                .queue();
    }

    public static void settingsEvent(String key, String value) {
        TelemetryEvent.create(Category.ACTION, Method.CHANGE, Object.SETTING, key)
                .extra(Extra.TO, value)
                .queue();
    }

    public static void shareEvent() {
        TelemetryEvent.create(Category.ACTION, Method.SHARE, Object.MENU).queue();
    }

    public static void shareLinkEvent() {
        TelemetryEvent.create(Category.ACTION, Method.SHARE, Object.BROWSER_CONTEXTMENU, Value.LINK).queue();
    }

    public static void shareImageEvent() {
        TelemetryEvent.create(Category.ACTION, Method.SHARE, Object.BROWSER_CONTEXTMENU, Value.IMAGE).queue();
    }

    public static void saveImageEvent() {
        TelemetryEvent.create(Category.ACTION, Method.SAVE, Object.BROWSER_CONTEXTMENU, Value.IMAGE).queue();
    }

    public static void copyLinkEvent() {
        TelemetryEvent.create(Category.ACTION, Method.COPY, Object.BROWSER_CONTEXTMENU, Value.LINK).queue();
    }

    public static void copyImageEvent() {
        TelemetryEvent.create(Category.ACTION, Method.COPY, Object.BROWSER_CONTEXTMENU, Value.IMAGE).queue();
    }

    public static void openLinkInNewTabEvent() {
        withSessionCounts(TelemetryEvent.create(Category.ACTION, Method.OPEN, Object.BROWSER_CONTEXTMENU, Value.TAB))
                .queue();
    }

    public static void openWebContextMenuEvent() {
        TelemetryEvent.create(Category.ACTION, Method.LONG_PRESS, Object.BROWSER).queue();
    }

    public static void cancelWebContextMenuEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CANCEL, Object.BROWSER_CONTEXTMENU).queue();
    }

    public static void openDefaultAppEvent() {
        TelemetryEvent.create(Category.ACTION, Method.OPEN, Object.MENU, Value.DEFAULT).queue();
    }

    public static void openFromIconEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.APP_ICON, Value.OPEN).queue();
    }

    public static void resumeFromIconEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.APP_ICON, Value.RESUME).queue();
    }

    public static void openFirefoxEvent() {
        TelemetryEvent.create(Category.ACTION, Method.OPEN, Object.MENU, Value.FIREFOX).queue();
    }

    public static void installFirefoxEvent() {
        TelemetryEvent.create(Category.ACTION, Method.INSTALL, Object.APP, Value.FIREFOX).queue();
    }

    public static void openSelectionEvent() {
        TelemetryEvent.create(Category.ACTION, Method.OPEN, Object.MENU, Value.SELECTION).queue();
    }

    public static void blockingSwitchEvent(boolean isBlockingEnabled) {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.BLOCKING_SWITCH, String.valueOf(isBlockingEnabled)).queue();
    }

    public static void showFirstRunPageEvent(int page) {
        TelemetryEvent.create(Category.ACTION, Method.SHOW, Object.FIRSTRUN, String.valueOf(page)).queue();
    }

    public static void skipFirstRunEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.FIRSTRUN, Value.SKIP).queue();
    }

    public static void finishFirstRunEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.FIRSTRUN, Value.FINISH).queue();
    }

    public static void openTabsTrayEvent() {
        TelemetryEvent.create(Category.ACTION, Method.SHOW, Object.TABS_TRAY).queue();
    }

    public static void openWhatsNewEvent(boolean highlighted) {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.MENU, Value.WHATS_NEW)
                .extra(Extra.HIGHLIGHTED, String.valueOf(highlighted))
                .queue();
    }

    public static void closeTabsTrayEvent() {
        TelemetryEvent.create(Category.ACTION, Method.HIDE, Object.TABS_TRAY).queue();
    }

    public static void switchTabInTabsTrayEvent() {
        withSessionCounts(TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.TABS_TRAY, Value.TAB))
                .queue();
    }

    public static void eraseInTabsTrayEvent() {
        withSessionCounts(TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.TABS_TRAY, Value.ERASE))
                .queue();
    }
}
