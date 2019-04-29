package org.mozilla.fenix.collections

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import android.content.Context.INPUT_METHOD_SERVICE
import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputMethodManager
import android.view.inputmethod.InputMethodManager.SHOW_IMPLICIT
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import io.reactivex.Observable
import io.reactivex.Observer
import io.reactivex.functions.Consumer
import kotlinx.android.synthetic.main.component_collection_creation.*
import kotlinx.android.synthetic.main.component_collection_creation.view.*
import org.mozilla.fenix.R
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

    var step: SaveCollectionStep = SaveCollectionStep.SelectTabs
        private set

    private val collectionCreationTabListAdapter = CollectionCreationTabListAdapter(actionEmitter)
    private val collectionSaveListAdapter = SaveCollectionListAdapter(actionEmitter)
    private var selectedTabs: Set<Tab> = setOf()

    init {
        view.select_all_button.setOnClickListener {
            actionEmitter.onNext(CollectionCreationAction.SelectAllTapped)
        }

        view.close_icon.apply {
            increaseTapArea(increaseButtonByDps)
            setOnClickListener {
                actionEmitter.onNext(CollectionCreationAction.Close)
            }
        }

        name_collection_edittext.setOnEditorActionListener { v, actionId, event ->
            if (actionId == EditorInfo.IME_ACTION_DONE) {
                actionEmitter.onNext(
                    CollectionCreationAction.SaveCollectionName(
                        selectedTabs.toList(),
                        v.text.toString()
                    )
                )
                true
            }
            false
        }

        view.add_tabs_button.setOnClickListener {
            actionEmitter.onNext(CollectionCreationAction.SaveTabsToCollection(selectedTabs.toList()))
        }

        view.add_collection_button.setOnClickListener {
            actionEmitter.onNext(CollectionCreationAction.AddNewCollection(selectedTabs.toList()))
        }

        view.tab_list.run {
            adapter = collectionCreationTabListAdapter
            layoutManager = LinearLayoutManager(container.context, RecyclerView.VERTICAL, true)
        }

        view.collections_list.run {
            adapter = collectionSaveListAdapter
            layoutManager = LinearLayoutManager(container.context, RecyclerView.VERTICAL, true)
        }
    }

    override fun updateView() = Consumer<CollectionCreationState> {
        step = it.saveCollectionStep
        when (it.saveCollectionStep) {
            is SaveCollectionStep.SelectTabs -> {
                back_button.setOnClickListener {
                    actionEmitter.onNext(CollectionCreationAction.BackPressed(SaveCollectionStep.SelectTabs))
                }

                name_collection_edittext.visibility = View.GONE
                collections_list.visibility = View.GONE
                add_collection_button.visibility = View.GONE
                divider.visibility = View.GONE

                this.selectedTabs = it.selectedTabs
                collectionCreationTabListAdapter.updateData(it.tabs, it.selectedTabs)

                back_button.text = view.context.getString(R.string.create_collection_select_tabs)

                val buttonText = if (it.selectedTabs.isEmpty()) {
                    view.context.getString(R.string.create_collection_save_to_collection_empty)
                } else {
                    view.context.getString(
                        R.string.create_collection_save_to_collection_full,
                        it.selectedTabs.size
                    )
                }

                tab_list.visibility = View.VISIBLE
                select_all_button.visibility = View.VISIBLE
                add_tabs_button.visibility = View.VISIBLE

                val enableSaveButton = it.selectedTabs.isNotEmpty()
                view.add_tabs_button.isClickable = enableSaveButton

                view.add_tabs_button.contentDescription = buttonText
                view.add_tabs_button_text.text = buttonText
            }
            is SaveCollectionStep.SelectCollection -> {
                back_button.setOnClickListener {
                    actionEmitter.onNext(CollectionCreationAction.BackPressed(SaveCollectionStep.SelectCollection))
                }
                collections_list.visibility = View.VISIBLE
                add_collection_button.visibility = View.VISIBLE
                divider.visibility = View.VISIBLE
                tab_list.visibility = View.GONE
                select_all_button.visibility = View.GONE
                add_tabs_button.visibility = View.GONE
                name_collection_edittext.visibility = View.GONE

                back_button.text =
                    view.context.getString(R.string.create_collection_select_collection)
            }
            is SaveCollectionStep.NameCollection -> {
                back_button.setOnClickListener {
                    actionEmitter.onNext(CollectionCreationAction.BackPressed(SaveCollectionStep.NameCollection))
                }
                name_collection_edittext.visibility = View.VISIBLE
                name_collection_edittext.requestFocus()
                val imm =
                    view.context.getSystemService(INPUT_METHOD_SERVICE) as? InputMethodManager
                imm?.showSoftInput(name_collection_edittext, SHOW_IMPLICIT)
                collections_list.visibility = View.GONE
                add_collection_button.visibility = View.GONE
                divider.visibility = View.GONE

                tab_list.visibility = View.GONE
                select_all_button.visibility = View.GONE
                add_tabs_button.visibility = View.GONE
                back_button.text =
                    view.context.getString(R.string.create_collection_name_collection)
            }
        }
    }

    fun onKey(keyCode: Int, event: KeyEvent?): Boolean {
        if (event?.action == KeyEvent.ACTION_UP && keyCode == KeyEvent.KEYCODE_BACK) {
            when (step) {
                SaveCollectionStep.SelectTabs -> {
                    actionEmitter.onNext(CollectionCreationAction.BackPressed(SaveCollectionStep.SelectTabs))
                }
                SaveCollectionStep.SelectCollection -> {
                    actionEmitter.onNext(CollectionCreationAction.BackPressed(SaveCollectionStep.SelectCollection))
                }
                SaveCollectionStep.NameCollection -> {
                    actionEmitter.onNext(CollectionCreationAction.BackPressed(SaveCollectionStep.NameCollection))
                }
            }
        }
        return true
    }

    companion object {
        private const val increaseButtonByDps = 16
    }
}
