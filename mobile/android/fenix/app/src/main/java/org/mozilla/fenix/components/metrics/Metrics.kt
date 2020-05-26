/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context
import mozilla.components.browser.awesomebar.facts.BrowserAwesomeBarFacts
import mozilla.components.browser.errorpages.ErrorType
import mozilla.components.browser.menu.facts.BrowserMenuFacts
import mozilla.components.browser.search.SearchEngine
import mozilla.components.browser.toolbar.facts.ToolbarFacts
import mozilla.components.concept.awesomebar.AwesomeBar
import mozilla.components.feature.awesomebar.provider.BookmarksStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.ClipboardSuggestionProvider
import mozilla.components.feature.awesomebar.provider.HistoryStorageSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SearchSuggestionProvider
import mozilla.components.feature.awesomebar.provider.SessionSuggestionProvider
import mozilla.components.feature.contextmenu.facts.ContextMenuFacts
import mozilla.components.feature.customtabs.CustomTabsFacts
import mozilla.components.feature.downloads.facts.DownloadsFacts
import mozilla.components.feature.findinpage.facts.FindInPageFacts
import mozilla.components.feature.media.facts.MediaFacts
import mozilla.components.support.base.Component
import mozilla.components.support.base.facts.Action
import mozilla.components.support.base.facts.Fact
import mozilla.components.support.base.facts.FactProcessor
import mozilla.components.support.base.facts.Facts
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.webextensions.facts.WebExtensionFacts
import org.mozilla.fenix.BuildConfig
import org.mozilla.fenix.GleanMetrics.Addons
import org.mozilla.fenix.GleanMetrics.AppTheme
import org.mozilla.fenix.GleanMetrics.Collections
import org.mozilla.fenix.GleanMetrics.ContextMenu
import org.mozilla.fenix.GleanMetrics.CrashReporter
import org.mozilla.fenix.GleanMetrics.ErrorPage
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.Logins
import org.mozilla.fenix.GleanMetrics.PerfAwesomebar
import org.mozilla.fenix.GleanMetrics.SearchShortcuts
import org.mozilla.fenix.GleanMetrics.Tip
import org.mozilla.fenix.GleanMetrics.ToolbarSettings
import org.mozilla.fenix.GleanMetrics.TrackingProtection
import org.mozilla.fenix.R
import org.mozilla.fenix.search.awesomebar.ShortcutsSuggestionProvider
import java.util.Locale

sealed class Event {

    // Interaction Events
    object OpenedAppFirstRun : Event()
    object InteractWithSearchURLArea : Event()
    object DismissedOnboarding : Event()
    object ClearedPrivateData : Event()
    object AddBookmark : Event()
    object RemoveBookmark : Event()
    object OpenedBookmark : Event()
    object OpenedBookmarkInNewTab : Event()
    object OpenedBookmarksInNewTabs : Event()
    object OpenedBookmarkInPrivateTab : Event()
    object OpenedBookmarksInPrivateTabs : Event()
    object EditedBookmark : Event()
    object MovedBookmark : Event()
    object ShareBookmark : Event()
    object CopyBookmark : Event()
    object AddBookmarkFolder : Event()
    object RemoveBookmarkFolder : Event()
    object RemoveBookmarks : Event()
    object CustomTabsClosed : Event()
    object CustomTabsActionTapped : Event()
    object CustomTabsMenuOpened : Event()
    object UriOpened : Event()
    object QRScannerOpened : Event()
    object QRScannerPromptDisplayed : Event()
    object QRScannerNavigationAllowed : Event()
    object QRScannerNavigationDenied : Event()
    object SyncAuthOpened : Event()
    object SyncAuthClosed : Event()
    object SyncAuthSignUp : Event()
    object SyncAuthSignIn : Event()
    object SyncAuthSignOut : Event()
    object SyncAuthScanPairing : Event()
    object SyncAuthUseEmail : Event()
    object SyncAuthUseEmailProblem : Event()
    object SyncAuthPaired : Event()
    object SyncAuthRecovered : Event()
    object SyncAuthOtherExternal : Event()
    object SyncAuthFromShared : Event()
    object SyncAccountOpened : Event()
    object SyncAccountClosed : Event()
    object SyncAccountSyncNow : Event()
    object SendTab : Event()
    object SignInToSendTab : Event()
    object HistoryOpened : Event()
    object HistoryItemShared : Event()
    object HistoryItemOpened : Event()
    object HistoryItemRemoved : Event()
    object HistoryAllItemsRemoved : Event()
    object ReaderModeAvailable : Event()
    object ReaderModeOpened : Event()
    object ReaderModeAppearanceOpened : Event()
    object CollectionRenamed : Event()
    object CollectionTabRestored : Event()
    object CollectionAllTabsRestored : Event()
    object CollectionTabRemoved : Event()
    object CollectionShared : Event()
    object CollectionRemoved : Event()
    object CollectionTabSelectOpened : Event()
    object CollectionTabLongPressed : Event()
    object CollectionAddTabPressed : Event()
    object CollectionRenamePressed : Event()
    object SearchWidgetNewTabPressed : Event()
    object SearchWidgetVoiceSearchPressed : Event()
    object FindInPageOpened : Event()
    object FindInPageClosed : Event()
    object FindInPageSearchCommitted : Event()
    object PrivateBrowsingGarbageIconTapped : Event()
    object PrivateBrowsingSnackbarUndoTapped : Event()
    object PrivateBrowsingNotificationTapped : Event()
    object PrivateBrowsingNotificationOpenTapped : Event()
    object PrivateBrowsingNotificationDeleteAndOpenTapped : Event()
    object PrivateBrowsingCreateShortcut : Event()
    object PrivateBrowsingAddShortcutCFR : Event()
    object PrivateBrowsingCancelCFR : Event()
    object PrivateBrowsingPinnedShortcutPrivateTab : Event()
    object PrivateBrowsingStaticShortcutTab : Event()
    object PrivateBrowsingStaticShortcutPrivateTab : Event()
    object TabMediaPlay : Event()
    object TabMediaPause : Event()
    object MediaPlayState : Event()
    object MediaPauseState : Event()
    object MediaStopState : Event()
    object InAppNotificationDownloadOpen : Event()
    object InAppNotificationDownloadTryAgain : Event()
    object NotificationDownloadCancel : Event()
    object NotificationDownloadOpen : Event()
    object NotificationDownloadPause : Event()
    object NotificationDownloadResume : Event()
    object NotificationDownloadTryAgain : Event()
    object NotificationMediaPlay : Event()
    object NotificationMediaPause : Event()
    object TopSiteOpenDefault : Event()
    object TopSiteOpenInNewTab : Event()
    object TopSiteOpenInPrivateTab : Event()
    object TopSiteRemoved : Event()
    object TrackingProtectionTrackerList : Event()
    object TrackingProtectionIconPressed : Event()
    object TrackingProtectionSettingsPanel : Event()
    object TrackingProtectionSettings : Event()
    object TrackingProtectionException : Event()
    object OpenLogins : Event()
    object OpenOneLogin : Event()
    object CopyLogin : Event()
    object ViewLoginPassword : Event()
    object CustomEngineAdded : Event()
    object CustomEngineDeleted : Event()
    object PrivateBrowsingShowSearchSuggestions : Event()
    object WhatsNewTapped : Event()
    object SupportTapped : Event()
    object PrivacyNoticeTapped : Event()
    object RightsTapped : Event()
    object LicensingTapped : Event()
    object LibrariesThatWeUseTapped : Event()
    object PocketTopSiteClicked : Event()
    object PocketTopSiteRemoved : Event()
    object FennecToFenixMigrated : Event()
    object AddonsOpenInSettings : Event()
    object AddonsOpenInToolbarMenu : Event()
    object VoiceSearchTapped : Event()
    object SearchWidgetCFRDisplayed : Event()
    object SearchWidgetCFRCanceled : Event()
    object SearchWidgetCFRNotNowPressed : Event()
    object SearchWidgetCFRAddWidgetPressed : Event()

    // Interaction events with extras

    data class PreferenceToggled(
        val preferenceKey: String,
        val enabled: Boolean,
        val context: Context
    ) : Event() {
        private val booleanPreferenceTelemetryAllowList = listOf(
            context.getString(R.string.pref_key_show_search_suggestions),
            context.getString(R.string.pref_key_remote_debugging),
            context.getString(R.string.pref_key_telemetry),
            context.getString(R.string.pref_key_tracking_protection),
            context.getString(R.string.pref_key_search_bookmarks),
            context.getString(R.string.pref_key_search_browsing_history),
            context.getString(R.string.pref_key_show_clipboard_suggestions),
            context.getString(R.string.pref_key_show_search_shortcuts),
            context.getString(R.string.pref_key_open_links_in_a_private_tab),
            context.getString(R.string.pref_key_sync_logins),
            context.getString(R.string.pref_key_sync_bookmarks),
            context.getString(R.string.pref_key_sync_history),
            context.getString(R.string.pref_key_show_voice_search),
            context.getString(R.string.pref_key_show_search_suggestions_in_private)
        )

        override val extras: Map<Events.preferenceToggledKeys, String>?
            get() = mapOf(
                Events.preferenceToggledKeys.preferenceKey to preferenceKey,
                Events.preferenceToggledKeys.enabled to enabled.toString()
            )

        init {
            // If the event is not in the allow list, we don't want to track it
            require(booleanPreferenceTelemetryAllowList.contains(preferenceKey))
        }
    }

    data class TipDisplayed(val identifier: String) : Event() {
        override val extras: Map<Tip.displayedKeys, String>?
            get() = hashMapOf(Tip.displayedKeys.identifier to identifier)
    }

    data class TipPressed(val identifier: String) : Event() {
        override val extras: Map<Tip.pressedKeys, String>?
            get() = hashMapOf(Tip.pressedKeys.identifier to identifier)
    }

    data class TipClosed(val identifier: String) : Event() {
        override val extras: Map<Tip.closedKeys, String>?
            get() = hashMapOf(Tip.closedKeys.identifier to identifier)
    }

    data class ToolbarPositionChanged(val position: Position) : Event() {
        enum class Position { TOP, BOTTOM }

        override val extras: Map<ToolbarSettings.changedPositionKeys, String>?
            get() = hashMapOf(ToolbarSettings.changedPositionKeys.position to position.name)
    }

    data class OpenedLink(val mode: Mode) : Event() {
        enum class Mode { NORMAL, PRIVATE }

        override val extras: Map<Events.openedLinkKeys, String>?
            get() = hashMapOf(Events.openedLinkKeys.mode to mode.name)
    }

    data class TrackingProtectionSettingChanged(val setting: Setting) : Event() {
        enum class Setting { STRICT, STANDARD }

        override val extras: Map<TrackingProtection.etpSettingChangedKeys, String>?
            get() = hashMapOf(TrackingProtection.etpSettingChangedKeys.etpSetting to setting.name)
    }

    data class SaveLoginsSettingChanged(val setting: Setting) : Event() {
        enum class Setting { NEVER_SAVE, ASK_TO_SAVE }

        override val extras: Map<Logins.saveLoginsSettingChangedKeys, String>?
            get() = hashMapOf(Logins.saveLoginsSettingChangedKeys.setting to setting.name)
    }

    data class OpenedApp(val source: Source) : Event() {
        enum class Source { APP_ICON, LINK, CUSTOM_TAB }

        override val extras: Map<Events.appOpenedKeys, String>?
            get() = hashMapOf(Events.appOpenedKeys.source to source.name)
    }

    data class CollectionSaveButtonPressed(val fromScreen: String) : Event() {
        override val extras: Map<Collections.saveButtonKeys, String>?
            get() = mapOf(Collections.saveButtonKeys.fromScreen to fromScreen)
    }

    data class CollectionSaved(val tabsOpenCount: Int, val tabsSelectedCount: Int) : Event() {
        override val extras: Map<Collections.savedKeys, String>?
            get() = mapOf(
                Collections.savedKeys.tabsOpen to tabsOpenCount.toString(),
                Collections.savedKeys.tabsSelected to tabsSelectedCount.toString()
            )
    }

    data class CollectionTabsAdded(val tabsOpenCount: Int, val tabsSelectedCount: Int) : Event() {
        override val extras: Map<Collections.tabsAddedKeys, String>?
            get() = mapOf(
                Collections.tabsAddedKeys.tabsOpen to tabsOpenCount.toString(),
                Collections.tabsAddedKeys.tabsSelected to tabsSelectedCount.toString()
            )
    }

    data class ErrorPageVisited(val errorType: ErrorType) : Event() {
        override val extras: Map<ErrorPage.visitedErrorKeys, String>?
            get() = mapOf(ErrorPage.visitedErrorKeys.errorType to errorType.name)
    }

    data class SearchBarTapped(val source: Source) : Event() {
        enum class Source { HOME, BROWSER }

        override val extras: Map<Events.searchBarTappedKeys, String>?
            get() = mapOf(Events.searchBarTappedKeys.source to source.name)
    }

    data class EnteredUrl(val autoCompleted: Boolean) : Event() {
        override val extras: Map<Events.enteredUrlKeys, String>?
            get() = mapOf(Events.enteredUrlKeys.autocomplete to autoCompleted.toString())
    }

    data class PerformedSearch(val eventSource: EventSource) : Event() {
        sealed class EngineSource {
            abstract val engine: SearchEngine
            abstract val isCustom: Boolean

            data class Default(override val engine: SearchEngine, override val isCustom: Boolean) :
                EngineSource()

            data class Shortcut(override val engine: SearchEngine, override val isCustom: Boolean) :
                EngineSource()

            // https://github.com/mozilla-mobile/fenix/issues/1607
            // Sanitize identifiers for custom search engines.
            val identifier: String
                get() = if (isCustom) "custom" else engine.identifier

            val searchEngine: SearchEngine
                get() = when (this) {
                    is Default -> engine
                    is Shortcut -> engine
                }

            val descriptor: String
                get() = when (this) {
                    is Default -> "default"
                    is Shortcut -> "shortcut"
                }
        }

        sealed class EventSource(open val engineSource: EngineSource) {
            data class Suggestion(override val engineSource: EngineSource) :
                EventSource(engineSource)

            data class Action(override val engineSource: EngineSource) : EventSource(engineSource)
            data class Widget(override val engineSource: EngineSource) : EventSource(engineSource)
            data class Shortcut(override val engineSource: EngineSource) : EventSource(engineSource)
            data class Other(override val engineSource: EngineSource) : EventSource(engineSource)

            private val label: String
                get() = when (this) {
                    is Suggestion -> "suggestion"
                    is Action -> "action"
                    is Widget -> "widget"
                    is Shortcut -> "shortcut"
                    is Other -> "other"
                }

            val countLabel: String
                get() = "${engineSource.identifier.toLowerCase(Locale.getDefault())}.$label"

            val sourceLabel: String
                get() = "${engineSource.descriptor}.$label"
        }

        enum class SearchAccessPoint {
            SUGGESTION, ACTION, WIDGET, SHORTCUT, NONE
        }

        override val extras: Map<Events.performedSearchKeys, String>?
            get() = mapOf(Events.performedSearchKeys.source to eventSource.sourceLabel)
    }

    data class SearchShortcutSelected(val engine: SearchEngine, val isCustom: Boolean) : Event() {
        private val engineName = if (isCustom) "custom" else engine.name
        override val extras: Map<SearchShortcuts.selectedKeys, String>?
            get() = mapOf(SearchShortcuts.selectedKeys.engine to engineName)
    }

    data class DarkThemeSelected(val source: Source) : Event() {
        enum class Source { SETTINGS, ONBOARDING }

        override val extras: Map<AppTheme.darkThemeSelectedKeys, String>?
            get() = mapOf(AppTheme.darkThemeSelectedKeys.source to source.name)
    }

    data class SearchWithAds(val providerName: String) : Event() {
        val label: String
            get() = providerName
    }

    data class SearchAdClicked(val providerName: String) : Event() {
        val label: String
            get() = providerName
    }

    data class SearchInContent(val keyName: String) : Event() {
        val label: String
            get() = keyName
    }

    class ContextMenuItemTapped private constructor(val item: String) : Event() {
        override val extras: Map<ContextMenu.itemTappedKeys, String>?
            get() = mapOf(ContextMenu.itemTappedKeys.named to item)

        companion object {
            fun create(context_item: String) =
                allowList[context_item]?.let { ContextMenuItemTapped(it) }

            private val allowList = mapOf(
                "mozac.feature.contextmenu.open_in_new_tab" to "open_in_new_tab",
                "mozac.feature.contextmenu.open_in_private_tab" to "open_in_private_tab",
                "mozac.feature.contextmenu.open_image_in_new_tab" to "open_image_in_new_tab",
                "mozac.feature.contextmenu.save_image" to "save_image",
                "mozac.feature.contextmenu.share_link" to "share_link",
                "mozac.feature.contextmenu.copy_link" to "copy_link",
                "mozac.feature.contextmenu.copy_image_location" to "copy_image_location"
            )
        }
    }

    object CrashReporterOpened : Event()
    data class CrashReporterClosed(val crashSubmitted: Boolean) : Event() {
        override val extras: Map<CrashReporter.closedKeys, String>?
            get() = mapOf(CrashReporter.closedKeys.crashSubmitted to crashSubmitted.toString())
    }

    data class BrowserMenuItemTapped(val item: Item) : Event() {
        enum class Item {
            SETTINGS, HELP, DESKTOP_VIEW_ON, DESKTOP_VIEW_OFF, FIND_IN_PAGE, NEW_TAB,
            NEW_PRIVATE_TAB, SHARE, REPORT_SITE_ISSUE, BACK, FORWARD, RELOAD, STOP, OPEN_IN_FENIX,
            SAVE_TO_COLLECTION, ADD_TO_TOP_SITES, ADD_TO_HOMESCREEN, QUIT, READER_MODE_ON,
            READER_MODE_OFF, OPEN_IN_APP, BOOKMARK, READER_MODE_APPEARANCE, ADDONS_MANAGER,
            BOOKMARKS, HISTORY, SYNC_TABS
        }

        override val extras: Map<Events.browserMenuActionKeys, String>?
            get() = mapOf(Events.browserMenuActionKeys.item to item.toString().toLowerCase(Locale.ROOT))
    }

    sealed class Search

    internal open val extras: Map<*, String>?
        get() = null
}

private fun Fact.toEvent(): Event? = when (Pair(component, item)) {
    Component.FEATURE_FINDINPAGE to FindInPageFacts.Items.CLOSE -> Event.FindInPageClosed
    Component.FEATURE_FINDINPAGE to FindInPageFacts.Items.INPUT -> Event.FindInPageSearchCommitted
    Component.FEATURE_CONTEXTMENU to ContextMenuFacts.Items.ITEM -> {
        metadata?.get("item")?.let { Event.ContextMenuItemTapped.create(it.toString()) }
    }

    Component.BROWSER_TOOLBAR to ToolbarFacts.Items.MENU -> {
        metadata?.get("customTab")?.let { Event.CustomTabsMenuOpened }
    }
    Component.BROWSER_MENU to BrowserMenuFacts.Items.WEB_EXTENSION_MENU_ITEM -> Event.AddonsOpenInToolbarMenu
    Component.FEATURE_CUSTOMTABS to CustomTabsFacts.Items.CLOSE -> Event.CustomTabsClosed
    Component.FEATURE_CUSTOMTABS to CustomTabsFacts.Items.ACTION_BUTTON -> Event.CustomTabsActionTapped

    Component.FEATURE_DOWNLOADS to DownloadsFacts.Items.NOTIFICATION -> {
        when (action) {
            Action.CANCEL -> Event.NotificationDownloadCancel
            Action.OPEN -> Event.NotificationDownloadOpen
            Action.PAUSE -> Event.NotificationDownloadPause
            Action.RESUME -> Event.NotificationDownloadResume
            Action.TRY_AGAIN -> Event.NotificationDownloadTryAgain
            else -> null
        }
    }

    Component.FEATURE_MEDIA to MediaFacts.Items.NOTIFICATION -> {
        when (action) {
            Action.PLAY -> Event.NotificationMediaPlay
            Action.PAUSE -> Event.NotificationMediaPause
            else -> null
        }
    }
    Component.FEATURE_MEDIA to MediaFacts.Items.STATE -> {
        when (action) {
            Action.PLAY -> Event.MediaPlayState
            Action.PAUSE -> Event.MediaPauseState
            Action.STOP -> Event.MediaStopState
            else -> null
        }
    }
    Component.SUPPORT_WEBEXTENSIONS to WebExtensionFacts.Items.WEB_EXTENSIONS_INITIALIZED -> {
        metadata?.get("installed")?.let { installedAddons ->
            if (installedAddons is List<*>) {
                Addons.hasInstalledAddons.set(installedAddons.size > 0)
            }
        }

        metadata?.get("enabled")?.let { enabledAddons ->
            if (enabledAddons is List<*>) {
                Addons.hasEnabledAddons.set(enabledAddons.size > 0)
            }
        }

        null
    }
    Component.BROWSER_AWESOMEBAR to BrowserAwesomeBarFacts.Items.PROVIDER_DURATION -> {
        metadata?.get(BrowserAwesomeBarFacts.MetadataKeys.DURATION_PAIR)?.let { providerTiming ->
            require(providerTiming is Pair<*, *>) { "Expected providerTiming to be a Pair" }
            when (val provider = providerTiming.first as AwesomeBar.SuggestionProvider) {
                is HistoryStorageSuggestionProvider -> PerfAwesomebar.historySuggestions
                is BookmarksStorageSuggestionProvider -> PerfAwesomebar.bookmarkSuggestions
                is SessionSuggestionProvider -> PerfAwesomebar.sessionSuggestions
                is SearchSuggestionProvider -> PerfAwesomebar.searchEngineSuggestions
                is ClipboardSuggestionProvider -> PerfAwesomebar.clipboardSuggestions
                is ShortcutsSuggestionProvider -> PerfAwesomebar.shortcutsSuggestions
                // NB: add PerfAwesomebar.syncedTabsSuggestions once we're using SyncedTabsSuggestionProvider
                else -> {
                    Logger("Metrics").error("Unknown suggestion provider: $provider")
                    null
                }
            }?.accumulateSamples(longArrayOf(providerTiming.second as Long))
        }
        null
    }
    else -> null
}

enum class MetricServiceType {
    Data, Marketing;
}

interface MetricsService {
    val type: MetricServiceType

    fun start()
    fun stop()
    fun track(event: Event)
    fun shouldTrack(event: Event): Boolean
}

interface MetricController {
    fun start(type: MetricServiceType)
    fun stop(type: MetricServiceType)
    fun track(event: Event)

    companion object {
        fun create(
            services: List<MetricsService>,
            isDataTelemetryEnabled: () -> Boolean,
            isMarketingDataTelemetryEnabled: () -> Boolean
        ): MetricController {
            return if (BuildConfig.TELEMETRY) {
                ReleaseMetricController(
                    services,
                    isDataTelemetryEnabled,
                    isMarketingDataTelemetryEnabled
                )
            } else DebugMetricController()
        }
    }
}

private class DebugMetricController : MetricController {
    override fun start(type: MetricServiceType) {
        Logger.debug("DebugMetricController: start")
    }

    override fun stop(type: MetricServiceType) {
        Logger.debug("DebugMetricController: stop")
    }

    override fun track(event: Event) {
        Logger.debug("DebugMetricController: track event: $event")
    }
}

private class ReleaseMetricController(
    private val services: List<MetricsService>,
    private val isDataTelemetryEnabled: () -> Boolean,
    private val isMarketingDataTelemetryEnabled: () -> Boolean
) : MetricController {
    private var initialized = mutableSetOf<MetricServiceType>()

    init {
        Facts.registerProcessor(object : FactProcessor {
            override fun process(fact: Fact) {
                fact.toEvent()?.also {
                    track(it)
                }
            }
        })
    }

    override fun start(type: MetricServiceType) {
        val isEnabled = isTelemetryEnabled(type)
        val isInitialized = isInitialized(type)
        if (!isEnabled || isInitialized) {
            return
        }

        services
            .filter { it.type == type }
            .forEach { it.start() }

        initialized.add(type)
    }

    override fun stop(type: MetricServiceType) {
        val isEnabled = isTelemetryEnabled(type)
        val isInitialized = isInitialized(type)
        if (isEnabled || !isInitialized) {
            return
        }

        services
            .filter { it.type == type }
            .forEach { it.stop() }

        initialized.remove(type)
    }

    override fun track(event: Event) {
        services
            .filter { it.shouldTrack(event) }
            .forEach {
                val isEnabled = isTelemetryEnabled(it.type)
                val isInitialized = isInitialized(it.type)
                if (!isEnabled || !isInitialized) {
                    return
                }

                it.track(event)
            }
    }

    private fun isInitialized(type: MetricServiceType): Boolean = initialized.contains(type)

    private fun isTelemetryEnabled(type: MetricServiceType): Boolean = when (type) {
        MetricServiceType.Data -> isDataTelemetryEnabled()
        MetricServiceType.Marketing -> isMarketingDataTelemetryEnabled()
    }
}
