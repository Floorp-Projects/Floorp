/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.os.Build
import android.view.View
import androidx.lifecycle.ProcessLifecycleOwner
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.extensions.LayoutContainer
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSite
import mozilla.components.lib.state.ext.consumeFrom
import org.mozilla.fenix.R
import org.mozilla.fenix.home.HomeFragmentState
import org.mozilla.fenix.home.HomeFragmentStore
import org.mozilla.fenix.home.HomeScreenViewModel
import org.mozilla.fenix.home.Mode
import org.mozilla.fenix.home.OnboardingState
import org.mozilla.fenix.home.Tab

val noTabMessage = AdapterItem.NoContentMessageWithAction(
    R.string.no_open_tabs_header_2,
    R.string.no_open_tabs_description,
    R.drawable.ic_new,
    R.string.home_screen_shortcut_open_new_tab_2
)

val noCollectionMessage = AdapterItem.NoContentMessage(
    R.string.no_collections_header,
    R.string.collections_description
)

private fun normalModeAdapterItems(
    tabs: List<Tab>,
    topSites: List<TopSite>,
    collections: List<TabCollection>,
    expandedCollections: Set<Long>
): List<AdapterItem> {
    val items = mutableListOf<AdapterItem>()

    if (topSites.isNotEmpty()) {
        items.add(AdapterItem.TopSiteList(topSites))
    }

    items.add(AdapterItem.TabHeader(false, tabs.isNotEmpty()))

    when {
        tabs.isNotEmpty() && collections.isNotEmpty() -> {
            showTabs(items, tabs)
            showCollections(collections, expandedCollections, tabs, items)
        }

        tabs.isNotEmpty() && collections.isEmpty() -> {
            showTabs(items, tabs)
            items.add(AdapterItem.CollectionHeader)
            items.add(noCollectionMessage)
        }

        tabs.isEmpty() && collections.isNotEmpty() -> {
            items.add(noTabMessage)
            showCollections(collections, expandedCollections, tabs, items)
        }

        tabs.isEmpty() && collections.isEmpty() -> {
            items.add(noTabMessage)
        }
    }

    return items
}

private fun showTabs(
    items: MutableList<AdapterItem>,
    tabs: List<Tab>
) {
    items.addAll(tabs.reversed().map(AdapterItem::TabItem))
    items.add(AdapterItem.SaveTabGroup)
}

private fun showCollections(
    collections: List<TabCollection>,
    expandedCollections: Set<Long>,
    tabs: List<Tab>,
    items: MutableList<AdapterItem>
) {
    // If the collection is expanded, we want to add all of its tabs beneath it in the adapter
    items.add(AdapterItem.CollectionHeader)
    collections.map {
        AdapterItem.CollectionItem(it, expandedCollections.contains(it.id), tabs.isNotEmpty())
    }.forEach {
        items.add(it)
        if (it.expanded) {
            items.addAll(collectionTabItems(it.collection))
        }
    }
}

private fun privateModeAdapterItems(tabs: List<Tab>): List<AdapterItem> {
    val items = mutableListOf<AdapterItem>()
    items.add(AdapterItem.TabHeader(true, tabs.isNotEmpty()))

    if (tabs.isNotEmpty()) {
        items.addAll(tabs.reversed().map(AdapterItem::TabItem))
    } else {
        items.add(AdapterItem.PrivateBrowsingDescription)
    }

    return items
}

private fun onboardingAdapterItems(onboardingState: OnboardingState): List<AdapterItem> {
    val items: MutableList<AdapterItem> = mutableListOf(AdapterItem.OnboardingHeader)

    // Customize FxA items based on where we are with the account state:
    items.addAll(when (onboardingState) {
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
    })

    items.addAll(listOf(
        AdapterItem.OnboardingSectionHeader {
            val appName = it.getString(R.string.app_name)
            it.getString(R.string.onboarding_feature_section_header, appName)
        },
        AdapterItem.OnboardingWhatsNew,
        AdapterItem.OnboardingTrackingProtection,
        AdapterItem.OnboardingThemePicker,
        AdapterItem.OnboardingPrivateBrowsing,
        AdapterItem.OnboardingToolbarPositionPicker,
        AdapterItem.OnboardingPrivacyNotice,
        AdapterItem.OnboardingFinish
    ))

    return items
}

private fun HomeFragmentState.toAdapterList(): List<AdapterItem> = when (mode) {
    is Mode.Normal -> normalModeAdapterItems(tabs, topSites, collections, expandedCollections)
    is Mode.Private -> privateModeAdapterItems(tabs)
    is Mode.Onboarding -> onboardingAdapterItems(mode.state)
}

private fun collectionTabItems(collection: TabCollection) = collection.tabs.mapIndexed { index, tab ->
        AdapterItem.TabInCollectionItem(collection, tab, index == collection.tabs.lastIndex)
}

@ExperimentalCoroutinesApi
class SessionControlView(
    private val homeFragmentStore: HomeFragmentStore,
    override val containerView: View?,
    interactor: SessionControlInteractor,
    private var homeScreenViewModel: HomeScreenViewModel
) : LayoutContainer {

    val view: RecyclerView = containerView as RecyclerView

    private val sessionControlAdapter = SessionControlAdapter(interactor)

    init {
        view.apply {
            adapter = sessionControlAdapter
            layoutManager = LinearLayoutManager(containerView!!.context)
            val itemTouchHelper =
                ItemTouchHelper(
                    SwipeToDeleteCallback(
                        interactor
                    )
                )
            itemTouchHelper.attachToRecyclerView(this)

            view.consumeFrom(homeFragmentStore, ProcessLifecycleOwner.get()) {
                update(it)
            }
        }
    }

    fun update(state: HomeFragmentState) {
        // Workaround for list not updating until scroll on Android 5 + 6
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
            sessionControlAdapter.submitList(null)
        }

        val stateAdapterList = state.toAdapterList()

        if (homeScreenViewModel.shouldScrollToTopSites) {
            sessionControlAdapter.submitList(stateAdapterList) {

                val loadedTopSites = stateAdapterList.find { adapterItem ->
                    adapterItem is AdapterItem.TopSiteList && adapterItem.topSites.isNotEmpty()
                }
                loadedTopSites?.run {
                    homeScreenViewModel.shouldScrollToTopSites = false
                    view.scrollToPosition(stateAdapterList.indexOf(this))
                }
            }
        } else {
            sessionControlAdapter.submitList(stateAdapterList)
        }
    }
}
