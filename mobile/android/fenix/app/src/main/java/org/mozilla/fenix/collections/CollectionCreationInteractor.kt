/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("TooManyFunctions")

package org.mozilla.fenix.collections

import mozilla.components.feature.tab.collections.TabCollection

interface CollectionCreationInteractor {

    fun onNewCollectionNameSaved(tabs: List<Tab>, name: String)

    fun onCollectionRenamed(collection: TabCollection, name: String)

    /**
     * Called when either the physical back button, or the back arrow are clicked.
     *
     * Note that this is not called when the close button on the snackbar is clicked. See [close].
     */
    fun onBackPressed(fromStep: SaveCollectionStep)

    /**
     * Called when a user hits 'Select All' from the 'Select Tabs' step. This affects which tabs
     * have been 'selected' to be saved into a collection.
     */
    fun selectAllTapped()

    /**
     * Called when a user hits 'Deselect All' from the 'Select Tabs' step. This affects which tabs
     * have been 'selected' to be saved into a collection.
     */
    fun deselectAllTapped()

    /**
     * Called when a user hits the close button on the snackbar.
     *
     * Note that this is not called when the back arrow is clicked. See [onBackPressed].
     */
    fun close()

    fun selectCollection(collection: TabCollection, tabs: List<Tab>)

    /**
     * Called when the user decides to save tabs to the currently selected session.
     */
    fun saveTabsToCollection(tabs: List<Tab>)

    fun addNewCollection()

    fun addTabToSelection(tab: Tab)

    fun removeTabFromSelection(tab: Tab)
}

/**
 * Forwards all method calls to their equivalents in [CollectionCreationController].
 */
class DefaultCollectionCreationInteractor(
    private val controller: CollectionCreationController,
) : CollectionCreationInteractor {
    override fun onNewCollectionNameSaved(tabs: List<Tab>, name: String) {
        controller.saveCollectionName(tabs, name)
    }

    override fun onCollectionRenamed(collection: TabCollection, name: String) {
        controller.renameCollection(collection, name)
    }

    override fun onBackPressed(fromStep: SaveCollectionStep) {
        controller.backPressed(fromStep)
    }

    override fun selectAllTapped() {
        controller.selectAllTabs()
    }

    override fun deselectAllTapped() {
        controller.deselectAllTabs()
    }

    override fun close() {
        controller.close()
    }

    override fun selectCollection(collection: TabCollection, tabs: List<Tab>) {
        controller.selectCollection(collection, tabs)
    }

    override fun saveTabsToCollection(tabs: List<Tab>) {
        controller.saveTabsToCollection(tabs)
    }

    override fun addNewCollection() {
        controller.addNewCollection()
    }

    override fun addTabToSelection(tab: Tab) {
        controller.addTabToSelection(tab)
    }

    override fun removeTabFromSelection(tab: Tab) {
        controller.removeTabFromSelection(tab)
    }
}
