/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.graphics.Bitmap
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.media.state.MediaState
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * The [Store] for holding the [HomeFragmentState] and applying [HomeFragmentAction]s.
 */
class HomeFragmentStore(
    initialState: HomeFragmentState
) : Store<HomeFragmentState, HomeFragmentAction>(
    initialState, ::homeFragmentStateReducer
)

data class Tab(
    val sessionId: String,
    val url: String,
    val hostname: String,
    val title: String,
    val selected: Boolean? = null,
    var mediaState: MediaState? = null,
    val icon: Bitmap? = null
)

fun List<Tab>.toSessionBundle(sessionManager: SessionManager): List<Session> {
    return this.mapNotNull { sessionManager.findSessionById(it.sessionId) }
}

/**
 * The state for the [HomeFragment].
 *
 * @property collections The list of [TabCollection] to display in the [HomeFragment].
 * @property expandedCollections A set containing the ids of the [TabCollection] that are expanded
 *                               in the [HomeFragment].
 * @property mode The state of the [HomeFragment] UI.
 * @property tabs The list of opened [Tab] in the [HomeFragment].
 * @property topSites The list of [TopSite] in the [HomeFragment].
 */
data class HomeFragmentState(
    val collections: List<TabCollection>,
    val expandedCollections: Set<Long>,
    val mode: Mode,
    val tabs: List<Tab>,
    val topSites: List<TopSite>
) : State

sealed class HomeFragmentAction : Action {
    data class Change(
        val tabs: List<Tab>,
        val topSites: List<TopSite>,
        val mode: Mode,
        val collections: List<TabCollection>
    ) :
        HomeFragmentAction()

    data class CollectionExpanded(val collection: TabCollection, val expand: Boolean) :
        HomeFragmentAction()

    data class CollectionsChange(val collections: List<TabCollection>) : HomeFragmentAction()
    data class ModeChange(val mode: Mode, val tabs: List<Tab> = emptyList()) : HomeFragmentAction()
    data class TabsChange(val tabs: List<Tab>) : HomeFragmentAction()
    data class TopSitesChange(val topSites: List<TopSite>) : HomeFragmentAction()
}

private fun homeFragmentStateReducer(
    state: HomeFragmentState,
    action: HomeFragmentAction
): HomeFragmentState {
    return when (action) {
        is HomeFragmentAction.Change -> state.copy(
            collections = action.collections,
            mode = action.mode,
            tabs = action.tabs,
            topSites = action.topSites
        )
        is HomeFragmentAction.CollectionExpanded -> {
            val newExpandedCollection = state.expandedCollections.toMutableSet()

            if (action.expand) {
                newExpandedCollection.add(action.collection.id)
            } else {
                newExpandedCollection.remove(action.collection.id)
            }

            state.copy(expandedCollections = newExpandedCollection)
        }
        is HomeFragmentAction.CollectionsChange -> state.copy(collections = action.collections)
        is HomeFragmentAction.ModeChange -> state.copy(mode = action.mode, tabs = action.tabs)
        is HomeFragmentAction.TabsChange -> state.copy(tabs = action.tabs)
        is HomeFragmentAction.TopSitesChange -> state.copy(topSites = action.topSites)
    }
}
