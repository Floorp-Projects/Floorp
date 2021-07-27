/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context
import mozilla.components.browser.errorpages.ErrorType
import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.feature.top.sites.TopSite
import org.mozilla.fenix.GleanMetrics.Addons
import org.mozilla.fenix.GleanMetrics.AndroidKeystoreExperiment
import org.mozilla.fenix.GleanMetrics.AppTheme
import org.mozilla.fenix.GleanMetrics.Autoplay
import org.mozilla.fenix.GleanMetrics.Collections
import org.mozilla.fenix.GleanMetrics.ContextMenu
import org.mozilla.fenix.GleanMetrics.CrashReporter
import org.mozilla.fenix.GleanMetrics.ErrorPage
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.Logins
import org.mozilla.fenix.GleanMetrics.Onboarding
import org.mozilla.fenix.GleanMetrics.ProgressiveWebApp
import org.mozilla.fenix.GleanMetrics.SearchShortcuts
import org.mozilla.fenix.GleanMetrics.Tip
import org.mozilla.fenix.GleanMetrics.ToolbarSettings
import org.mozilla.fenix.GleanMetrics.TopSites
import org.mozilla.fenix.GleanMetrics.TrackingProtection
import org.mozilla.fenix.R
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
    object NormalAndPrivateUriOpened : Event()
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
    object SyncAuthFromSharedReuse : Event()
    object SyncAuthFromSharedCopy : Event()
    object SyncAccountOpened : Event()
    object SyncAccountSyncNow : Event()
    object SendTab : Event()
    object SignInToSendTab : Event()
    object HistoryOpened : Event()
    object HistoryItemShared : Event()
    object HistoryItemOpened : Event()
    object HistoryOpenedInNewTab : Event()
    object HistoryOpenedInNewTabs : Event()
    object HistoryOpenedInPrivateTab : Event()
    object HistoryOpenedInPrivateTabs : Event()
    object HistoryItemRemoved : Event()
    object HistoryAllItemsRemoved : Event()
    object ReaderModeAvailable : Event()
    object ReaderModeOpened : Event()
    object ReaderModeClosed : Event()
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
    object PrivateBrowsingSnackbarUndoTapped : Event()
    object PrivateBrowsingNotificationTapped : Event()
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
    object MediaFullscreenState : Event()
    object MediaPictureInPictureState : Event()
    object InAppNotificationDownloadOpen : Event()
    object InAppNotificationDownloadTryAgain : Event()
    object NotificationDownloadCancel : Event()
    object NotificationDownloadOpen : Event()
    object NotificationDownloadPause : Event()
    object NotificationDownloadResume : Event()
    object NotificationDownloadTryAgain : Event()
    object DownloadAdded : Event()
    object DownloadsScreenOpened : Event()
    object DownloadsItemOpened : Event()
    object DownloadsItemDeleted : Event()
    object NotificationMediaPlay : Event()
    object NotificationMediaPause : Event()
    object TopSiteOpenDefault : Event()
    object TopSiteOpenGoogle : Event()
    object TopSiteOpenBaidu : Event()
    object TopSiteOpenFrecent : Event()
    object TopSiteOpenPinned : Event()
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
    object DeleteLogin : Event()
    object EditLogin : Event()
    object EditLoginSave : Event()
    object ViewLoginPassword : Event()
    object CustomEngineAdded : Event()
    object CustomEngineDeleted : Event()
    object PrivateBrowsingShowSearchSuggestions : Event()
    object WhatsNewTapped : Event()
    object SupportTapped : Event()
    object PrivacyNoticeTapped : Event()
    object PocketTopSiteClicked : Event()
    object PocketTopSiteRemoved : Event()
    object FennecToFenixMigrated : Event()
    object AddonsOpenInSettings : Event()
    object VoiceSearchTapped : Event()
    object SearchWidgetInstalled : Event()
    object OnboardingAutoSignIn : Event()
    object OnboardingManualSignIn : Event()
    object OnboardingPrivacyNotice : Event()
    object OnboardingPrivateBrowsing : Event()
    object OnboardingFinish : Event()
    object ChangedToDefaultBrowser : Event()
    object DefaultBrowserNotifTapped : Event()

    object LoginDialogPromptDisplayed : Event()
    object LoginDialogPromptCancelled : Event()
    object LoginDialogPromptSave : Event()
    object LoginDialogPromptNeverSave : Event()

    // Tab tray
    object TabsTrayOpened : Event()
    object TabsTrayClosed : Event()
    object OpenedExistingTab : Event()
    object ClosedExistingTab : Event()
    object TabsTrayPrivateModeTapped : Event()
    object TabsTrayNormalModeTapped : Event()
    object TabsTraySyncedModeTapped : Event()
    object NewTabTapped : Event()
    object NewPrivateTabTapped : Event()
    object TabsTrayMenuOpened : Event()
    object TabsTraySaveToCollectionPressed : Event()
    object TabsTrayShareAllTabsPressed : Event()
    object TabsTrayCloseAllTabsPressed : Event()
    object TabsTrayRecentlyClosedPressed : Event()

    object ProgressiveWebAppOpenFromHomescreenTap : Event()
    object ProgressiveWebAppInstallAsShortcut : Event()

    object MasterPasswordMigrationSuccess : Event()
    object MasterPasswordMigrationDisplayed : Event()

    object TabSettingsOpened : Event()

    object CopyUrlUsed : Event()

    object SyncedTabOpened : Event()

    object RecentlyClosedTabsOpened : Event()
    object HaveOpenTabs : Event()
    object HaveNoOpenTabs : Event()

    object BannerOpenInAppDisplayed : Event()
    object BannerOpenInAppDismissed : Event()
    object BannerOpenInAppGoToSettings : Event()

    object ContextMenuCopyTapped : Event()
    object ContextMenuSearchTapped : Event()
    object ContextMenuSelectAllTapped : Event()
    object ContextMenuShareTapped : Event()

    object SyncedTabSuggestionClicked : Event()
    object BookmarkSuggestionClicked : Event()
    object ClipboardSuggestionClicked : Event()
    object HistorySuggestionClicked : Event()
    object SearchActionClicked : Event()
    object SearchSuggestionClicked : Event()
    object OpenedTabSuggestionClicked : Event()

    // Set default browser experiment metrics
    object SetDefaultBrowserNewTabClicked : Event()
    object CloseExperimentCardClicked : Event()
    object ToolbarMenuShown : Event()
    object SetDefaultBrowserToolbarMenuClicked : Event()
    object SetDefaultBrowserSettingsScreenClicked : Event()

    // Home menu interaction
    object HomeMenuSettingsItemClicked : Event()
    object HomeScreenDisplayed : Event()

    // Browser Toolbar
    object BrowserToolbarHomeButtonClicked : Event()

    // Start on Home
    object StartOnHomeEnterHomeScreen : Event()
    object StartOnHomeOpenTabsTray : Event()

    // Recent tabs
    object ShowAllRecentTabs : Event()
    object OpenRecentTab : Event()
    object OpenInProgressMediaTab : Event()

    // Android Autofill
    object AndroidAutofillUnlockSuccessful : Event()
    object AndroidAutofillUnlockCanceled : Event()
    object AndroidAutofillSearchDisplayed : Event()
    object AndroidAutofillSearchItemSelected : Event()
    object AndroidAutofillConfirmationSuccessful : Event()
    object AndroidAutofillConfirmationCanceled : Event()
    object AndroidAutofillRequestWithLogins : Event()
    object AndroidAutofillRequestWithoutLogins : Event()

    // Interaction events with extras

    data class TopSiteSwipeCarousel(val page: Int) : Event() {
        override val extras: Map<TopSites.swipeCarouselKeys, String>?
            get() = hashMapOf(TopSites.swipeCarouselKeys.page to page.toString())
    }

    data class SecurePrefsExperimentFailure(val failureException: String) : Event() {
        override val extras =
            mapOf(AndroidKeystoreExperiment.experimentFailureKeys.failureException to failureException)
    }
    data class SecurePrefsGetFailure(val failureException: String) : Event() {
        override val extras =
            mapOf(AndroidKeystoreExperiment.getFailureKeys.failureException to failureException)
    }
    data class SecurePrefsGetSuccess(val successCode: String) : Event() {
        override val extras =
            mapOf(AndroidKeystoreExperiment.getResultKeys.result to successCode)
    }
    data class SecurePrefsWriteFailure(val failureException: String) : Event() {
        override val extras =
            mapOf(AndroidKeystoreExperiment.writeFailureKeys.failureException to failureException)
    }
    object SecurePrefsWriteSuccess : Event()
    object SecurePrefsReset : Event()

    data class TopSiteLongPress(val type: TopSite.Type) : Event() {
        override val extras: Map<TopSites.longPressKeys, String>?
            get() = hashMapOf(TopSites.longPressKeys.type to type.name)
    }

    data class ProgressiveWebAppForeground(val timeForegrounded: Long) : Event() {
        override val extras: Map<ProgressiveWebApp.foregroundKeys, String>?
            get() = mapOf(ProgressiveWebApp.foregroundKeys.timeMs to timeForegrounded.toString())
    }

    data class ProgressiveWebAppBackground(val timeBackgrounded: Long) : Event() {
        override val extras: Map<ProgressiveWebApp.backgroundKeys, String>?
            get() = mapOf(ProgressiveWebApp.backgroundKeys.timeMs to timeBackgrounded.toString())
    }

    data class OnboardingToolbarPosition(val position: Position) : Event() {
        enum class Position { TOP, BOTTOM }

        override val extras: Map<Onboarding.prefToggledToolbarPositionKeys, String>?
            get() = hashMapOf(Onboarding.prefToggledToolbarPositionKeys.position to position.name)
    }

    data class OnboardingTrackingProtection(val setting: Setting) : Event() {
        enum class Setting { STRICT, STANDARD }

        override val extras: Map<Onboarding.prefToggledTrackingProtKeys, String>?
            get() = hashMapOf(Onboarding.prefToggledTrackingProtKeys.setting to setting.name)
    }

    data class OnboardingThemePicker(val theme: Theme) : Event() {
        enum class Theme { LIGHT, DARK, FOLLOW_DEVICE }

        override val extras: Map<Onboarding.prefToggledThemePickerKeys, String>?
            get() = mapOf(Onboarding.prefToggledThemePickerKeys.theme to theme.name)
    }

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
            context.getString(R.string.pref_key_show_search_engine_shortcuts),
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

    data class AddonsOpenInToolbarMenu(val addonId: String) : Event() {
        override val extras: Map<Addons.openAddonInToolbarMenuKeys, String>?
            get() = hashMapOf(Addons.openAddonInToolbarMenuKeys.addonId to addonId)
    }

    data class AddonOpenSetting(val addonId: String) : Event() {
        override val extras: Map<Addons.openAddonSettingKeys, String>?
            get() = hashMapOf(Addons.openAddonSettingKeys.addonId to addonId)
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
        enum class Setting { STRICT, STANDARD, CUSTOM }

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
                get() = if (isCustom) "custom" else engine.id

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
            data class TopSite(override val engineSource: EngineSource) : EventSource(engineSource)
            data class Other(override val engineSource: EngineSource) : EventSource(engineSource)

            private val label: String
                get() = when (this) {
                    is Suggestion -> "suggestion"
                    is Action -> "action"
                    is Widget -> "widget"
                    is Shortcut -> "shortcut"
                    is TopSite -> "topsite"
                    is Other -> "other"
                }

            val countLabel: String
                get() = "${engineSource.identifier.lowercase(Locale.getDefault())}.$label"

            val sourceLabel: String
                get() = "${engineSource.descriptor}.$label"
        }

        enum class SearchAccessPoint {
            SUGGESTION, ACTION, WIDGET, SHORTCUT, TOPSITE, NONE
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
        enum class Source { SETTINGS }

        override val extras: Map<AppTheme.darkThemeSelectedKeys, String>?
            get() = mapOf(AppTheme.darkThemeSelectedKeys.source to source.name)
    }

    data class SearchWithAds(val providerName: String) : Event() {
        val label: String
            get() = providerName
    }

    data class SearchAdClicked(val keyName: String) : Event() {
        val label: String
            get() = keyName
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
                "mozac.feature.contextmenu.copy_image_location" to "copy_image_location",
                "mozac.feature.contextmenu.share_image" to "share_image"
            )
        }
    }

    object CrashReporterOpened : Event()
    data class AddonInstalled(val addonId: String) : Event()

    data class CrashReporterClosed(val crashSubmitted: Boolean) : Event() {
        override val extras: Map<CrashReporter.closedKeys, String>?
            get() = mapOf(CrashReporter.closedKeys.crashSubmitted to crashSubmitted.toString())
    }

    data class BrowserMenuItemTapped(val item: Item) : Event() {
        enum class Item {
            SETTINGS, HELP, DESKTOP_VIEW_ON, DESKTOP_VIEW_OFF, FIND_IN_PAGE, NEW_TAB,
            NEW_PRIVATE_TAB, SHARE, BACK, FORWARD, RELOAD, STOP, OPEN_IN_FENIX,
            SAVE_TO_COLLECTION, ADD_TO_TOP_SITES, ADD_TO_HOMESCREEN, QUIT, READER_MODE_ON,
            READER_MODE_OFF, OPEN_IN_APP, BOOKMARK, READER_MODE_APPEARANCE, ADDONS_MANAGER,
            BOOKMARKS, HISTORY, SYNC_TABS, DOWNLOADS, SET_DEFAULT_BROWSER, SYNC_ACCOUNT
        }

        override val extras: Map<Events.browserMenuActionKeys, String>?
            get() = mapOf(Events.browserMenuActionKeys.item to item.toString().lowercase(Locale.ROOT))
    }

    data class TabCounterMenuItemTapped(val item: Item) : Event() {
        enum class Item {
            NEW_TAB, NEW_PRIVATE_TAB, CLOSE_TAB
        }

        override val extras: Map<Events.tabCounterMenuActionKeys, String>?
            get() = mapOf(Events.tabCounterMenuActionKeys.item to item.toString().lowercase(Locale.ROOT))
    }

    object AutoPlaySettingVisited : Event()

    data class AutoPlaySettingChanged(val setting: AutoplaySetting) : Event() {
        enum class AutoplaySetting {
            BLOCK_CELLULAR, BLOCK_AUDIO, BLOCK_ALL, ALLOW_ALL
        }

        override val extras: Map<Autoplay.settingChangedKeys, String>?
            get() = mapOf(Autoplay.settingChangedKeys.autoplaySetting to setting.toString().lowercase(Locale.ROOT))
    }

    data class TabViewSettingChanged(val type: Type) : Event() {
        enum class Type { LIST, GRID }

        override val extras: Map<Events.tabViewChangedKeys, String>?
            get() = mapOf(Events.tabViewChangedKeys.type to type.toString().lowercase(Locale.ROOT))
    }

    sealed class Search

    internal open val extras: Map<*, String>?
        get() = null
}
