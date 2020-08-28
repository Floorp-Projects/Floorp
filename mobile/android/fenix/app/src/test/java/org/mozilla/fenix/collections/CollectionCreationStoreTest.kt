/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import io.mockk.mockk
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.Tab

@RunWith(FenixRobolectricTestRunner::class)
class CollectionCreationStoreTest {

    @Test
    fun `select and deselect all tabs`() {
        val tabs = listOf<Tab>(mockk(), mockk())
        val store = CollectionCreationStore(
            CollectionCreationState(
                tabs = tabs,
                selectedTabs = emptySet()
            )
        )

        store.dispatch(CollectionCreationAction.AddAllTabs).joinBlocking()
        assertEquals(tabs.toSet(), store.state.selectedTabs)

        store.dispatch(CollectionCreationAction.RemoveAllTabs).joinBlocking()
        assertEquals(emptySet<Tab>(), store.state.selectedTabs)
    }

    @Test
    fun `select and deselect individual tabs`() {
        val tab1 = mockk<Tab>()
        val tab2 = mockk<Tab>()
        val tab3 = mockk<Tab>()
        val store = CollectionCreationStore(
            CollectionCreationState(
                tabs = listOf(tab1, tab2),
                selectedTabs = setOf(tab2)
            )
        )

        store.dispatch(CollectionCreationAction.TabAdded(tab2)).joinBlocking()
        assertEquals(setOf(tab2), store.state.selectedTabs)

        store.dispatch(CollectionCreationAction.TabAdded(tab1)).joinBlocking()
        assertEquals(setOf(tab1, tab2), store.state.selectedTabs)

        store.dispatch(CollectionCreationAction.TabAdded(tab3)).joinBlocking()
        assertEquals(setOf(tab1, tab2, tab3), store.state.selectedTabs)

        store.dispatch(CollectionCreationAction.TabRemoved(tab2)).joinBlocking()
        assertEquals(setOf(tab1, tab3), store.state.selectedTabs)
    }

    @Test
    fun `change the current step`() {
        val store = CollectionCreationStore(
            CollectionCreationState(
                saveCollectionStep = SaveCollectionStep.SelectTabs,
                defaultCollectionNumber = 1
            )
        )

        store.dispatch(CollectionCreationAction.StepChanged(
            saveCollectionStep = SaveCollectionStep.RenameCollection,
            defaultCollectionNumber = 3
        )).joinBlocking()
        assertEquals(SaveCollectionStep.RenameCollection, store.state.saveCollectionStep)
        assertEquals(3, store.state.defaultCollectionNumber)
    }
}
