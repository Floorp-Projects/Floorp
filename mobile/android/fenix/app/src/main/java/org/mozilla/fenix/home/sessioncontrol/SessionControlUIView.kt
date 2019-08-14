/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.ItemTouchHelper
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import org.mozilla.fenix.R
import org.mozilla.fenix.mvi.UIView

val noTabMessage = AdapterItem.NoContentMessage(
    R.drawable.ic_tabs,
    R.string.no_open_tabs_header,
    R.string.no_open_tabs_description
)

val noCollectionMessage = AdapterItem.NoContentMessage(
    R.drawable.ic_tab_collection,
    R.string.no_collections_header,
    R.string.collections_description
)

private fun normalModeAdapterItems(
    tabs: List<Tab>,
    collections: List<TabCollection>,
    expandedCollections: Set<Long>
): List<AdapterItem> {
    val items = mutableListOf<AdapterItem>()
    items.add(AdapterItem.TabHeader(false, tabs.isNotEmpty()))

    if (tabs.isNotEmpty()) {
        items.addAll(tabs.reversed().map(AdapterItem::TabItem))
        items.add(AdapterItem.SaveTabGroup)
    } else {
        items.add(noTabMessage)
    }

    items.add(AdapterItem.CollectionHeader)
    if (collections.isNotEmpty()) {

        // If the collection is expanded, we want to add all of its tabs beneath it in the adapter
        collections.map {
            AdapterItem.CollectionItem(it, expandedCollections.contains(it.id), tabs.isNotEmpty())
        }.forEach {
            items.add(it)
            if (it.expanded) {
                items.addAll(collectionTabItems(it.collection))
            }
        }
    } else {
        items.add(noCollectionMessage)
    }

    return items
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
                AdapterItem.OnboardingManualSignIn(onboardingState)
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
        AdapterItem.OnboardingThemePicker,
        AdapterItem.OnboardingTrackingProtection,
        AdapterItem.OnboardingPrivateBrowsing,
        AdapterItem.OnboardingPrivacyNotice,
        AdapterItem.OnboardingFinish
    ))

    return items
}

private fun SessionControlState.toAdapterList(): List<AdapterItem> = when (mode) {
    is Mode.Normal -> normalModeAdapterItems(tabs, collections, expandedCollections)
    is Mode.Private -> privateModeAdapterItems(tabs)
    is Mode.Onboarding -> onboardingAdapterItems(mode.state)
}

private fun collectionTabItems(collection: TabCollection) = collection.tabs.mapIndexed { index, tab ->
        AdapterItem.TabInCollectionItem(collection, tab, index == collection.tabs.lastIndex)
}

class SessionControlUIView(
    container: ViewGroup,
    actionEmitter: Observer<SessionControlAction>,
    changesObservable: Observable<SessionControlChange>
) :
    UIView<SessionControlState, SessionControlAction, SessionControlChange>(
        container,
        actionEmitter,
        changesObservable
    ) {

    override val view: RecyclerView = LayoutInflater.from(container.context)
        .inflate(R.layout.component_session_control, container, true)
        .findViewById(R.id.home_component)

    private val sessionControlAdapter = SessionControlAdapter(actionEmitter)

    init {
        view.apply {
            adapter = sessionControlAdapter
            layoutManager = LinearLayoutManager(container.context)
            val itemTouchHelper =
                ItemTouchHelper(
                    SwipeToDeleteCallback(
                        actionEmitter
                    )
                )
            itemTouchHelper.attachToRecyclerView(this)
        }
    }

    override fun updateView() = Consumer<SessionControlState> {
        sessionControlAdapter.submitList(it.toAdapterList())
    }
}
