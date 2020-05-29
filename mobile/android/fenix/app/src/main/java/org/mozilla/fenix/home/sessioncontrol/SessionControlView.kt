/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.content.Context
import android.os.Build
import android.view.View
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.extensions.LayoutContainer
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.top.sites.TopSite
import org.mozilla.fenix.R
import org.mozilla.fenix.home.HomeFragmentState
import org.mozilla.fenix.home.HomeScreenViewModel
import org.mozilla.fenix.home.Mode
import org.mozilla.fenix.home.OnboardingState
import org.mozilla.fenix.home.Tab
import org.mozilla.fenix.components.tips.Tip
import org.mozilla.fenix.ext.settings

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

// This method got a little complex with the addition of the tab tray feature flag
// When we remove the tabs from the home screen this will get much simpler again.
@SuppressWarnings("LongParameterList", "ComplexMethod")
private fun normalModeAdapterItems(
    context: Context,
    tabs: List<Tab>,
    topSites: List<TopSite>,
    collections: List<TabCollection>,
    expandedCollections: Set<Long>,
    tip: Tip?
): List<AdapterItem> {
    val items = mutableListOf<AdapterItem>()

    tip?.let { items.add(AdapterItem.TipItem(it)) }

    if (topSites.isNotEmpty()) {
        items.add(AdapterItem.TopSiteList(topSites))
    }

    val useNewTabTray = context.settings().useNewTabTray

    if (!useNewTabTray) {
        items.add(AdapterItem.TabHeader(false, tabs.isNotEmpty()))
    }

    when {
        tabs.isNotEmpty() && collections.isNotEmpty() -> {
            if (!useNewTabTray) { showTabs(items, tabs) }
            showCollections(collections, expandedCollections, tabs, items)
        }

        tabs.isNotEmpty() && collections.isEmpty() -> {
            if (!useNewTabTray) { showTabs(items, tabs) }
            items.add(AdapterItem.CollectionHeader)
            items.add(noCollectionMessage)
        }

        tabs.isEmpty() && collections.isNotEmpty() -> {
            if (!useNewTabTray) { items.add(noTabMessage) }
            showCollections(collections, expandedCollections, tabs, items)
        }

        tabs.isEmpty() && collections.isEmpty() && !useNewTabTray -> {
            items.add(noTabMessage)
        }

        collections.isEmpty() && useNewTabTray -> {
            items.add(AdapterItem.CollectionHeader)
            items.add(noCollectionMessage)
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

private fun privateModeAdapterItems(context: Context, tabs: List<Tab>): List<AdapterItem> {
    val items = mutableListOf<AdapterItem>()

    val useNewTabTray = context.settings().useNewTabTray

    if (useNewTabTray) {
        items.add(AdapterItem.PrivateBrowsingDescription)
    } else {
        items.add(AdapterItem.TabHeader(true, tabs.isNotEmpty()))

        if (tabs.isNotEmpty()) {
            items.addAll(tabs.reversed().map(AdapterItem::TabItem))
        } else {
            items.add(AdapterItem.PrivateBrowsingDescription)
        }
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

private fun HomeFragmentState.toAdapterList(context: Context): List<AdapterItem> = when (mode) {
    is Mode.Normal -> normalModeAdapterItems(context, tabs, topSites, collections, expandedCollections, tip)
    is Mode.Private -> privateModeAdapterItems(context, tabs)
    is Mode.Onboarding -> onboardingAdapterItems(mode.state)
}

private fun collectionTabItems(collection: TabCollection) = collection.tabs.mapIndexed { index, tab ->
        AdapterItem.TabInCollectionItem(collection, tab, index == collection.tabs.lastIndex)
}

@ExperimentalCoroutinesApi
class SessionControlView(
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
        }
    }

    fun update(state: HomeFragmentState) {
        // Workaround for list not updating until scroll on Android 5 + 6
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
            sessionControlAdapter.submitList(null)
        }

        val stateAdapterList = state.toAdapterList(view.context)

        if (homeScreenViewModel.shouldScrollToTopSites) {
            sessionControlAdapter.submitList(stateAdapterList) {

                val loadedTopSites = stateAdapterList.find { adapterItem ->
                    adapterItem is AdapterItem.TopSiteList && adapterItem.topSites.isNotEmpty()
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
