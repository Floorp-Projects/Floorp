/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We haven't migrated from TelemetryEventPingBuilder to MobileEventPingBuilder yet.
@file:Suppress("DEPRECATION")

package org.mozilla.focus.telemetry

import android.content.Context
import android.os.Build
import android.os.StrictMode
import androidx.annotation.CheckResult
import androidx.annotation.VisibleForTesting
import androidx.preference.PreferenceManager
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.selectedOrDefaultSearchEngine
import org.json.JSONObject
import org.mozilla.focus.BuildConfig
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import org.mozilla.focus.utils.AppConstants
import org.mozilla.focus.utils.MobileMetricsPingStorage
import org.mozilla.focus.utils.UrlUtils
import org.mozilla.telemetry.Telemetry
import org.mozilla.telemetry.TelemetryHolder
import org.mozilla.telemetry.config.TelemetryConfiguration
import org.mozilla.telemetry.event.TelemetryEvent
import org.mozilla.telemetry.measurement.DefaultSearchMeasurement
import org.mozilla.telemetry.measurement.SearchesMeasurement
import org.mozilla.telemetry.net.TelemetryClient
import org.mozilla.telemetry.ping.TelemetryCorePingBuilder
import org.mozilla.telemetry.ping.TelemetryEventPingBuilder
import org.mozilla.telemetry.ping.TelemetryMobileMetricsPingBuilder
import org.mozilla.telemetry.schedule.jobscheduler.JobSchedulerTelemetryScheduler
import org.mozilla.telemetry.serialize.JSONPingSerializer
import org.mozilla.telemetry.storage.FileTelemetryStorage
import java.net.MalformedURLException
import java.net.URL
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@Suppress(
    // Yes, this a large class with a lot of functions. But it's very simple and still easy to read.
    "TooManyFunctions",
    "LargeClass"
)
object TelemetryWrapper {
    private const val TELEMETRY_APP_NAME_FOCUS = "Focus"
    private const val TELEMETRY_APP_NAME_KLAR = "Klar"
    private const val LAST_MOBILE_METRICS_PINGS = "LAST_MOBILE_METRICS_PINGS"

    private val dateFormat = SimpleDateFormat("yyyyMMdd", Locale.US)

    private const val HISTOGRAM_SIZE = 200
    private const val BUCKET_SIZE_MS = 100
    private const val HISTOGRAM_MIN_INDEX = 0

    private val isEnabledByDefault: Boolean
        get() = !AppConstants.isKlarBuild

    private object Category {
        const val ACTION = "action"
        const val HISTOGRAM = "histogram"
    }

    private object Method {
        const val TYPE_URL = "type_url"
        const val TYPE_QUERY = "type_query"
        const val TYPE_SELECT_QUERY = "select_query"
        const val CLICK = "click"
        const val CHANGE = "change"
        const val FOREGROUND = "foreground"
        const val BACKGROUND = "background"
        const val SHARE = "share"
        const val SAVE = "save"
        const val COPY = "copy"
        const val OPEN = "open"
        const val INSTALL = "install"
        const val INTENT_URL = "intent_url"
        const val TEXT_SELECTION_INTENT = "text_selection_intent"
        const val SHOW = "show"
        const val HIDE = "hide"
        const val SHARE_INTENT = "share_intent"
        const val REMOVE = "remove"
        const val REMOVE_ALL = "remove_all"
        const val REORDER = "reorder"
        const val RESTORE = "restore"
    }

    private object Object {
        const val SEARCH_BAR = "search_bar"
        const val ERASE_BUTTON = "erase_button"
        const val SETTING = "setting"
        const val APP = "app"
        const val MENU = "menu"
        const val BACK_BUTTON = "back_button"
        const val NOTIFICATION = "notification"
        const val NOTIFICATION_ACTION = "notification_action"
        const val SHORTCUT = "shortcut"
        const val DESKTOP_REQUEST_CHECK = "desktop_request_check"
        const val BROWSER = "browser"
        const val BROWSER_CONTEXTMENU = "browser_contextmenu"
        const val CUSTOM_TAB_CLOSE_BUTTON = "custom_tab_close_but"
        const val CUSTOM_TAB_ACTION_BUTTON = "custom_tab_action_bu"
        const val FIRSTRUN = "firstrun"
        const val DOWNLOAD_DIALOG = "download_dialog"
        const val ADD_TO_HOMESCREEN_DIALOG = "add_to_homescreen_dialog"
        const val HOMESCREEN_SHORTCUT = "homescreen_shortcut"
        const val TABS_TRAY = "tabs_tray"
        const val RECENT_APPS = "recent_apps"
        const val APP_ICON = "app_icon"
        const val AUTOCOMPLETE_DOMAIN = "autocomplete_domain"
        const val SEARCH_ENGINE_SETTING = "search_engine_setting"
        const val ADD_SEARCH_ENGINE_LEARN_MORE = "search_engine_learn_more"
        const val CUSTOM_SEARCH_ENGINE = "custom_search_engine"
        const val REMOVE_SEARCH_ENGINES = "remove_search_engines"
        const val SEARCH_SUGGESTION_PROMPT = "search_suggestion_prompt"
        const val MAKE_DEFAULT_BROWSER_OPEN_WITH = "make_default_browser_open_with"
        const val MAKE_DEFAULT_BROWSER_SETTINGS = "make_default_browser_settings"
        const val ALLOWLIST = "allowlist"
        const val CRASH_REPORTER = "crash_reporter"
    }

    private object Value {
        const val FIREFOX = "firefox"
        const val SELECTION = "selection"
        const val ERASE = "erase"
        const val ERASE_AND_OPEN = "erase_open"
        const val ERASE_TO_HOME = "erase_home"
        const val ERASE_TO_APP = "erase_app"
        const val IMAGE = "image"
        const val LINK = "link"
        const val CUSTOM_TAB = "custom_tab"
        const val SKIP = "skip"
        const val FINISH = "finish"
        const val OPEN = "open"
        const val DOWNLOAD = "download"
        const val URL = "url"
        const val SEARCH = "search"
        const val CANCEL = "cancel"
        const val ADD_TO_HOMESCREEN = "add_to_homescreen"
        const val TAB = "tab"
        const val WHATS_NEW = "whats_new"
        const val RESUME = "resume"
        const val RELOAD = "refresh"
        const val FULL_BROWSER = "full_browser"
        const val REPORT_ISSUE = "report_issue"
        const val SETTINGS = "settings"
        const val FIND_IN_PAGE = "find_in_page"
        const val CLOSE_TAB = "close_tab"
    }

    private object Extra {
        const val FROM = "from"
        const val TO = "to"
        const val TOTAL = "total"
        const val SELECTED = "selected"
        const val HIGHLIGHTED = "highlighted"
        const val SOURCE = "source"
        const val SUCCESS = "success"
        const val SEARCH_SUGGESTION = "search_suggestion"
        const val TOTAL_URI_COUNT = "total_uri_count"
        const val UNIQUE_DOMAINS_COUNT = "unique_domains_count"
        const val SUBMIT_CRASH = "submit_crash"
    }

    @JvmStatic
    fun isTelemetryEnabled(context: Context): Boolean {
        if (isDeviceWithTelemetryDisabled()) { return false }

        // The first access to shared preferences will require a disk read.
        val threadPolicy = StrictMode.allowThreadDiskReads()
        try {
            val resources = context.resources
            val preferences = PreferenceManager.getDefaultSharedPreferences(context)

            return preferences.getBoolean(
                resources.getString(R.string.pref_key_telemetry), isEnabledByDefault
            ) && !AppConstants.isDevBuild
        } finally {
            StrictMode.setThreadPolicy(threadPolicy)
        }
    }

    @JvmStatic
    @Suppress("LongMethod")
    fun init(context: Context) {
        // When initializing the telemetry library it will make sure that all directories exist and
        // are readable/writable.
        val threadPolicy = StrictMode.allowThreadDiskWrites()
        try {
            val resources = context.resources

            val telemetryEnabled = isTelemetryEnabled(context)

            val configuration = TelemetryConfiguration(context)
                .setServerEndpoint("https://incoming.telemetry.mozilla.org")
                .setAppName(if (AppConstants.isKlarBuild) TELEMETRY_APP_NAME_KLAR else TELEMETRY_APP_NAME_FOCUS)
                .setUpdateChannel(BuildConfig.BUILD_TYPE)
                .setPreferencesImportantForTelemetry(
                    resources.getString(R.string.pref_key_search_engine),
                    resources.getString(R.string.pref_key_privacy_block_ads),
                    resources.getString(R.string.pref_key_privacy_block_analytics),
                    resources.getString(R.string.pref_key_privacy_block_social),
                    resources.getString(R.string.pref_key_privacy_block_other3),
                    resources.getString(R.string.pref_key_performance_block_javascript),
                    resources.getString(R.string.pref_key_performance_enable_cookies),
                    resources.getString(R.string.pref_key_performance_block_webfonts),
                    resources.getString(R.string.pref_key_locale),
                    resources.getString(R.string.pref_key_secure),
                    resources.getString(R.string.pref_key_default_browser),
                    resources.getString(R.string.pref_key_autocomplete_preinstalled),
                    resources.getString(R.string.pref_key_autocomplete_custom),
                    resources.getString(R.string.pref_key_remote_debugging),
                    resources.getString(R.string.pref_key_show_search_suggestions)
                )
                .setSettingsProvider(TelemetrySettingsProvider(context))
                .setCollectionEnabled(telemetryEnabled)
                .setUploadEnabled(telemetryEnabled)
                .setBuildId(TelemetryConfiguration(context).buildId)

            val serializer = JSONPingSerializer()
            val storage = FileTelemetryStorage(configuration, serializer)
            val client = TelemetryClient(context.components.client.unwrap())
            val scheduler = JobSchedulerTelemetryScheduler()

            TelemetryHolder.set(
                Telemetry(configuration, storage, client, scheduler)
                    .addPingBuilder(TelemetryCorePingBuilder(configuration))
                    .addPingBuilder(TelemetryEventPingBuilder(configuration))
                    .also {
                        if (!dayPassedSinceLastUpload(context)) return@also

                        runBlocking {
                            val metricsStorage = MobileMetricsPingStorage(context)
                            val mobileMetrics = metricsStorage.load() ?: JSONObject()
                            metricsStorage.clearStorage()

                            it.addPingBuilder(
                                TelemetryMobileMetricsPingBuilder(
                                    mobileMetrics,
                                    configuration
                                )
                            )
                        }

                        // Record new edited date
                        PreferenceManager.getDefaultSharedPreferences(context)
                            .edit()
                            .putLong(LAST_MOBILE_METRICS_PINGS, (dateFormat.format(Date()).toLong()))
                            .apply()
                    }
                    .setDefaultSearchProvider(createDefaultSearchProvider(context))
            )
        } finally {
            StrictMode.setThreadPolicy(threadPolicy)
        }
    }

    val clientId: String
        get() = TelemetryHolder.get().clientId

    private fun createDefaultSearchProvider(context: Context): DefaultSearchMeasurement.DefaultSearchEngineProvider {
        return DefaultSearchMeasurement.DefaultSearchEngineProvider {
            runBlocking {
                getDefaultSearchEngineIdentifierForTelemetry(context)
            }
        }
    }

    /**
     * Add the position of the current session and total number of sessions as extras to the event
     * and return it.
     */
    @CheckResult
    private fun withSessionCounts(event: TelemetryEvent): TelemetryEvent {
        val context = TelemetryHolder.get().configuration.context

        val state = context.components.store.state
        val privateTabs = state.privateTabs
        val positionOfSelectedTab = privateTabs.indexOfFirst { tab -> tab.id == state.selectedTabId }

        event.extra(Extra.SELECTED, positionOfSelectedTab.toString())
        event.extra(Extra.TOTAL, privateTabs.size.toString())

        return event
    }

    @JvmStatic
    fun startSession() {
        TelemetryHolder.get().recordSessionStart()

        TelemetryEvent.create(Category.ACTION, Method.FOREGROUND, Object.APP).queue()
    }

    @VisibleForTesting var histogram = IntArray(HISTOGRAM_SIZE)
    @VisibleForTesting var domainMap = HashSet<String>()
    @VisibleForTesting var numUri = 0

    @JvmStatic
    fun addLoadToHistogram(url: String, newLoadTime: Long) {
        try {
            domainMap.add(UrlUtils.stripCommonSubdomains(URL(url).host))
            numUri++
            var histogramLoadIndex = (newLoadTime / BUCKET_SIZE_MS).toInt()

            if (histogramLoadIndex > (HISTOGRAM_SIZE - 2)) {
                histogramLoadIndex = HISTOGRAM_SIZE - 1
            } else if (histogramLoadIndex < HISTOGRAM_MIN_INDEX) {
                histogramLoadIndex = HISTOGRAM_MIN_INDEX
            }

            histogram[histogramLoadIndex]++
        } catch (e: MalformedURLException) {
            // ignore invalid URLs
        }
    }

    @JvmStatic
    fun stopSession() {
        TelemetryHolder.get().recordSessionEnd()

        val histogramEvent = TelemetryEvent.create(Category.HISTOGRAM, Method.FOREGROUND, Object.BROWSER)
        for (bucketIndex in histogram.indices) {
            histogramEvent.extra((bucketIndex * BUCKET_SIZE_MS).toString(), histogram[bucketIndex].toString())
        }
        histogramEvent.queue()

        // Clear histogram array after queueing it
        histogram = IntArray(HISTOGRAM_SIZE)

        TelemetryEvent.create(Category.ACTION, Method.OPEN, Object.BROWSER).extra(
            Extra.UNIQUE_DOMAINS_COUNT,
            domainMap.size.toString()
        ).queue()
        domainMap.clear()

        TelemetryEvent.create(Category.ACTION, Method.OPEN, Object.BROWSER).extra(
            Extra.TOTAL_URI_COUNT,
            numUri.toString()
        ).queue()
        numUri = 0

        TelemetryEvent.create(Category.ACTION, Method.BACKGROUND, Object.APP).queue()
    }

    @JvmStatic
    fun stopMainActivity() {
        TelemetryHolder.get()
            .queuePing(TelemetryCorePingBuilder.TYPE)
            .queuePing(TelemetryEventPingBuilder.TYPE)
            .queuePing(TelemetryMobileMetricsPingBuilder.TYPE)
            .scheduleUpload()
    }

    @JvmStatic
    fun urlBarEvent(isUrl: Boolean) {
        if (isUrl) {
            browseEvent()
        } else {
            searchEnterEvent()
        }
    }

    private fun browseEvent() {
        TelemetryEvent.create(Category.ACTION, Method.TYPE_URL, Object.SEARCH_BAR)
            .queue()
    }

    @JvmStatic
    fun browseIntentEvent() {
        TelemetryEvent.create(Category.ACTION, Method.INTENT_URL, Object.APP).queue()
    }

    @JvmStatic
    fun shareIntentEvent(isSearch: Boolean) {
        if (isSearch) {
            TelemetryEvent.create(Category.ACTION, Method.SHARE_INTENT, Object.APP, Value.SEARCH).queue()
        } else {
            TelemetryEvent.create(Category.ACTION, Method.SHARE_INTENT, Object.APP, Value.URL).queue()
        }
    }

    @JvmStatic
    fun downloadDialogDownloadEvent(sentToDownload: Boolean) {
        if (sentToDownload) {
            TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.DOWNLOAD_DIALOG, Value.DOWNLOAD).queue()
        } else {
            TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.DOWNLOAD_DIALOG, Value.CANCEL).queue()
        }
    }

    @JvmStatic
    fun closeCustomTabEvent() {
        withSessionCounts(TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.CUSTOM_TAB_CLOSE_BUTTON))
            .queue()
    }

    @JvmStatic
    fun customTabActionButtonEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.CUSTOM_TAB_ACTION_BUTTON).queue()
    }

    @JvmStatic
    fun customTabMenuEvent() {
        TelemetryEvent.create(Category.ACTION, Method.OPEN, Object.MENU, Value.CUSTOM_TAB).queue()
    }

    @JvmStatic
    fun textSelectionIntentEvent() {
        TelemetryEvent.create(Category.ACTION, Method.TEXT_SELECTION_INTENT, Object.APP).queue()
    }

    fun searchEnterEvent() {
        val telemetry = TelemetryHolder.get()

        TelemetryEvent.create(Category.ACTION, Method.TYPE_QUERY, Object.SEARCH_BAR).queue()

        val searchEngine = getDefaultSearchEngineIdentifierForTelemetry(telemetry.configuration.context)

        telemetry.recordSearch(SearchesMeasurement.LOCATION_ACTIONBAR, searchEngine)
    }

    fun searchSelectEvent(isSearchSuggestion: Boolean) {
        val telemetry = TelemetryHolder.get()

        TelemetryEvent
            .create(Category.ACTION, Method.TYPE_SELECT_QUERY, Object.SEARCH_BAR)
            .extra(Extra.SEARCH_SUGGESTION, "$isSearchSuggestion")
            .queue()

        val searchEngineIdentifier = getDefaultSearchEngineIdentifierForTelemetry(telemetry.configuration.context)

        telemetry.recordSearch(SearchesMeasurement.LOCATION_SUGGESTION, searchEngineIdentifier)
    }

    private fun getDefaultSearchEngineIdentifierForTelemetry(context: Context): String {
        val searchEngine = context.components.store.state.search.selectedOrDefaultSearchEngine
        return if (searchEngine?.type == SearchEngine.Type.CUSTOM) {
            "custom"
        } else {
            searchEngine?.name ?: "<none>"
        }
    }

    @JvmStatic
    fun eraseEvent() {
        withSessionCounts(TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.ERASE_BUTTON))
            .queue()
    }

    @JvmStatic
    fun eraseBackToHomeEvent() {
        withSessionCounts(TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.BACK_BUTTON, Value.ERASE_TO_HOME))
            .queue()
    }

    @JvmStatic
    fun eraseBackToAppEvent() {
        withSessionCounts(TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.BACK_BUTTON, Value.ERASE_TO_APP))
            .queue()
    }

    @JvmStatic
    fun eraseNotificationEvent() {
        withSessionCounts(TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.NOTIFICATION, Value.ERASE))
            .queue()
    }

    @JvmStatic
    fun eraseAndOpenNotificationActionEvent() {
        withSessionCounts(
            TelemetryEvent.create(
                Category.ACTION,
                Method.CLICK,
                Object.NOTIFICATION_ACTION,
                Value.ERASE_AND_OPEN
            )
        ).queue()
    }

    @JvmStatic
    fun crashReporterOpened() {
        TelemetryEvent.create(Category.ACTION, Method.SHOW, Object.CRASH_REPORTER).queue()
    }

    @JvmStatic
    fun closeTabButtonTapped(crashSubmitted: Boolean) {
        TelemetryEvent.create(
            Category.ACTION,
            Method.CLICK,
            Object.CRASH_REPORTER,
            Value.CLOSE_TAB
        )
            .extra(Extra.SUBMIT_CRASH, crashSubmitted.toString()).queue()
    }

    @JvmStatic
    fun openNotificationActionEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.NOTIFICATION_ACTION, Value.OPEN).queue()
    }

    @JvmStatic
    fun openHomescreenShortcutEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.HOMESCREEN_SHORTCUT, Value.OPEN).queue()
    }

    @JvmStatic
    fun addToHomescreenShortcutEvent() {
        TelemetryEvent.create(
            Category.ACTION,
            Method.CLICK,
            Object.ADD_TO_HOMESCREEN_DIALOG,
            Value.ADD_TO_HOMESCREEN
        ).queue()
    }

    @JvmStatic
    fun cancelAddToHomescreenShortcutEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.ADD_TO_HOMESCREEN_DIALOG, Value.CANCEL).queue()
    }

    @JvmStatic
    fun eraseShortcutEvent() {
        withSessionCounts(TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.SHORTCUT, Value.ERASE))
            .queue()
    }

    @JvmStatic
    fun eraseAndOpenShortcutEvent() {
        withSessionCounts(TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.SHORTCUT, Value.ERASE_AND_OPEN))
            .queue()
    }

    @JvmStatic
    fun eraseTaskRemoved() {
        withSessionCounts(TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.RECENT_APPS, Value.ERASE))
            .queue()
    }

    @JvmStatic
    fun settingsEvent(key: String, value: String) {
        TelemetryEvent.create(Category.ACTION, Method.CHANGE, Object.SETTING, key)
            .extra(Extra.TO, value)
            .queue()
    }

    @JvmStatic
    fun shareEvent() {
        TelemetryEvent.create(Category.ACTION, Method.SHARE, Object.MENU).queue()
    }

    @JvmStatic
    fun shareLinkEvent() {
        TelemetryEvent.create(Category.ACTION, Method.SHARE, Object.BROWSER_CONTEXTMENU, Value.LINK).queue()
    }

    @JvmStatic
    fun shareImageEvent() {
        TelemetryEvent.create(Category.ACTION, Method.SHARE, Object.BROWSER_CONTEXTMENU, Value.IMAGE).queue()
    }

    @JvmStatic
    fun saveImageEvent() {
        TelemetryEvent.create(Category.ACTION, Method.SAVE, Object.BROWSER_CONTEXTMENU, Value.IMAGE).queue()
    }

    @JvmStatic
    fun copyLinkEvent() {
        TelemetryEvent.create(Category.ACTION, Method.COPY, Object.BROWSER_CONTEXTMENU, Value.LINK).queue()
    }

    @JvmStatic
    fun copyImageEvent() {
        TelemetryEvent.create(Category.ACTION, Method.COPY, Object.BROWSER_CONTEXTMENU, Value.IMAGE).queue()
    }

    @JvmStatic
    fun openLinkInNewTabEvent() {
        withSessionCounts(TelemetryEvent.create(Category.ACTION, Method.OPEN, Object.BROWSER_CONTEXTMENU, Value.TAB))
            .queue()
    }

    @JvmStatic
    fun openLinkInFullBrowserFromCustomTabEvent() {
        withSessionCounts(
            TelemetryEvent.create(
                Category.ACTION,
                Method.OPEN,
                Object.BROWSER_CONTEXTMENU,
                Value.FULL_BROWSER
            )
        )
            .queue()
    }

    /**
     * Switching from a custom tab to the full-featured browser (regular tab).
     */
    @JvmStatic
    fun openFullBrowser() {
        TelemetryEvent.create(Category.ACTION, Method.OPEN, Object.MENU, Value.FULL_BROWSER).queue()
    }

    @JvmStatic
    fun openFromIconEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.APP_ICON, Value.OPEN).queue()
    }

    @JvmStatic
    fun resumeFromIconEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.APP_ICON, Value.RESUME).queue()
    }

    @JvmStatic
    fun openFirefoxEvent() {
        TelemetryEvent.create(Category.ACTION, Method.OPEN, Object.MENU, Value.FIREFOX).queue()
    }

    @JvmStatic
    fun installFirefoxEvent() {
        TelemetryEvent.create(Category.ACTION, Method.INSTALL, Object.APP, Value.FIREFOX).queue()
    }

    @JvmStatic
    fun openSelectionEvent() {
        TelemetryEvent.create(Category.ACTION, Method.OPEN, Object.MENU, Value.SELECTION).queue()
    }

    @JvmStatic
    fun openExceptionsListSetting() {
        TelemetryEvent.create(Category.ACTION, Method.OPEN, Object.ALLOWLIST).queue()
    }

    fun removeExceptionDomains(count: Int) {
        TelemetryEvent.create(Category.ACTION, Method.REMOVE, Object.ALLOWLIST)
            .extra(Extra.TOTAL, count.toString())
            .queue()
    }

    fun removeAllExceptionDomains(count: Int) {
        TelemetryEvent.create(Category.ACTION, Method.REMOVE_ALL, Object.ALLOWLIST)
            .extra(Extra.TOTAL, count.toString())
            .queue()
    }

    @JvmStatic
    fun desktopRequestCheckEvent(shouldRequestDesktop: Boolean) {
        TelemetryEvent.create(
            Category.ACTION,
            Method.CLICK,
            Object.DESKTOP_REQUEST_CHECK,
            shouldRequestDesktop.toString()
        ).queue()
    }

    @JvmStatic
    fun showFirstRunPageEvent(page: Int) {
        TelemetryEvent.create(Category.ACTION, Method.SHOW, Object.FIRSTRUN, page.toString()).queue()
    }

    @JvmStatic
    fun skipFirstRunEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.FIRSTRUN, Value.SKIP).queue()
    }

    @JvmStatic
    fun finishFirstRunEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.FIRSTRUN, Value.FINISH).queue()
    }

    @JvmStatic
    fun openTabsTrayEvent() {
        TelemetryEvent.create(Category.ACTION, Method.SHOW, Object.TABS_TRAY).queue()
    }

    @JvmStatic
    fun openWhatsNewEvent(highlighted: Boolean) {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.SETTING, Value.WHATS_NEW)
            .extra(Extra.HIGHLIGHTED, highlighted.toString())
            .queue()
    }

    @JvmStatic
    fun findInPageMenuEvent() {
        TelemetryEvent.create(Category.ACTION, Method.OPEN, Object.MENU, Value.FIND_IN_PAGE).queue()
    }

    @JvmStatic
    fun closeTabsTrayEvent() {
        TelemetryEvent.create(Category.ACTION, Method.HIDE, Object.TABS_TRAY).queue()
    }

    @JvmStatic
    fun menuReloadEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.MENU, Value.RELOAD).queue()
    }

    enum class AutoCompleteEventSource {
        SETTINGS
    }

    fun saveAutocompleteDomainEvent(eventSource: AutoCompleteEventSource) {
        val source = when (eventSource) {
            AutoCompleteEventSource.SETTINGS -> Value.SETTINGS
        }

        TelemetryEvent.create(Category.ACTION, Method.SAVE, Object.AUTOCOMPLETE_DOMAIN)
            .extra(Extra.SOURCE, source)
            .queue()
    }

    fun removeAutocompleteDomainsEvent(count: Int) {
        TelemetryEvent.create(Category.ACTION, Method.REMOVE, Object.AUTOCOMPLETE_DOMAIN)
            .extra(Extra.TOTAL, count.toString())
            .queue()
    }

    fun reorderAutocompleteDomainEvent(from: Int, to: Int) {
        TelemetryEvent.create(Category.ACTION, Method.REORDER, Object.AUTOCOMPLETE_DOMAIN)
            .extra(Extra.FROM, from.toString())
            .extra(Extra.TO, to.toString())
            .queue()
    }

    @JvmStatic
    fun setDefaultSearchEngineEvent(source: String) {
        TelemetryEvent.create(Category.ACTION, Method.SAVE, Object.SEARCH_ENGINE_SETTING)
            .extra(Extra.SOURCE, source)
            .queue()
    }

    @JvmStatic
    fun openSearchSettingsEvent() {
        TelemetryEvent.create(Category.ACTION, Method.OPEN, Object.SEARCH_ENGINE_SETTING).queue()
    }

    @JvmStatic
    fun menuRemoveEnginesEvent() {
        TelemetryEvent.create(Category.ACTION, Method.REMOVE, Object.SEARCH_ENGINE_SETTING).queue()
    }

    @JvmStatic
    fun menuRestoreEnginesEvent() {
        TelemetryEvent.create(Category.ACTION, Method.RESTORE, Object.SEARCH_ENGINE_SETTING).queue()
    }

    @JvmStatic
    fun menuAddSearchEngineEvent() {
        TelemetryEvent.create(Category.ACTION, Method.SHOW, Object.CUSTOM_SEARCH_ENGINE).queue()
    }

    @JvmStatic
    fun saveCustomSearchEngineEvent(success: Boolean) {
        TelemetryEvent.create(Category.ACTION, Method.SAVE, Object.CUSTOM_SEARCH_ENGINE)
            .extra(Extra.SUCCESS, success.toString())
            .queue()
    }

    @JvmStatic
    fun removeSearchEnginesEvent(selected: Int) {
        TelemetryEvent.create(Category.ACTION, Method.REMOVE, Object.REMOVE_SEARCH_ENGINES)
            .extra(Extra.SELECTED, selected.toString())
            .queue()
    }

    @JvmStatic
    fun addSearchEngineLearnMoreEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.ADD_SEARCH_ENGINE_LEARN_MORE).queue()
    }

    @JvmStatic
    fun reportSiteIssueEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.MENU, Value.REPORT_ISSUE).queue()
    }

    @JvmStatic
    fun respondToSearchSuggestionPrompt(enable: Boolean) {
        TelemetryEvent
            .create(Category.ACTION, Method.CLICK, Object.SEARCH_SUGGESTION_PROMPT, "$enable")
            .queue()
    }

    @JvmStatic
    fun makeDefaultBrowserSettings() {
        TelemetryEvent.create(Category.ACTION, Method.SHOW, Object.MAKE_DEFAULT_BROWSER_SETTINGS).queue()
    }

    @JvmStatic
    fun dayPassedSinceLastUpload(context: Context): Boolean {
        val dateOfLastPing = PreferenceManager
            .getDefaultSharedPreferences(context).getLong(LAST_MOBILE_METRICS_PINGS, 0)
        // Make sure a minimum of 1 day has passed since we collected data
        val currentDateLong = dateFormat.format(Date()).toLong()
        return currentDateLong > dateOfLastPing
    }

    private fun isDeviceWithTelemetryDisabled(): Boolean {
        val brand = "blackberry"
        val device = "bbf100"

        return Build.BRAND == brand && Build.DEVICE == device
    }
}
