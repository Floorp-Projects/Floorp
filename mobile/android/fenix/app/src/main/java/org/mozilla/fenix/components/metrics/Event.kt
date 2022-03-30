/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import mozilla.components.browser.state.search.SearchEngine
import mozilla.components.feature.top.sites.TopSite
import org.mozilla.fenix.GleanMetrics.Addons
import org.mozilla.fenix.GleanMetrics.AppTheme
import org.mozilla.fenix.GleanMetrics.Autoplay
import org.mozilla.fenix.GleanMetrics.ContextMenu
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.Logins
import org.mozilla.fenix.GleanMetrics.Pocket
import org.mozilla.fenix.GleanMetrics.SearchTerms
import org.mozilla.fenix.GleanMetrics.TopSites
import org.mozilla.fenix.ext.name
import java.util.Locale

sealed class Event {

    // Interaction Events
    object OpenedAppFirstRun : Event()
    object InteractWithSearchURLArea : Event()
    object DismissedOnboarding : Event()
    object ClearedPrivateData : Event()
    object AddBookmark : Event()
    object CustomTabsClosed : Event()
    object CustomTabsActionTapped : Event()
    object CustomTabsMenuOpened : Event()
    object HistoryHighlightOpened : Event()
    object HistorySearchGroupOpened : Event()
    object ReaderModeAvailable : Event()
    object ReaderModeOpened : Event()
    object ReaderModeClosed : Event()
    object ReaderModeAppearanceOpened : Event()
    object TabMediaPlay : Event()
    object TabMediaPause : Event()
    object MediaPlayState : Event()
    object MediaPauseState : Event()
    object MediaStopState : Event()
    object MediaFullscreenState : Event()
    object MediaPictureInPictureState : Event()
    object NotificationMediaPlay : Event()
    object NotificationMediaPause : Event()
    object TopSiteOpenDefault : Event()
    object TopSiteOpenGoogle : Event()
    object TopSiteOpenBaidu : Event()
    object TopSiteOpenFrecent : Event()
    object TopSiteOpenPinned : Event()
    object TopSiteOpenProvided : Event()
    object TopSiteOpenInNewTab : Event()
    object TopSiteOpenInPrivateTab : Event()
    object TopSiteOpenContileInPrivateTab : Event()
    object TopSiteRemoved : Event()
    object TopSiteContileSettings : Event()
    object TopSiteContilePrivacy : Event()
    object GoogleTopSiteRemoved : Event()
    object BaiduTopSiteRemoved : Event()
    object OpenLogins : Event()
    object OpenOneLogin : Event()
    object CopyLogin : Event()
    object DeleteLogin : Event()
    object EditLogin : Event()
    object EditLoginSave : Event()
    object ViewLoginPassword : Event()
    object PocketTopSiteClicked : Event()
    object PocketTopSiteRemoved : Event()
    object PocketHomeRecsShown : Event()
    object PocketHomeRecsDiscoverMoreClicked : Event()
    object PocketHomeRecsLearnMoreClicked : Event()
    data class PocketHomeRecsStoryClicked(
        val timesShown: Long,
        val storyPosition: Pair<Int, Int>,
    ) : Event() {
        override val extras: Map<Pocket.homeRecsStoryClickedKeys, String>
            get() = mapOf(
                Pocket.homeRecsStoryClickedKeys.timesShown to timesShown.toString(),
                Pocket.homeRecsStoryClickedKeys.position to "${storyPosition.first}x${storyPosition.second}"
            )
    }

    data class PocketHomeRecsCategoryClicked(
        val categoryname: String,
        val previousSelectedCategoriesTotal: Int,
        val isSelectedNextState: Boolean
    ) : Event() {
        override val extras: Map<Pocket.homeRecsCategoryClickedKeys, String>
            get() = mapOf(
                Pocket.homeRecsCategoryClickedKeys.categoryName to categoryname,
                Pocket.homeRecsCategoryClickedKeys.selectedTotal to previousSelectedCategoriesTotal.toString(),
                Pocket.homeRecsCategoryClickedKeys.newState to when (isSelectedNextState) {
                    true -> "selected"
                    false -> "deselected"
                }
            )
    }
    object AddonsOpenInSettings : Event()
    object VoiceSearchTapped : Event()
    object SearchWidgetInstalled : Event()

    object ProgressiveWebAppOpenFromHomescreenTap : Event()
    object ProgressiveWebAppInstallAsShortcut : Event()

    object TabSettingsOpened : Event()

    object HaveOpenTabs : Event()
    object HaveNoOpenTabs : Event()

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
    object SetDefaultBrowserToolbarMenuClicked : Event()

    // Home menu interaction
    object HomeMenuSettingsItemClicked : Event()
    object HomeScreenDisplayed : Event()
    object HomeScreenViewCount : Event()
    object HomeScreenCustomizedHomeClicked : Event()

    // Start on Home
    object StartOnHomeEnterHomeScreen : Event()
    object StartOnHomeOpenTabsTray : Event()

    // Recent tabs
    object ShowAllRecentTabs : Event()
    object OpenRecentTab : Event()
    object OpenInProgressMediaTab : Event()
    object RecentTabsSectionIsVisible : Event()
    object RecentTabsSectionIsNotVisible : Event()

    // Recent bookmarks
    object BookmarkClicked : Event()
    object ShowAllBookmarks : Event()
    object RecentBookmarksShown : Event()
    data class RecentBookmarkCount(val count: Int) : Event()

    // Recently visited/Recent searches
    object RecentSearchesGroupDeleted : Event()

    // Android Autofill
    object AndroidAutofillUnlockSuccessful : Event()
    object AndroidAutofillUnlockCanceled : Event()
    object AndroidAutofillSearchDisplayed : Event()
    object AndroidAutofillSearchItemSelected : Event()
    object AndroidAutofillConfirmationSuccessful : Event()
    object AndroidAutofillConfirmationCanceled : Event()
    object AndroidAutofillRequestWithLogins : Event()
    object AndroidAutofillRequestWithoutLogins : Event()

    // Credit cards
    object CreditCardSaved : Event()
    object CreditCardDeleted : Event()
    object CreditCardModified : Event()
    object CreditCardFormDetected : Event()
    object CreditCardAutofilled : Event()
    object CreditCardAutofillPromptShown : Event()
    object CreditCardAutofillPromptExpanded : Event()
    object CreditCardAutofillPromptDismissed : Event()
    object CreditCardManagementAddTapped : Event()
    object CreditCardManagementCardTapped : Event()

    // Interaction events with extras

    data class TopSiteSwipeCarousel(val page: Int) : Event() {
        override val extras: Map<TopSites.swipeCarouselKeys, String>?
            get() = hashMapOf(TopSites.swipeCarouselKeys.page to page.toString())
    }

    data class TopSiteLongPress(val topSite: TopSite) : Event() {
        override val extras: Map<TopSites.longPressKeys, String>?
            get() = hashMapOf(TopSites.longPressKeys.type to topSite.name())
    }

    data class TopSiteContileImpression(val position: Int, val source: Source) : Event() {
        enum class Source { NEWTAB }
    }

    data class TopSiteContileClick(val position: Int, val source: Source) : Event() {
        enum class Source { NEWTAB }
    }

    data class AddonsOpenInToolbarMenu(val addonId: String) : Event() {
        override val extras: Map<Addons.openAddonInToolbarMenuKeys, String>?
            get() = hashMapOf(Addons.openAddonInToolbarMenuKeys.addonId to addonId)
    }

    data class AddonOpenSetting(val addonId: String) : Event() {
        override val extras: Map<Addons.openAddonSettingKeys, String>?
            get() = hashMapOf(Addons.openAddonSettingKeys.addonId to addonId)
    }

    data class SaveLoginsSettingChanged(val setting: Setting) : Event() {
        enum class Setting { NEVER_SAVE, ASK_TO_SAVE }

        override val extras: Map<Logins.saveLoginsSettingChangedKeys, String>?
            get() = hashMapOf(Logins.saveLoginsSettingChangedKeys.setting to setting.name)
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

    data class AddonInstalled(val addonId: String) : Event()

    object AutoPlaySettingVisited : Event()

    data class AutoPlaySettingChanged(val setting: AutoplaySetting) : Event() {
        enum class AutoplaySetting {
            BLOCK_CELLULAR, BLOCK_AUDIO, BLOCK_ALL, ALLOW_ALL
        }

        override val extras: Map<Autoplay.settingChangedKeys, String>?
            get() = mapOf(Autoplay.settingChangedKeys.autoplaySetting to setting.toString().lowercase(Locale.ROOT))
    }

    data class SearchTermGroupCount(val count: Int) : Event() {
        override val extras: Map<SearchTerms.numberOfSearchTermGroupKeys, String>
            get() = hashMapOf(SearchTerms.numberOfSearchTermGroupKeys.count to count.toString())
    }

    data class AverageTabsPerSearchTermGroup(val averageSize: Double) : Event() {
        override val extras: Map<SearchTerms.averageTabsPerGroupKeys, String>
            get() = hashMapOf(SearchTerms.averageTabsPerGroupKeys.count to averageSize.toString())
    }

    data class SearchTermGroupSizeDistribution(val groupSizes: List<Long>) : Event()

    object JumpBackInGroupTapped : Event()

    sealed class Search

    object WallpaperSettingsOpened : Event()
    data class WallpaperSelected(val wallpaper: org.mozilla.fenix.wallpapers.Wallpaper) : Event()
    data class WallpaperSwitched(val wallpaper: org.mozilla.fenix.wallpapers.Wallpaper) : Event()
    data class ChangeWallpaperWithLogoToggled(val checked: Boolean) : Event()

    sealed class Messaging(open val messageId: String) : Event() {
        data class MessageShown(override val messageId: String) : Messaging(messageId)
        data class MessageDismissed(override val messageId: String) : Messaging(messageId)
        data class MessageClicked(override val messageId: String, val uuid: String?) :
            Messaging(messageId)
        data class MessageMalformed(override val messageId: String) : Messaging(messageId)
        data class MessageExpired(override val messageId: String) : Messaging(messageId)
    }

    internal open val extras: Map<*, String>?
        get() = null
}
