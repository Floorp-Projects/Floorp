package org.mozilla.fenix.collections

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import io.reactivex.Observer
import io.reactivex.Observable
import io.reactivex.functions.Consumer
import kotlinx.android.synthetic.main.component_collection_creation.view.*
import org.mozilla.fenix.ext.increaseTapArea
import org.mozilla.fenix.mvi.UIView

class CollectionCreationUIView(
    container: ViewGroup,
    actionEmitter: Observer<CollectionCreationAction>,
    changesObservable: Observable<CollectionCreationChange>
) : UIView<CollectionCreationState, CollectionCreationAction, CollectionCreationChange>(
    container,
    actionEmitter,
    changesObservable
) {
    override val view = LayoutInflater.from(container.context)
        .inflate(R.layout.component_collection_creation, container, true)

    private val collectionCreationTabListAdapter = CollectionCreationTabListAdapter(actionEmitter)

    init {
        view.back_button.setOnClickListener {
            actionEmitter.onNext(CollectionCreationAction.Close)
        }

        view.select_all_button.setOnClickListener {
            actionEmitter.onNext(CollectionCreationAction.SelectAllTapped)
        }

        view.close_icon.apply {
            increaseTapArea(increaseButtonByDps)
            setOnClickListener {
                actionEmitter.onNext(CollectionCreationAction.Close)
            }
        }

        view.tab_list.run {
            adapter = collectionCreationTabListAdapter
            layoutManager = LinearLayoutManager(container.context, RecyclerView.VERTICAL, true)
        }
    }

    override fun updateView() = Consumer<CollectionCreationState> {
        collectionCreationTabListAdapter.updateData(it.tabs, it.selectedTabs)

        val buttonText = if (it.selectedTabs.isEmpty()) {
            view.context.getString(R.string.create_collection_save_to_collection_empty)
        } else {
            view.context.getString(R.string.create_collection_save_to_collection_full, it.selectedTabs.size)
        }

        view.add_tabs_button.contentDescription = buttonText
        view.add_tabs_button_text.text = buttonText
    }

    companion object {
        private const val increaseButtonByDps = 16
    }
}