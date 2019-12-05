/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.collections

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.feature.tabs.TabsUseCases
import org.mozilla.fenix.R
import org.mozilla.fenix.components.Analytics
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.home.Tab
import org.mozilla.fenix.home.toSessionBundle

interface CollectionCreationController {

    fun saveCollectionName(tabs: List<Tab>, name: String)

    fun renameCollection(collection: TabCollection, name: String)

    /**
     * See [CollectionCreationInteractor.onBackPressed]
     */
    fun backPressed(fromStep: SaveCollectionStep)

    /**
     * See [CollectionCreationInteractor.selectAllTapped]
     */
    fun selectAllTabs()

    /**
     * See [CollectionCreationInteractor.deselectAllTapped]
     */
    fun deselectAllTabs()

    /**
     * See [CollectionCreationInteractor.close]
     */
    fun close()

    fun selectCollection(collection: TabCollection, tabs: List<Tab>)

    /**
     * See [CollectionCreationInteractor.saveTabsToCollection]
     */
    fun saveTabsToCollection(tabs: List<Tab>)

    fun addNewCollection()

    fun addTabToSelection(tab: Tab)

    fun removeTabFromSelection(tab: Tab)
}

class DefaultCollectionCreationController(
    private val store: CollectionCreationStore,
    private val dismiss: () -> Unit,
    private val analytics: Analytics,
    private val tabCollectionStorage: TabCollectionStorage,
    private val tabsUseCases: TabsUseCases,
    private val sessionManager: SessionManager,
    private val lifecycleScope: CoroutineScope
) : CollectionCreationController {
    override fun saveCollectionName(tabs: List<Tab>, name: String) {
        dismiss()

        val sessionBundle = tabs.toList().toSessionBundle(sessionManager)
        lifecycleScope.launch(Dispatchers.IO) {
            tabCollectionStorage.createCollection(name, sessionBundle)
        }

        analytics.metrics.track(
            Event.CollectionSaved(normalSessionSize(sessionManager), sessionBundle.size)
        )

        closeTabsIfNecessary(tabs, sessionManager, tabsUseCases)
    }

    override fun renameCollection(collection: TabCollection, name: String) {
        dismiss()
        lifecycleScope.launch(Dispatchers.IO) {
            tabCollectionStorage.renameCollection(collection, name)
            analytics.metrics.track(Event.CollectionRenamed)
        }
    }

    override fun backPressed(fromStep: SaveCollectionStep) {
        handleBackPress(fromStep)
    }

    override fun selectAllTabs() {
        store.dispatch(CollectionCreationAction.AddAllTabs)
    }

    override fun deselectAllTabs() {
        store.dispatch(CollectionCreationAction.RemoveAllTabs)
    }

    override fun close() {
        dismiss()
    }

    override fun selectCollection(collection: TabCollection, tabs: List<Tab>) {
        dismiss()
        val sessionBundle = tabs.toList().toSessionBundle(sessionManager)
        lifecycleScope.launch(Dispatchers.IO) {
            tabCollectionStorage
                .addTabsToCollection(collection, sessionBundle)
        }

        analytics.metrics.track(
            Event.CollectionTabsAdded(normalSessionSize(sessionManager), sessionBundle.size)
        )

        closeTabsIfNecessary(tabs, sessionManager, tabsUseCases)
    }

    override fun saveTabsToCollection(tabs: List<Tab>) {
        store.dispatch(CollectionCreationAction.StepChanged(
            saveCollectionStep = if (store.state.tabCollections.isEmpty()) {
                SaveCollectionStep.NameCollection
            } else {
                SaveCollectionStep.SelectCollection
            }
        ))
    }

    override fun addNewCollection() {
        store.dispatch(CollectionCreationAction.StepChanged(SaveCollectionStep.NameCollection))
    }

    override fun addTabToSelection(tab: Tab) {
        store.dispatch(CollectionCreationAction.TabAdded(tab))
    }

    override fun removeTabFromSelection(tab: Tab) {
        store.dispatch(CollectionCreationAction.TabRemoved(tab))
    }

    private fun handleBackPress(backFromStep: SaveCollectionStep) {
        val newStep = stepBack(backFromStep)
        if (newStep != null) {
            store.dispatch(CollectionCreationAction.StepChanged(newStep))
        } else {
            dismiss()
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun stepBack(
        backFromStep: SaveCollectionStep
    ): SaveCollectionStep? {
        /*
        Will return the next valid state according to this diagram.

        Name Collection -> Select Collection -> Select Tabs -> (dismiss fragment) <- Rename Collection
         */

        val tabCollectionCount = store.state.tabCollections.size
        val tabCount = store.state.tabs.size

        return when (backFromStep) {
            SaveCollectionStep.NameCollection -> if (tabCollectionCount > 0) {
                SaveCollectionStep.SelectCollection
            } else {
                stepBack(SaveCollectionStep.SelectCollection)
            }
            SaveCollectionStep.SelectCollection -> if (tabCount > 1) {
                SaveCollectionStep.SelectTabs
            } else {
                stepBack(SaveCollectionStep.SelectTabs)
            }
            SaveCollectionStep.SelectTabs, SaveCollectionStep.RenameCollection -> null
        }
    }

    /**
     * @return the number of currently active sessions that are neither custom nor private
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun normalSessionSize(sessionManager: SessionManager): Int {
        return sessionManager.sessions.filter { session ->
            (!session.isCustomTabSession() && !session.private)
        }.size
    }

    private fun closeTabsIfNecessary(tabs: List<Tab>, sessionManager: SessionManager, tabsUseCases: TabsUseCases) {
        // Only close the tabs if the user is not on the BrowserFragment
        if (store.state.previousFragmentId == R.id.browserFragment) { return }
        tabs.asSequence()
            .mapNotNull { tab -> sessionManager.findSessionById(tab.sessionId) }
            .forEach { session -> tabsUseCases.removeTab(session) }
    }
}
