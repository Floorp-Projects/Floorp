/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import org.mozilla.fenix.R
import org.mozilla.fenix.mvi.UIView
import androidx.recyclerview.widget.ItemTouchHelper
import org.mozilla.fenix.BuildConfig

private fun normalModeAdapterItems(tabs: List<Tab>, collections: List<TabCollection>): List<AdapterItem> {
    val items = mutableListOf<AdapterItem>()
    items.add(AdapterItem.TabHeader(false, tabs.isNotEmpty()))

    if (tabs.isNotEmpty()) {
        items.addAll(tabs.reversed().map(AdapterItem::TabItem))
        if (BuildConfig.COLLECTIONS_ENABLED) {
            items.add(AdapterItem.SaveTabGroup)
        }
    } else {
        items.add(AdapterItem.NoTabMessage)
    }

    items.add(AdapterItem.CollectionHeader)
    if (collections.isNotEmpty()) {

        // If the collection is expanded, we want to add all of its tabs beneath it in the adapter
        collections.reversed().map(AdapterItem::CollectionItem).forEach {
            items.add(it)
            if (it.collection.expanded) {
                items.addAll(collectionTabItems(it.collection))
            }
        }
    } else {
        items.add(AdapterItem.NoCollectionMessage)
    }

    return items
}

private fun privateModeAdapterItems(tabs: List<Tab>): List<AdapterItem> {
    val items = mutableListOf<AdapterItem>()
    items.add(AdapterItem.TabHeader(true, tabs.isNotEmpty()))

    if (tabs.isNotEmpty()) {
        items.addAll(tabs.reversed().map(AdapterItem::TabItem))
        items.add(AdapterItem.DeleteTabs)
    } else {
        items.add(AdapterItem.PrivateBrowsingDescription)
    }

    return items
}

private fun onboardingAdapterItems(): List<AdapterItem> = listOf(
    AdapterItem.OnboardingHeader,
    AdapterItem.OnboardingSectionHeader() { it.getString(R.string.onboarding_fxa_section_header) },
    AdapterItem.OnboardingFirefoxAccount,
    AdapterItem.OnboardingSectionHeader() {
        val appName = it.getString(R.string.app_name)
        it.getString(R.string.onboarding_feature_section_header, appName)
    },
    AdapterItem.OnboardingThemePicker,
    AdapterItem.OnboardingTrackingProtection,
    AdapterItem.OnboardingPrivateBrowsing,
    AdapterItem.OnboardingPrivacyNotice,
    AdapterItem.OnboardingFinish
)

private fun SessionControlState.toAdapterList(): List<AdapterItem> = when (mode) {
    is Mode.Normal -> normalModeAdapterItems(tabs, collections)
    is Mode.Private -> privateModeAdapterItems(tabs)
    is Mode.Onboarding -> onboardingAdapterItems()
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
        sessionControlAdapter.reloadData(it.toAdapterList())

        // There is a current bug in the combination of MotionLayout~alhpa4 and RecyclerView where it doesn't think
        // it has to redraw itself. For some reason calling scrollBy forces this to happen every time
        // https://stackoverflow.com/a/42549611
        view.scrollBy(0, 0)
    }
}
