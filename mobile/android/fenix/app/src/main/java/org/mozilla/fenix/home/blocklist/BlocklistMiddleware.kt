/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.blocklist

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.home.HomeFragmentAction
import org.mozilla.fenix.home.HomeFragmentState
import org.mozilla.fenix.home.recenttabs.RecentTab

/**
 * This [Middleware] reacts to item removals from the home screen, adding them to a blocklist.
 * Additionally, it reacts to state changes in order to filter them by the blocklist.
 *
 * @param settings Blocklist is stored here as a string set
 */
class BlocklistMiddleware(
    private val blocklistHandler: BlocklistHandler
) : Middleware<HomeFragmentState, HomeFragmentAction> {

    /**
     * Will filter "Change" actions using the blocklist and use "Remove" actions to update
     * the blocklist.
     */
    override fun invoke(
        context: MiddlewareContext<HomeFragmentState, HomeFragmentAction>,
        next: (HomeFragmentAction) -> Unit,
        action: HomeFragmentAction
    ) {
        next(getUpdatedAction(context.state, action))
    }

    private fun getUpdatedAction(
        state: HomeFragmentState,
        action: HomeFragmentAction
    ) = with(blocklistHandler) {
        when (action) {
            is HomeFragmentAction.Change -> {
                action.copy(
                    recentBookmarks = action.recentBookmarks.filteredByBlocklist(),
                    recentTabs = action.recentTabs.filteredByBlocklist(),
                    recentHistory = action.recentHistory.filteredByBlocklist()
                )
            }
            is HomeFragmentAction.RecentTabsChange -> {
                action.copy(
                    recentTabs = action.recentTabs.filteredByBlocklist()
                )
            }
            is HomeFragmentAction.RecentBookmarksChange -> {
                action.copy(
                    recentBookmarks = action.recentBookmarks.filteredByBlocklist()
                )
            }
            is HomeFragmentAction.RecentHistoryChange -> {
                action.copy(recentHistory = action.recentHistory.filteredByBlocklist())
            }
            is HomeFragmentAction.RemoveRecentTab -> {
                if (action.recentTab is RecentTab.Tab) {
                    addUrlToBlocklist(action.recentTab.state.content.url)
                    state.toActionFilteringAllState(this)
                } else {
                    action
                }
            }
            is HomeFragmentAction.RemoveRecentBookmark -> {
                action.recentBookmark.url?.let { url ->
                    addUrlToBlocklist(url)
                    state.toActionFilteringAllState(this)
                } ?: action
            }
            is HomeFragmentAction.RemoveRecentHistoryHighlight -> {
                addUrlToBlocklist(action.highlightUrl)
                state.toActionFilteringAllState(this)
            }
            else -> action
        }
    }

    // When an item is removed from any part of the state, it should also be removed from any other
    // relevant parts that contain it.
    // This is a candidate for refactoring once context receivers lands in Kotlin 1.6.20
    // https://blog.jetbrains.com/kotlin/2022/02/kotlin-1-6-20-m1-released/#prototype-of-context-receivers-for-kotlin-jvm
    private fun HomeFragmentState.toActionFilteringAllState(blocklistHandler: BlocklistHandler) =
        with(blocklistHandler) {
            HomeFragmentAction.Change(
                recentTabs = recentTabs.filteredByBlocklist(),
                recentBookmarks = recentBookmarks.filteredByBlocklist(),
                recentHistory = recentHistory.filteredByBlocklist(),
                topSites = topSites,
                mode = mode,
                collections = collections,
                showCollectionPlaceholder = showCollectionPlaceholder
            )
        }
}
