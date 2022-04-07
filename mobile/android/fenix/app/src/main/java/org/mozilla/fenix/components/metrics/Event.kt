/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import org.mozilla.fenix.GleanMetrics.Autoplay
import org.mozilla.fenix.GleanMetrics.ContextMenu
import org.mozilla.fenix.GleanMetrics.SearchTerms
import java.util.Locale

sealed class Event {

    // Interaction Events
    object OpenedAppFirstRun : Event()
    object InteractWithSearchURLArea : Event()
    object DismissedOnboarding : Event()
    object ClearedPrivateData : Event()
    object AddBookmark : Event()
    object HistoryHighlightOpened : Event()
    object HistorySearchGroupOpened : Event()
    object SearchWidgetInstalled : Event()

    object ProgressiveWebAppOpenFromHomescreenTap : Event()
    object ProgressiveWebAppInstallAsShortcut : Event()

    object TabSettingsOpened : Event()

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
