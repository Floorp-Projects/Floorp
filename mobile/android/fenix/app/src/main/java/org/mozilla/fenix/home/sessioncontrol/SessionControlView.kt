/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.content.Context
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.LifecycleOwner
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.service.pocket.PocketRecommendedStory
import org.mozilla.fenix.components.tips.Tip
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.historymetadata.HistoryMetadataGroup
import org.mozilla.fenix.home.HomeFragmentState
import org.mozilla.fenix.home.HomeFragmentStore
import org.mozilla.fenix.home.HomeScreenViewModel
import org.mozilla.fenix.home.Mode
import org.mozilla.fenix.home.OnboardingState
import org.mozilla.fenix.home.recenttabs.view.RecentTabsItemPosition

// This method got a little complex with the addition of the tab tray feature flag
// When we remove the tabs from the home screen this will get much simpler again.
@Suppress("ComplexMethod", "LongParameterList")
private fun normalModeAdapterItems(
    context: Context,
    topSites: List<TopSite>,
    collections: List<TabCollection>,
    expandedCollections: Set<Long>,
    tip: Tip?,
    recentBookmarks: List<BookmarkNode>,
    showCollectionsPlaceholder: Boolean,
    showSetAsDefaultBrowserCard: Boolean,
    recentTabs: List<TabSessionState>,
    historyMetadata: List<HistoryMetadataGroup>,
    pocketArticles: List<PocketRecommendedStory>
): List<AdapterItem> {
    val items = mutableListOf<AdapterItem>()

    tip?.let { items.add(AdapterItem.TipItem(it)) }

    if (showSetAsDefaultBrowserCard) {
        items.add(AdapterItem.ExperimentDefaultBrowserCard)
    }

    if (topSites.isNotEmpty()) {
        items.add(AdapterItem.TopSitePager(topSites))
    }

    if (recentTabs.isNotEmpty()) {
        showRecentTabs(recentTabs, items)
    }

    if (recentBookmarks.isNotEmpty()) {
        items.add(AdapterItem.RecentBookmarks(recentBookmarks))
    }

    if (historyMetadata.isNotEmpty()) {
        showHistoryMetadata(historyMetadata, items)
    }

    if (collections.isEmpty()) {
        if (showCollectionsPlaceholder) {
            items.add(AdapterItem.NoCollectionsMessage)
        }
    } else {
        showCollections(collections, expandedCollections, items)
    }

    if (context.settings().pocketRecommendations && pocketArticles.isNotEmpty()) {
        items.add(AdapterItem.PocketStoriesItem)
    }

    return items
}

/**
 * Constructs the list of items to be shown in the recent tabs section.
 *
 * This section's structure is:
 * - section header
 * - one or more normal tabs
 * - zero or one media tab (if there is a tab opened on which media started playing.
 * This may be a duplicate of one of the normal tabs shown above).
 */
@VisibleForTesting
internal fun showRecentTabs(
    recentTabs: List<TabSessionState>,
    items: MutableList<AdapterItem>
) {
    items.add(AdapterItem.RecentTabsHeader)

    recentTabs.forEachIndexed { index, recentTab ->
        // If this is the first tab to be shown but more will follow.
        if (index == 0 && recentTabs.size > 1) {
            items.add(AdapterItem.RecentTabItem(recentTab, RecentTabsItemPosition.TOP))
        }

        // if this is the only tab to be shown.
        else if (index == 0 && recentTabs.size == 1) {
            items.add(AdapterItem.RecentTabItem(recentTab, RecentTabsItemPosition.SINGLE))
        }

        // If there are items above and below.
        else if (index < recentTabs.size - 1) {
            items.add(AdapterItem.RecentTabItem(recentTab, RecentTabsItemPosition.MIDDLE))
        }

        // If this is the last recent tab to be shown.
        else if (index < recentTabs.size) {
            items.add(AdapterItem.RecentTabItem(recentTab, RecentTabsItemPosition.BOTTOM))
        }
    }
}

private fun showHistoryMetadata(
    historyMetadata: List<HistoryMetadataGroup>,
    items: MutableList<AdapterItem>
) {
    items.add(AdapterItem.HistoryMetadataHeader)

    historyMetadata.forEach { container ->
        items.add(AdapterItem.HistoryMetadataGroup(historyMetadataGroup = container))

        if (container.expanded) {
            container.historyMetadata.forEach {
                items.add(AdapterItem.HistoryMetadataItem(it))
            }
        }
    }
}

private fun showCollections(
    collections: List<TabCollection>,
    expandedCollections: Set<Long>,
    items: MutableList<AdapterItem>
) {
    // If the collection is expanded, we want to add all of its tabs beneath it in the adapter
    items.add(AdapterItem.CollectionHeader)
    collections.map {
        AdapterItem.CollectionItem(it, expandedCollections.contains(it.id))
    }.forEach {
        items.add(it)
        if (it.expanded) {
            items.addAll(collectionTabItems(it.collection))
        }
    }
}

private fun privateModeAdapterItems() = listOf(AdapterItem.PrivateBrowsingDescription)

private fun onboardingAdapterItems(onboardingState: OnboardingState): List<AdapterItem> {
    val items: MutableList<AdapterItem> = mutableListOf(AdapterItem.OnboardingHeader)

    items.addAll(
        listOf(
            AdapterItem.OnboardingThemePicker,
            AdapterItem.OnboardingToolbarPositionPicker,
            AdapterItem.OnboardingTrackingProtection
        )
    )
    // Customize FxA items based on where we are with the account state:
    items.addAll(
        when (onboardingState) {
            OnboardingState.SignedOutNoAutoSignIn -> {
                listOf(
                    AdapterItem.OnboardingManualSignIn
                )
            }
            is OnboardingState.SignedOutCanAutoSignIn -> {
                listOf(
                    AdapterItem.OnboardingAutomaticSignIn(onboardingState)
                )
            }
            OnboardingState.SignedIn -> listOf()
        }
    )

    items.addAll(
        listOf(
            AdapterItem.OnboardingPrivacyNotice,
            AdapterItem.OnboardingFinish
        )
    )

    return items
}

private fun HomeFragmentState.toAdapterList(context: Context): List<AdapterItem> = when (mode) {
    is Mode.Normal -> normalModeAdapterItems(
        context,
        topSites,
        collections,
        expandedCollections,
        tip,
        recentBookmarks,
        showCollectionPlaceholder,
        showSetAsDefaultBrowserCard,
        recentTabs,
        historyMetadata,
        pocketArticles
    )
    is Mode.Private -> privateModeAdapterItems()
    is Mode.Onboarding -> onboardingAdapterItems(mode.state)
}

private fun collectionTabItems(collection: TabCollection) =
    collection.tabs.mapIndexed { index, tab ->
        AdapterItem.TabInCollectionItem(collection, tab, index == collection.tabs.lastIndex)
    }

class SessionControlView(
    store: HomeFragmentStore,
    val containerView: View,
    viewLifecycleOwner: LifecycleOwner,
    interactor: SessionControlInteractor,
    private var homeScreenViewModel: HomeScreenViewModel
) {

    val view: RecyclerView = containerView as RecyclerView

    private val sessionControlAdapter = SessionControlAdapter(
        store,
        interactor,
        viewLifecycleOwner,
        containerView.context.components
    )

    init {
        view.apply {
            adapter = sessionControlAdapter
            layoutManager = LinearLayoutManager(containerView.context)
            val itemTouchHelper =
                ItemTouchHelper(
                    SwipeToDeleteCallback(
                        interactor
                    )
                )
            itemTouchHelper.attachToRecyclerView(this)
        }
    }

    fun update(state: HomeFragmentState) {
        val stateAdapterList = state.toAdapterList(view.context)
        if (homeScreenViewModel.shouldScrollToTopSites) {
            sessionControlAdapter.submitList(stateAdapterList) {

                val loadedTopSites = stateAdapterList.find { adapterItem ->
                    adapterItem is AdapterItem.TopSitePager && adapterItem.topSites.isNotEmpty()
                }
                loadedTopSites?.run {
                    homeScreenViewModel.shouldScrollToTopSites = false
                    view.scrollToPosition(0)
                }
            }
        } else {
            sessionControlAdapter.submitList(stateAdapterList)
        }
    }
}
