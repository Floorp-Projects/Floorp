/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.fragment.app.DialogFragment
import androidx.lifecycle.lifecycleScope
import androidx.navigation.fragment.navArgs
import kotlinx.android.synthetic.main.fragment_create_collection.view.*
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.session.SessionManager
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.lib.state.ext.consumeFrom
import org.mozilla.fenix.R
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.toTab
import org.mozilla.fenix.home.Tab

@ExperimentalCoroutinesApi
class CollectionCreationFragment : DialogFragment() {
    private lateinit var collectionCreationView: CollectionCreationView
    private lateinit var collectionCreationStore: CollectionCreationStore
    private lateinit var collectionCreationInteractor: CollectionCreationInteractor

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        isCancelable = false
        setStyle(STYLE_NO_TITLE, R.style.CreateCollectionDialogStyle)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.fragment_create_collection, container, false)
        val args: CollectionCreationFragmentArgs by navArgs()

        val sessionManager = requireComponents.core.sessionManager
        val publicSuffixList = requireComponents.publicSuffixList
        val tabs = sessionManager.getTabs(args.tabIds, publicSuffixList)
        val selectedTabs = if (args.selectedTabIds != null) {
            sessionManager.getTabs(args.selectedTabIds, publicSuffixList).toSet()
        } else {
            if (tabs.size == 1) setOf(tabs.first()) else emptySet()
        }

        val tabCollections = requireComponents.core.tabCollectionStorage.cachedTabCollections
        val selectedTabCollection = args.selectedTabCollectionId
            .let { id -> tabCollections.firstOrNull { it.id == id } }

        collectionCreationStore = StoreProvider.get(this) {
            CollectionCreationStore(
                CollectionCreationState(
                    previousFragmentId = args.previousFragmentId,
                    tabs = tabs,
                    selectedTabs = selectedTabs,
                    saveCollectionStep = args.saveCollectionStep,
                    tabCollections = tabCollections,
                    selectedTabCollection = selectedTabCollection
                )
            )
        }
        collectionCreationInteractor = DefaultCollectionCreationInteractor(
            DefaultCollectionCreationController(
                collectionCreationStore,
                ::dismiss,
                requireComponents.analytics,
                requireComponents.core.tabCollectionStorage,
                requireComponents.useCases.tabsUseCases,
                requireComponents.core.sessionManager,
                viewLifecycleOwner.lifecycleScope
            )
        )
        collectionCreationView = CollectionCreationView(view.createCollectionWrapper, collectionCreationInteractor)

        return view
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        consumeFrom(collectionCreationStore) { newState ->
            collectionCreationView.update(newState)
        }
    }

    override fun onResume() {
        super.onResume()
        collectionCreationView.onResumed()
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val dialog = super.onCreateDialog(savedInstanceState)
        dialog.setOnKeyListener { _, keyCode, event ->
            collectionCreationView.onKey(keyCode, event)
        }
        return dialog
    }
}

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
fun SessionManager.getTabs(tabIds: Array<String>?, publicSuffixList: PublicSuffixList): List<Tab> {
    return tabIds
        ?.mapNotNull { this.findSessionById(it) }
        ?.map { it.toTab(publicSuffixList) }
        ?: emptyList()
}
