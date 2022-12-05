/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import android.os.Handler
import android.os.Looper
import android.text.InputFilter
import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.ViewGroup
import android.view.inputmethod.EditorInfo
import androidx.constraintlayout.widget.ConstraintSet
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.transition.AutoTransition
import androidx.transition.Transition
import androidx.transition.TransitionManager
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.ktx.android.view.showKeyboard
import mozilla.components.support.ktx.kotlin.toShortUrl
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.Collections
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.ComponentCollectionCreationBinding
import org.mozilla.fenix.ext.components

class CollectionCreationView(
    private val container: ViewGroup,
    private val interactor: CollectionCreationInteractor,
) {

    private val binding = ComponentCollectionCreationBinding.inflate(
        LayoutInflater.from(container.context),
        container,
        true,
    )

    private val bottomBarView = CollectionCreationBottomBarView(
        interactor = interactor,
        layout = binding.bottomButtonBarLayout,
        iconButton = binding.bottomBarIconButton,
        textView = binding.bottomBarText,
        saveButton = binding.saveButton,
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
        transition.excludeTarget(binding.backButton, true)

        binding.nameCollectionEdittext.filters += InputFilter.LengthFilter(
            COLLECTION_NAME_MAX_LENGTH,
        )
        binding.nameCollectionEdittext.setOnEditorActionListener { view, actionId, _ ->
            val text = view.text.toString()
            if (actionId == EditorInfo.IME_ACTION_DONE && text.isNotBlank()) {
                when (step) {
                    SaveCollectionStep.NameCollection ->
                        interactor.onNewCollectionNameSaved(selectedTabs.toList(), text)
                    SaveCollectionStep.RenameCollection ->
                        selectedCollection?.let { interactor.onCollectionRenamed(it, text) }
                    else -> { /* noop */
                    }
                }
            }
            false
        }

        binding.tabList.run {
            adapter = collectionCreationTabListAdapter
            itemAnimator = null
            layoutManager = LinearLayoutManager(container.context, RecyclerView.VERTICAL, true)
        }

        binding.collectionsList.run {
            adapter = collectionSaveListAdapter
            layoutManager = LinearLayoutManager(container.context, RecyclerView.VERTICAL, true)
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
        Collections.tabSelectOpened.record(NoExtras())

        binding.tabList.isClickable = true

        binding.backButton.apply {
            text = context.getString(R.string.create_collection_select_tabs)
            setOnClickListener {
                interactor.onBackPressed(SaveCollectionStep.SelectTabs)
            }
        }

        binding.selectAllButton.apply {
            val allSelected = state.selectedTabs.size == state.tabs.size

            text = if (allSelected) {
                context.getString(R.string.create_collection_deselect_all)
            } else {
                context.getString(R.string.create_collection_select_all)
            }

            setOnClickListener {
                if (allSelected) {
                    interactor.deselectAllTapped()
                } else {
                    interactor.selectAllTapped()
                }
            }
        }

        selectTabsConstraints.clone(
            container.context,
            R.layout.component_collection_creation,
        )
        collectionCreationTabListAdapter.updateData(state.tabs, state.selectedTabs)
        selectTabsConstraints.applyTo(binding.collectionConstraintLayout)
    }

    private fun updateForSelectCollection() {
        binding.tabList.isClickable = false
        selectCollectionConstraints.clone(
            container.context,
            R.layout.component_collection_creation_select_collection,
        )
        selectCollectionConstraints.applyTo(binding.collectionConstraintLayout)

        binding.backButton.apply {
            text = context.getString(R.string.create_collection_select_collection)
            setOnClickListener {
                interactor.onBackPressed(SaveCollectionStep.SelectCollection)
            }
        }
        TransitionManager.beginDelayedTransition(binding.collectionConstraintLayout, transition)
    }

    private fun updateForNameCollection(state: CollectionCreationState) {
        binding.tabList.isClickable = false
        nameCollectionConstraints.clone(
            container.context,
            R.layout.component_collection_creation_name_collection,
        )
        nameCollectionConstraints.applyTo(binding.collectionConstraintLayout)

        collectionCreationTabListAdapter.updateData(
            state.selectedTabs.toList(),
            state.selectedTabs,
            true,
        )
        binding.backButton.apply {
            text = context.getString(R.string.create_collection_name_collection)
            setOnClickListener {
                binding.nameCollectionEdittext.hideKeyboard()
                val handler = Handler(Looper.getMainLooper())
                handler.postDelayed(
                    {
                        interactor.onBackPressed(SaveCollectionStep.NameCollection)
                    },
                    TRANSITION_DURATION,
                )
            }
        }

        binding.nameCollectionEdittext.showKeyboard()

        binding.nameCollectionEdittext.setText(
            container.context.getString(
                R.string.create_collection_default_name,
                state.defaultCollectionNumber,
            ),
        )
        binding.nameCollectionEdittext.setSelection(0, binding.nameCollectionEdittext.text.length)
    }

    private fun updateForRenameCollection(state: CollectionCreationState) {
        binding.tabList.isClickable = false

        state.selectedTabCollection?.let { tabCollection ->
            val publicSuffixList = container.context.components.publicSuffixList
            tabCollection.tabs.map { tab ->
                Tab(
                    sessionId = tab.id.toString(),
                    url = tab.url,
                    hostname = tab.url.toShortUrl(publicSuffixList),
                    title = tab.title,
                )
            }.let { tabs ->
                collectionCreationTabListAdapter.updateData(tabs, tabs.toSet(), true)
            }
        }
        nameCollectionConstraints.clone(
            container.context,
            R.layout.component_collection_creation_name_collection,
        )
        nameCollectionConstraints.applyTo(binding.collectionConstraintLayout)
        binding.nameCollectionEdittext.setText(state.selectedTabCollection?.title)
        binding.nameCollectionEdittext.setSelection(0, binding.nameCollectionEdittext.text.length)

        binding.backButton.apply {
            text = context.getString(R.string.collection_rename)
            setOnClickListener {
                binding.nameCollectionEdittext.hideKeyboard()
                val handler = Handler(Looper.getMainLooper())
                handler.postDelayed(
                    {
                        interactor.onBackPressed(SaveCollectionStep.RenameCollection)
                    },
                    TRANSITION_DURATION,
                )
            }
        }
        transition.addListener(
            object : Transition.TransitionListener {
                override fun onTransitionStart(transition: Transition) { /* noop */
                }

                override fun onTransitionEnd(transition: Transition) {
                    binding.nameCollectionEdittext.showKeyboard()
                    transition.removeListener(this)
                }

                override fun onTransitionCancel(transition: Transition) { /* noop */
                }

                override fun onTransitionPause(transition: Transition) { /* noop */
                }

                override fun onTransitionResume(transition: Transition) { /* noop */
                }
            },
        )
        TransitionManager.beginDelayedTransition(binding.collectionConstraintLayout, transition)
    }

    fun onResumed() {
        if (step == SaveCollectionStep.NameCollection || step == SaveCollectionStep.RenameCollection) {
            binding.nameCollectionEdittext.showKeyboard()
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
