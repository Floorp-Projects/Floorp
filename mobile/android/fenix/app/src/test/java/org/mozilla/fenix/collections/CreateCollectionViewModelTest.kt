/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import io.mockk.MockKAnnotations
import io.mockk.mockk
import mozilla.components.feature.tab.collections.TabCollection
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.home.sessioncontrol.Tab

class CreateCollectionViewModelTest {

    private lateinit var viewModel: CreateCollectionViewModel

    @Before
    fun setup() {
        MockKAnnotations.init(this)

        viewModel = CreateCollectionViewModel()
    }

    @Test
    fun `initial state defaults`() {
        assertEquals(
            CollectionCreationState(
                tabs = emptyList(),
                selectedTabs = emptySet(),
                saveCollectionStep = SaveCollectionStep.SelectTabs,
                tabCollections = emptyList(),
                selectedTabCollection = null
            ),
            viewModel.state
        )
        assertNull(viewModel.previousFragmentId)
    }

    @Test
    fun `updateCollection copies tabs to state`() {
        val tabs = listOf<Tab>(mockk(), mockk())
        val tabCollections = listOf<TabCollection>(mockk(), mockk())
        val selectedCollection: TabCollection = mockk()
        viewModel.updateCollection(
            tabs = tabs,
            saveCollectionStep = SaveCollectionStep.SelectCollection,
            selectedTabCollection = selectedCollection,
            cachedTabCollections = tabCollections
        )
        assertEquals(tabs, viewModel.state.tabs)
        assertEquals(SaveCollectionStep.SelectCollection, viewModel.state.saveCollectionStep)
        assertEquals(selectedCollection, viewModel.state.selectedTabCollection)
        assertEquals(tabCollections.reversed(), viewModel.state.tabCollections)
    }

    @Test
    fun `updateCollection selects the only tab`() {
        val tab: Tab = mockk()
        viewModel.updateCollection(
            tabs = listOf(tab),
            saveCollectionStep = mockk(),
            selectedTabCollection = mockk(),
            cachedTabCollections = emptyList()
        )
        assertEquals(setOf(tab), viewModel.state.selectedTabs)

        viewModel.updateCollection(
            tabs = listOf(tab, mockk()),
            saveCollectionStep = mockk(),
            selectedTabCollection = mockk(),
            cachedTabCollections = emptyList()
        )
        assertEquals(emptySet<Tab>(), viewModel.state.selectedTabs)

        viewModel.updateCollection(
            tabs = emptyList(),
            saveCollectionStep = mockk(),
            selectedTabCollection = mockk(),
            cachedTabCollections = emptyList()
        )
        assertEquals(emptySet<Tab>(), viewModel.state.selectedTabs)
    }

    @Test
    fun `saveTabToCollection copies tabs to state`() {
        val tabs = listOf<Tab>(mockk(), mockk())
        val tabCollections = listOf<TabCollection>(mockk(), mockk())
        viewModel.saveTabToCollection(
            tabs = tabs,
            selectedTab = null,
            cachedTabCollections = tabCollections
        )
        assertEquals(tabs, viewModel.state.tabs)
        assertEquals(SaveCollectionStep.SelectTabs, viewModel.state.saveCollectionStep)
        assertNull(viewModel.state.selectedTabCollection)
        assertEquals(tabCollections.reversed(), viewModel.state.tabCollections)
    }

    @Test
    fun `saveTabToCollection selects selectedTab`() {
        val tab: Tab = mockk()
        viewModel.saveTabToCollection(
            tabs = listOf(mockk()),
            selectedTab = tab,
            cachedTabCollections = emptyList()
        )
        assertEquals(setOf(tab), viewModel.state.selectedTabs)

        viewModel.saveTabToCollection(
            tabs = listOf(mockk()),
            selectedTab = null,
            cachedTabCollections = emptyList()
        )
        assertEquals(emptySet<Tab>(), viewModel.state.selectedTabs)
    }

    @Test
    fun `saveTabToCollection sets saveCollectionStep`() {
        viewModel.saveTabToCollection(
            tabs = listOf(mockk(), mockk()),
            selectedTab = null,
            cachedTabCollections = listOf(mockk())
        )
        assertEquals(SaveCollectionStep.SelectTabs, viewModel.state.saveCollectionStep)

        viewModel.saveTabToCollection(
            tabs = listOf(mockk()),
            selectedTab = null,
            cachedTabCollections = listOf(mockk())
        )
        assertEquals(SaveCollectionStep.SelectCollection, viewModel.state.saveCollectionStep)

        viewModel.saveTabToCollection(
            tabs = emptyList(),
            selectedTab = null,
            cachedTabCollections = listOf(mockk())
        )
        assertEquals(SaveCollectionStep.SelectCollection, viewModel.state.saveCollectionStep)

        viewModel.saveTabToCollection(
            tabs = emptyList(),
            selectedTab = null,
            cachedTabCollections = emptyList()
        )
        assertEquals(SaveCollectionStep.NameCollection, viewModel.state.saveCollectionStep)
    }
}
