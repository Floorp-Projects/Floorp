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
import org.mozilla.focus.GleanMetrics.BrowserSearch
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

    private const val MAXIMUM_CUSTOM_TAB_EXTRAS = 10

    private const val HISTOGRAM_SIZE = 200
    private const val BUCKET_SIZE_MS = 100
    private const val HISTOGRAM_MIN_INDEX = 0

    private val isEnabledByDefault: Boolean
        get() = !AppConstants.isKlarBuild

    private object Category {
        val ACTION = "action"
        val ERROR = "error"
        val HISTOGRAM = "histogram"
    }

    private object Method {
        val TYPE_URL = "type_url"
        val TYPE_QUERY = "type_query"
        val TYPE_SELECT_QUERY = "select_query"
        val CLICK = "click"
        val CANCEL = "cancel"
        val LONG_PRESS = "long_press"
        val CHANGE = "change"
        val FOREGROUND = "foreground"
        val BACKGROUND = "background"
        val SAVE = "save"
        val OPEN = "open"
        val INSTALL = "install"
        val INTENT_URL = "intent_url"
        val INTENT_CUSTOM_TAB = "intent_custom_tab"
        val TEXT_SELECTION_INTENT = "text_selection_intent"
        val SHOW = "show"
        val HIDE = "hide"
        val SHARE_INTENT = "share_intent"
        val REMOVE = "remove"
        val REORDER = "reorder"
    }

    private object Object {
        val SEARCH_BAR = "search_bar"
        val SETTING = "setting"
        val APP = "app"
        val MENU = "menu"
        val BACK_BUTTON = "back_button"
        val BLOCKING_SWITCH = "blocking_switch"
        val BROWSER = "browser"
        val FIRSTRUN = "firstrun"
        val HOMESCREEN_SHORTCUT = "homescreen_shortcut"
        val APP_ICON = "app_icon"
        val AUTOCOMPLETE_DOMAIN = "autocomplete_domain"
        val SEARCH_SUGGESTION_PROMPT = "search_suggestion_prompt"
        val MAKE_DEFAULT_BROWSER_OPEN_WITH = "make_default_browser_open_with"
        val MAKE_DEFAULT_BROWSER_SETTINGS = "make_default_browser_settings"
    }

    private object Value {
        val DEFAULT = "default"
        val FIREFOX = "firefox"
        val SELECTION = "selection"
        val ERASE_TO_HOME = "erase_home"
        val ERASE_TO_APP = "erase_app"
        val SKIP = "skip"
        val FINISH = "finish"
        val OPEN = "open"
        val URL = "url"
        val SEARCH = "search"
        val CANCEL = "cancel"
        val TAB = "tab"
        val WHATS_NEW = "whats_new"
        val RESUME = "resume"
        val FULL_BROWSER = "full_browser"
        val REPORT_ISSUE = "report_issue"
        val SETTINGS = "settings"
        val QUICK_ADD = "quick_add"
        val CLOSE_TAB = "close_tab"
    }

    private object Extra {
        val FROM = "from"
        val TO = "to"
        val TOTAL = "total"
        val SELECTED = "selected"
        val HIGHLIGHTED = "highlighted"
        val AUTOCOMPLETE = "autocomplete"
        val SOURCE = "source"
        val SUCCESS = "success"
        val SEARCH_SUGGESTION = "search_suggestion"
        val TOTAL_URI_COUNT = "total_uri_count"
        val UNIQUE_DOMAINS_COUNT = "unique_domains_count"
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
                    resources.getString(R.string.pref_key_homescreen_tips),
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

    /**
     * Sends a list of the custom tab options that a custom-tab intent made use of.
     */
    @JvmStatic
    fun customTabsIntentEvent(options: List<String>) {
        val event = TelemetryEvent.create(Category.ACTION, Method.INTENT_CUSTOM_TAB, Object.APP)

        // We can send at most 10 extras per event - we just ignore the rest if there are too many
        val extrasCount: Int = if (options.size > MAXIMUM_CUSTOM_TAB_EXTRAS) {
            MAXIMUM_CUSTOM_TAB_EXTRAS
        } else {
            options.size
        }

        for (option in options.subList(0, extrasCount)) {
            event.extra(option, "true")
        }

        event.queue()
    }

    @JvmStatic
    fun textSelectionIntentEvent() {
        TelemetryEvent.create(Category.ACTION, Method.TEXT_SELECTION_INTENT, Object.APP).queue()
    }

    @JvmStatic
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
    fun openHomescreenShortcutEvent() {
        TelemetryEvent.create(Category.ACTION, Method.CLICK, Object.HOMESCREEN_SHORTCUT, Value.OPEN).queue()
    }

    @JvmStatic
    fun settingsEvent(key: String, value: String) {
        TelemetryEvent.create(Category.ACTION, Method.CHANGE, Object.SETTING, key)
            .extra(Extra.TO, value)
            .queue()
    }

    @JvmStatic
    fun blockingSwitchEvent(isBlockingEnabled: Boolean) {
        TelemetryEvent.create(
            Category.ACTION,
            Method.CLICK,
            Object.BLOCKING_SWITCH,
            isBlockingEnabled.toString()
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
    fun dayPassedSinceLastUpload(context: Context): Boolean {
        val dateOfLastPing = PreferenceManager
            .getDefaultSharedPreferences(context).getLong(LAST_MOBILE_METRICS_PINGS, 0)
        // Make sure a minimum of 1 day has passed since we collected data
        val currentDateLong = dateFormat.format(Date()).toLong()
        return currentDateLong > dateOfLastPing
    }

    fun searchWithAdsShownEvent(provider: String) {
        BrowserSearch.withAds[provider].add()
    }

    fun clickAddInSearchEvent(provider: String) {
        BrowserSearch.adClicks[provider].add()
    }

    fun inContentSearchEvent(provider: String) {
        BrowserSearch.inContent[provider].add()
    }

    private fun isDeviceWithTelemetryDisabled(): Boolean {
        val brand = "blackberry"
        val device = "bbf100"

        return Build.BRAND == brand && Build.DEVICE == device
    }
}
