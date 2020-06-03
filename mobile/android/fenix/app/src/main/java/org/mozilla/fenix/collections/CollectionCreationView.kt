/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import android.os.Handler
import android.text.InputFilter
import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import androidx.constraintlayout.widget.ConstraintSet
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.transition.AutoTransition
import androidx.transition.Transition
import androidx.transition.TransitionManager
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.component_collection_creation.*
import mozilla.components.browser.state.state.MediaState
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.ktx.android.view.showKeyboard
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.toShortUrl
import org.mozilla.fenix.home.Tab

class CollectionCreationView(
    container: ViewGroup,
    private val interactor: CollectionCreationInteractor
) : LayoutContainer {

    override val containerView: View = LayoutInflater.from(container.context)
        .inflate(R.layout.component_collection_creation, container, true)

    private val bottomBarView = CollectionCreationBottomBarView(
        interactor = interactor,
        layout = bottom_button_bar_layout,
        iconButton = bottom_bar_icon_button,
        textView = bottom_bar_text,
        saveButton = save_button
    )
    private val collectionCreationTabListAdapter = CollectionCreationTabListAdapter(interactor)
    private val collectionSaveListAdapter = SaveCollectionListAdapter(interactor)
    private val selectTabsConstraints = ConstraintSet()
    private val selectCollectionConstraints = ConstraintSet()
    private val nameCollectionConstraints = ConstraintSet()
    private val transition = AutoTransition()

    private var selectedCollection: TabCollection? = null
    private var selectedTabs: Set<Tab> = setOf()
    var step: SaveCollectionStep = SaveCollectionStep.SelectTabs
        private set

    init {
        transition.duration = TRANSITION_DURATION
        transition.excludeTarget(back_button, true)

        name_collection_edittext.filters += InputFilter.LengthFilter(COLLECTION_NAME_MAX_LENGTH)
        name_collection_edittext.setOnEditorActionListener { view, actionId, _ ->
            val text = view.text.toString()
            if (actionId == EditorInfo.IME_ACTION_DONE && text.isNotBlank()) {
                when (step) {
                    SaveCollectionStep.NameCollection ->
                        interactor.onNewCollectionNameSaved(selectedTabs.toList(), text)
                    SaveCollectionStep.RenameCollection ->
                        selectedCollection?.let { interactor.onCollectionRenamed(it, text) }
                    else -> { /* noop */ }
                }
            }
            false
        }

        tab_list.run {
            adapter = collectionCreationTabListAdapter
            itemAnimator = null
            layoutManager = LinearLayoutManager(containerView.context, RecyclerView.VERTICAL, true)
        }

        collections_list.run {
            adapter = collectionSaveListAdapter
            layoutManager = LinearLayoutManager(containerView.context, RecyclerView.VERTICAL, true)
        }
    }

    fun update(state: CollectionCreationState) {

        cacheState(state)

        bottomBarView.update(step, state)
        when (step) {
            SaveCollectionStep.SelectTabs -> updateForSelectTabs(state)
            SaveCollectionStep.SelectCollection -> updateForSelectCollection()
            SaveCollectionStep.NameCollection -> updateForNameCollection(state)
            SaveCollectionStep.RenameCollection -> updateForRenameCollection(state)
        }

        collectionSaveListAdapter.updateData(state.tabCollections, state.selectedTabs)
    }

    private fun cacheState(state: CollectionCreationState) {
        step = state.saveCollectionStep
        selectedTabs = state.selectedTabs
        selectedCollection = state.selectedTabCollection
    }

    private fun updateForSelectTabs(state: CollectionCreationState) {
        containerView.context.components.analytics.metrics.track(Event.CollectionTabSelectOpened)

        tab_list.isClickable = true

        back_button.apply {
            text = context.getString(R.string.create_collection_select_tabs)
            setOnClickListener {
                interactor.onBackPressed(SaveCollectionStep.SelectTabs)
            }
        }

        select_all_button.apply {
            val allSelected = state.selectedTabs.size == state.tabs.size
            text =
                if (allSelected) context.getString(R.string.create_collection_deselect_all)
                else context.getString(R.string.create_collection_select_all)
            setOnClickListener {
                if (allSelected) interactor.deselectAllTapped()
                else interactor.selectAllTapped()
            }
        }

        selectTabsConstraints.clone(
            containerView.context,
            R.layout.component_collection_creation
        )
        collectionCreationTabListAdapter.updateData(state.tabs, state.selectedTabs)
        selectTabsConstraints.applyTo(collection_constraint_layout)
    }

    private fun updateForSelectCollection() {
        tab_list.isClickable = false

        back_button.setOnClickListener {
            interactor.onBackPressed(SaveCollectionStep.SelectCollection)
        }
        TransitionManager.beginDelayedTransition(collection_constraint_layout, transition)
        selectCollectionConstraints.clone(
            containerView.context,
            R.layout.component_collection_creation_select_collection
        )
        selectCollectionConstraints.applyTo(collection_constraint_layout)
    }

    private fun updateForNameCollection(state: CollectionCreationState) {
        tab_list.isClickable = false

        collectionCreationTabListAdapter.updateData(state.selectedTabs.toList(), state.selectedTabs, true)
        back_button.apply {
            text = context.getString(R.string.create_collection_name_collection)
            setOnClickListener {
                name_collection_edittext.hideKeyboard()
                val handler = Handler()
                handler.postDelayed({
                    interactor.onBackPressed(SaveCollectionStep.NameCollection)
                }, TRANSITION_DURATION)
            }
        }

        name_collection_edittext.showKeyboard()
        nameCollectionConstraints.clone(
            containerView.context,
            R.layout.component_collection_creation_name_collection
        )
        nameCollectionConstraints.applyTo(collection_constraint_layout)
        name_collection_edittext.setText(
            containerView.context.getString(
                R.string.create_collection_default_name,
                state.defaultCollectionNumber
            )
        )
        name_collection_edittext.setSelection(0, name_collection_edittext.text.length)
    }

    private fun updateForRenameCollection(state: CollectionCreationState) {
        tab_list.isClickable = false

        state.selectedTabCollection?.let { tabCollection ->
            val publicSuffixList = containerView.context.components.publicSuffixList
            tabCollection.tabs.map { tab ->
                Tab(
                    sessionId = tab.id.toString(),
                    url = tab.url,
                    hostname = tab.url.toShortUrl(publicSuffixList),
                    title = tab.title,
                    mediaState = MediaState.State.NONE
                )
            }.let { tabs ->
                collectionCreationTabListAdapter.updateData(tabs, tabs.toSet(), true)
            }
        }
        nameCollectionConstraints.clone(
            containerView.context,
            R.layout.component_collection_creation_name_collection
        )
        nameCollectionConstraints.applyTo(collection_constraint_layout)
        name_collection_edittext.setText(state.selectedTabCollection?.title)
        name_collection_edittext.setSelection(0, name_collection_edittext.text.length)

        back_button.apply {
            text = context.getString(R.string.collection_rename)
            setOnClickListener {
                name_collection_edittext.hideKeyboard()
                val handler = Handler()
                handler.postDelayed({
                    interactor.onBackPressed(SaveCollectionStep.RenameCollection)
                }, TRANSITION_DURATION)
            }
        }
        transition.addListener(object : Transition.TransitionListener {
            override fun onTransitionStart(transition: Transition) { /* noop */ }

            override fun onTransitionEnd(transition: Transition) {
                name_collection_edittext.showKeyboard()
                transition.removeListener(this)
            }

            override fun onTransitionCancel(transition: Transition) { /* noop */ }
            override fun onTransitionPause(transition: Transition) { /* noop */ }
            override fun onTransitionResume(transition: Transition) { /* noop */ }
        })
        TransitionManager.beginDelayedTransition(collection_constraint_layout, transition)
    }

    fun onResumed() {
        if (step == SaveCollectionStep.NameCollection || step == SaveCollectionStep.RenameCollection) {
            name_collection_edittext.showKeyboard()
        }
    }

    fun onKey(keyCode: Int, event: KeyEvent): Boolean {
        return if (event.action == KeyEvent.ACTION_UP && keyCode == KeyEvent.KEYCODE_BACK) {
            interactor.onBackPressed(step)
            true
        } else {
            false
        }
    }

    companion object {
        private const val TRANSITION_DURATION = 200L
        private const val COLLECTION_NAME_MAX_LENGTH = 128
    }
}
