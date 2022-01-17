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
        val ACTION = "action"
        val ERROR = "error"
        val HISTOGRAM = "histogram"
    }

    private object Method {
        val CLICK = "click"
        val CANCEL = "cancel"
        val LONG_PRESS = "long_press"
        val CHANGE = "change"
        val OPEN = "open"
        val INSTALL = "install"
        val SHOW = "show"
        val HIDE = "hide"
        val TYPE_QUERY = "type_query"
        val TYPE_SELECT_QUERY = "select_query"
    }

    private object Object {
        val SETTING = "setting"
        val MENU = "menu"
        val BLOCKING_SWITCH = "blocking_switch"
        val BROWSER = "browser"
        val SEARCH_SUGGESTION_PROMPT = "search_suggestion_prompt"
        val MAKE_DEFAULT_BROWSER_OPEN_WITH = "make_default_browser_open_with"
        val MAKE_DEFAULT_BROWSER_SETTINGS = "make_default_browser_settings"
        val SEARCH_BAR = "search_bar"
    }

    private object Value {
        val DEFAULT = "default"
        val FIREFOX = "firefox"
        val SELECTION = "selection"
        val OPEN = "open"
        val URL = "url"
        val SEARCH = "search"
        val CANCEL = "cancel"
        val TAB = "tab"
        val WHATS_NEW = "whats_new"
        val RESUME = "resume"
        val FULL_BROWSER = "full_browser"
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
        val TOTAL_URI_COUNT = "total_uri_count"
        val UNIQUE_DOMAINS_COUNT = "unique_domains_count"
        val SEARCH_SUGGESTION = "search_suggestion"
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
    }

    @JvmStatic
    fun stopMainActivity() {
        TelemetryHolder.get()
            .queuePing(TelemetryCorePingBuilder.TYPE)
            .queuePing(TelemetryEventPingBuilder.TYPE)
            .queuePing(TelemetryMobileMetricsPingBuilder.TYPE)
            .scheduleUpload()
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

    enum class AutoCompleteEventSource {
        SETTINGS
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
