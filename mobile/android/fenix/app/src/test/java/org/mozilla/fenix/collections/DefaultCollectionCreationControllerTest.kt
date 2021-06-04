/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import io.mockk.MockKAnnotations
import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.verify
import io.mockk.verifyAll
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineScope
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.tab.collections.TabCollection
import mozilla.components.support.test.ext.joinBlocking
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.home.Tab

@ExperimentalCoroutinesApi
class DefaultCollectionCreationControllerTest {

    private val testCoroutineScope = TestCoroutineScope()
    private lateinit var state: CollectionCreationState
    private lateinit var controller: DefaultCollectionCreationController

    @MockK(relaxed = true) private lateinit var store: CollectionCreationStore
    @MockK(relaxed = true) private lateinit var dismiss: () -> Unit
    @MockK(relaxUnitFun = true) private lateinit var metrics: MetricController
    @MockK(relaxUnitFun = true) private lateinit var tabCollectionStorage: TabCollectionStorage
    private lateinit var browserStore: BrowserStore

    @Before
    fun before() {
        MockKAnnotations.init(this)

        state = CollectionCreationState(
            tabCollections = emptyList(),
            tabs = emptyList()
        )
        every { store.state } answers { state }

        browserStore = BrowserStore()

        controller = DefaultCollectionCreationController(
            store,
            browserStore,
            dismiss,
            metrics,
            tabCollectionStorage,
            testCoroutineScope
        )
    }

    @After
    fun cleanUp() {
        testCoroutineScope.cleanupTestCoroutines()
    }

    @Test
    fun `GIVEN tab list WHEN saveCollectionName is called THEN collection should be created`() {
        val tab1 = createTab("https://www.mozilla.org", id = "session-1")
        val tab2 = createTab("https://www.mozilla.org", id = "session-2")

        browserStore.dispatch(
            TabListAction.AddMultipleTabsAction(listOf(tab1, tab2))
        ).joinBlocking()

        coEvery { tabCollectionStorage.addTabsToCollection(any(), any()) } returns 1L
        coEvery { tabCollectionStorage.createCollection(any(), any()) } returns 1L

        val tabs = listOf(
            Tab("session-1", "", "", ""),
            Tab("null-session", "", "", "")
        )

        controller.saveCollectionName(tabs, "name")

        verify { dismiss() }
        coVerify { tabCollectionStorage.createCollection("name", listOf(tab1)) }
        verify { metrics.track(Event.CollectionSaved(2, 1)) }
    }

    @Test
    fun `GIVEN name collection WHEN backPressed is called THEN next step should be dispatched`() {
        state = state.copy(tabCollections = listOf(mockk()))
        controller.backPressed(SaveCollectionStep.NameCollection)
        verify { store.dispatch(CollectionCreationAction.StepChanged(SaveCollectionStep.SelectCollection)) }

        state = state.copy(tabCollections = emptyList(), tabs = listOf(mockk(), mockk()))
        controller.backPressed(SaveCollectionStep.NameCollection)
        verify { store.dispatch(CollectionCreationAction.StepChanged(SaveCollectionStep.SelectTabs)) }

        state = state.copy(tabCollections = emptyList(), tabs = listOf(mockk()))
        controller.backPressed(SaveCollectionStep.NameCollection)
        verify { dismiss() }
    }

    @Test
    fun `GIVEN select collection WHEN backPressed is called THEN next step should be dispatched`() {
        state = state.copy(tabCollections = emptyList(), tabs = listOf(mockk(), mockk()))
        controller.backPressed(SaveCollectionStep.SelectCollection)
        verify { store.dispatch(CollectionCreationAction.StepChanged(SaveCollectionStep.SelectTabs)) }

        state = state.copy(tabCollections = emptyList(), tabs = listOf(mockk()))
        controller.backPressed(SaveCollectionStep.SelectCollection)
        verify { dismiss() }
    }

    @Test
    fun `GIVEN last step WHEN backPressed is called THEN dismiss should be called`() {
        controller.backPressed(SaveCollectionStep.SelectTabs)
        verify { dismiss() }

        controller.backPressed(SaveCollectionStep.RenameCollection)
        verify { dismiss() }
    }

    @Test
    fun `GIVEN collection WHEN renameCollection is called THEN collection should be renamed`() = testCoroutineScope.runBlockingTest {
        val collection = mockk<TabCollection>()

        controller.renameCollection(collection, "name")
        advanceUntilIdle()

        verifyAll {
            dismiss()
            metrics.track(Event.CollectionRenamed)
        }
        coVerify { tabCollectionStorage.renameCollection(collection, "name") }
    }

    @Test
    fun `WHEN select all is called THEN add all should be dispatched`() {
        controller.selectAllTabs()
        verify { store.dispatch(CollectionCreationAction.AddAllTabs) }

        controller.deselectAllTabs()
        verify { store.dispatch(CollectionCreationAction.RemoveAllTabs) }

        controller.close()
        verify { dismiss() }
    }

    @Test
    fun `WHEN select tab is called THEN add tab should be dispatched`() {
        val tab = mockk<Tab>()

        controller.addTabToSelection(tab)
        verify { store.dispatch(CollectionCreationAction.TabAdded(tab)) }

        controller.removeTabFromSelection(tab)
        verify { store.dispatch(CollectionCreationAction.TabRemoved(tab)) }
    }

    @Test
    fun `WHEN selectCollection is called THEN add tabs should be added to collection`() {
        val tab1 = createTab("https://www.mozilla.org", id = "session-1")
        val tab2 = createTab("https://www.mozilla.org", id = "session-2")
        browserStore.dispatch(
            TabListAction.AddMultipleTabsAction(listOf(tab1, tab2))
        ).joinBlocking()

        val tabs = listOf(
            Tab("session-1", "", "", "")
        )
        val collection = mockk<TabCollection>()
        coEvery { tabCollectionStorage.addTabsToCollection(any(), any()) } returns 1L
        coEvery { tabCollectionStorage.createCollection(any(), any()) } returns 1L

        controller.selectCollection(collection, tabs)

        verify { dismiss() }
        coVerify { tabCollectionStorage.addTabsToCollection(collection, listOf(tab1)) }
        verify { metrics.track(Event.CollectionTabsAdded(2, 1)) }
    }

    @Test
    fun `GIVEN previous step was SelectTabs or RenameCollection WHEN stepBack is called THEN null should be returned`() {
        assertNull(controller.stepBack(SaveCollectionStep.SelectTabs))
        assertNull(controller.stepBack(SaveCollectionStep.RenameCollection))
    }

    @Test
    fun `GIVEN previous step was SelectCollection AND more than one tab is open WHEN stepBack is called THEN SelectTabs should be returned`() {
        state = state.copy(tabs = listOf(mockk(), mockk()))

        assertEquals(SaveCollectionStep.SelectTabs, controller.stepBack(SaveCollectionStep.SelectCollection))
    }

    @Test
    fun `GIVEN previous step was SelectCollection AND one or fewer tabs are open WHEN stepbback is called THEN null should be returned`() {
        state = state.copy(tabs = listOf(mockk()))
        assertNull(controller.stepBack(SaveCollectionStep.SelectCollection))

        state = state.copy(tabs = emptyList())
        assertNull(controller.stepBack(SaveCollectionStep.SelectCollection))
    }

    @Test
    fun `GIVEN previous step was NameCollection AND tabCollections is empty AND more than one tab is open WHEN stepBack is called THEN SelectTabs should be returned`() {
        state = state.copy(tabCollections = emptyList(), tabs = listOf(mockk(), mockk()))

        assertEquals(SaveCollectionStep.SelectTabs, controller.stepBack(SaveCollectionStep.NameCollection))
    }

    @Test
    fun `GIVEN previous step was NameCollection AND tabCollections is empty AND one or fewer tabs are open WHEN stepBack is called THEN null should be returned`() {
        state = state.copy(tabCollections = emptyList(), tabs = listOf(mockk()))
        assertNull(controller.stepBack(SaveCollectionStep.NameCollection))

        state = state.copy(tabCollections = emptyList(), tabs = emptyList())
        assertNull(controller.stepBack(SaveCollectionStep.NameCollection))
    }

    @Test
    fun `GIVEN previous step was NameCollection AND tabCollections is not empty WHEN stepBack is called THEN SelectCollection should be returned`() {
        state = state.copy(tabCollections = listOf(mockk()))
        assertEquals(SaveCollectionStep.SelectCollection, controller.stepBack(SaveCollectionStep.NameCollection))
    }

    @Test
    fun `WHEN adding a new collection THEN dispatch NameCollection step changed`() {
        controller.addNewCollection()

        verify { store.dispatch(CollectionCreationAction.StepChanged(SaveCollectionStep.NameCollection, 1)) }
    }

    @Test
    fun `GIVEN empty list of collections WHEN saving tabs to collection THEN dispatch NameCollection step changed`() {
        controller.saveTabsToCollection(ArrayList())

        verify { store.dispatch(CollectionCreationAction.StepChanged(SaveCollectionStep.NameCollection, 1)) }
    }

    @Test
    fun `GIVEN list of collections WHEN saving tabs to collection THEN dispatch NameCollection step changed`() {
        state = state.copy(tabCollections = listOf(
            mockk {
                every { title } returns "Collection 1"
            },
            mockk {
                every { title } returns "Random Collection"
            }
        ))

        controller.saveTabsToCollection(ArrayList())

        verify { store.dispatch(CollectionCreationAction.StepChanged(SaveCollectionStep.SelectCollection, 2)) }
    }
}
