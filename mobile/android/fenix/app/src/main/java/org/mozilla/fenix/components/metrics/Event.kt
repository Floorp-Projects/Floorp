/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import org.mozilla.fenix.GleanMetrics.SearchTerms

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

    // Recently visited/Recent searches
    object RecentSearchesGroupDeleted : Event()

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
