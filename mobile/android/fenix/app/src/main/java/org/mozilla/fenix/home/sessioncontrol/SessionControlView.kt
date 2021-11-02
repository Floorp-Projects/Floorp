/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.LifecycleOwner
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.concept.storage.BookmarkNode
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
import org.mozilla.fenix.home.recenttabs.RecentTab
import org.mozilla.fenix.onboarding.JumpBackInCFRDialog
import org.mozilla.fenix.utils.Settings

// This method got a little complex with the addition of the tab tray feature flag
// When we remove the tabs from the home screen this will get much simpler again.
@Suppress("ComplexMethod", "LongParameterList")
@VisibleForTesting
internal fun normalModeAdapterItems(
    topSites: List<TopSite>,
    collections: List<TabCollection>,
    expandedCollections: Set<Long>,
    tip: Tip?,
    recentBookmarks: List<BookmarkNode>,
    showCollectionsPlaceholder: Boolean,
    showSetAsDefaultBrowserCard: Boolean,
    recentTabs: List<RecentTab>,
    historyMetadata: List<HistoryMetadataGroup>,
    pocketStories: List<PocketRecommendedStory>
): List<AdapterItem> {
    val items = mutableListOf<AdapterItem>()
    var shouldShowCustomizeHome = false

    tip?.let { items.add(AdapterItem.TipItem(it)) }

    if (showSetAsDefaultBrowserCard) {
        items.add(AdapterItem.ExperimentDefaultBrowserCard)
    }

    if (topSites.isNotEmpty()) {
        items.add(AdapterItem.TopSitePager(topSites))
    }

    if (recentTabs.isNotEmpty()) {
        shouldShowCustomizeHome = true
        items.add(AdapterItem.RecentTabsHeader)
        items.add(AdapterItem.RecentTabItem)
    }

    if (recentBookmarks.isNotEmpty()) {
        shouldShowCustomizeHome = true
        items.add(AdapterItem.RecentBookmarks(recentBookmarks))
    }

    if (historyMetadata.isNotEmpty()) {
        shouldShowCustomizeHome = true
        items.add(AdapterItem.HistoryMetadataHeader)
        items.add(AdapterItem.HistoryMetadataGroup)
    }

    if (collections.isEmpty()) {
        if (showCollectionsPlaceholder) {
            items.add(AdapterItem.NoCollectionsMessage)
        }
    } else {
        showCollections(collections, expandedCollections, items)
    }

    if (pocketStories.isNotEmpty()) {
        shouldShowCustomizeHome = true
        items.add(AdapterItem.PocketStoriesItem)
    }

    if (shouldShowCustomizeHome) {
        items.add(AdapterItem.CustomizeHomeButton)
    }

    return items
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

private fun HomeFragmentState.toAdapterList(): List<AdapterItem> = when (mode) {
    is Mode.Normal -> normalModeAdapterItems(
        topSites,
        collections,
        expandedCollections,
        tip,
        recentBookmarks,
        showCollectionPlaceholder,
        showSetAsDefaultBrowserCard,
        recentTabs,
        historyMetadata,
        pocketStories
    )
    is Mode.Private -> privateModeAdapterItems()
    is Mode.Onboarding -> onboardingAdapterItems(mode.state)
}

@VisibleForTesting
internal fun HomeFragmentState.shouldShowHomeOnboardingDialog(settings: Settings): Boolean {
    val isAnySectionsVisible = recentTabs.isNotEmpty() || recentBookmarks.isNotEmpty() ||
        historyMetadata.isNotEmpty() || pocketStories.isNotEmpty()
    return isAnySectionsVisible && !settings.hasShownHomeOnboardingDialog
}

private fun collectionTabItems(collection: TabCollection) =
    collection.tabs.mapIndexed { index, tab ->
        AdapterItem.TabInCollectionItem(collection, tab, index == collection.tabs.lastIndex)
    }

class SessionControlView(
    store: HomeFragmentStore,
    val containerView: View,
    viewLifecycleOwner: LifecycleOwner,
    internal val interactor: SessionControlInteractor,
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
            layoutManager = object : LinearLayoutManager(containerView.context) {
                override fun onLayoutCompleted(state: RecyclerView.State?) {
                    super.onLayoutCompleted(state)

                    JumpBackInCFRDialog(view).showIfNeeded()
                }
            }
            val itemTouchHelper =
                ItemTouchHelper(
                    SwipeToDeleteCallback(
                        interactor
                    )
                )
            itemTouchHelper.attachToRecyclerView(this)
        }
    }

    fun update(state: HomeFragmentState, shouldReportMetrics: Boolean = false) {
        if (state.shouldShowHomeOnboardingDialog(view.context.settings())) {
            interactor.showOnboardingDialog()
        }

        if (shouldReportMetrics) interactor.reportSessionMetrics(state)

        val stateAdapterList = state.toAdapterList()
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
