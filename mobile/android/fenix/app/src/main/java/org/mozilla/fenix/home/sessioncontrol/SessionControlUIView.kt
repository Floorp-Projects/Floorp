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

// Convert HomeState into a data structure HomeAdapter understands
@SuppressWarnings("ComplexMethod", "NestedBlockDepth")
private fun SessionControlState.toAdapterList(): List<AdapterItem> {
    val items = mutableListOf<AdapterItem>()
    items.add(AdapterItem.TabHeader)

    // Populate tabs
    if (tabs.isNotEmpty()) {
        tabs.reversed().map(AdapterItem::TabItem).forEach { items.add(it) }
        if (mode == Mode.Private) {
            items.add(AdapterItem.DeleteTabs)
        } else if (BuildConfig.COLLECTIONS_ENABLED) {
            items.add(AdapterItem.SaveTabGroup)
        }
    } else {
        val item = if (mode == Mode.Private) AdapterItem.PrivateBrowsingDescription
                    else AdapterItem.NoTabMessage

        items.add(item)
    }

    // Populate collections
    if (mode == Mode.Normal) {
        items.add(AdapterItem.CollectionHeader)
        if (collections.isNotEmpty()) {

            // If the collection is expanded, we want to add all of its tabs beneath it in the adapter
            collections.reversed().map(AdapterItem::CollectionItem).forEach {
                if (it.collection.expanded) {
                    items.add(it)
                    addCollectionTabItems(it.collection, it.collection.tabs, items)
                } else {
                    items.add(it)
                }
            }
        } else {
            items.add(AdapterItem.NoCollectionMessage)
        }
    }

    return items
}

private fun addCollectionTabItems(
    collection: TabCollection,
    tabs: MutableList<Tab>,
    itemList: MutableList<AdapterItem>
) {
    for (tabIndex in 0 until tabs.size) {
        itemList.add(AdapterItem.TabInCollectionItem
            (collection, collection.tabs[tabIndex], tabIndex == collection.tabs.size - 1))
    }
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
