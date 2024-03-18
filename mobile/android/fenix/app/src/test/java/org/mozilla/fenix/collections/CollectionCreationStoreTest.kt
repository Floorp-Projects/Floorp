/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import kotlinx.coroutines.CompletableDeferred
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.createTab
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

private const val URL_MOZILLA = "www.mozilla.org"
private const val SESSION_ID_MOZILLA = "0"
private const val URL_BCC = "www.bcc.co.uk"
private const val SESSION_ID_BCC = "1"

private const val SESSION_ID_BAD_1 = "not a real session id"
private const val SESSION_ID_BAD_2 = "definitely not a real session id"

@RunWith(FenixRobolectricTestRunner::class)
class CollectionCreationStoreTest {

    @MockK private lateinit var tabCollectionStorage: TabCollectionStorage

    @MockK(relaxed = true)
    private lateinit var publicSuffixList: PublicSuffixList

    private val sessionMozilla = createTab(URL_MOZILLA, id = SESSION_ID_MOZILLA)
    private val sessionBcc = createTab(URL_BCC, id = SESSION_ID_BCC)
    private val state = BrowserState(
        tabs = listOf(sessionMozilla, sessionBcc),
    )

    @Before
    fun before() {
        MockKAnnotations.init(this)
        every { tabCollectionStorage.cachedTabCollections } returns emptyList()
        every { publicSuffixList.stripPublicSuffix(URL_MOZILLA) } returns CompletableDeferred(URL_MOZILLA)
        every { publicSuffixList.stripPublicSuffix(URL_BCC) } returns CompletableDeferred(URL_BCC)
    }

    @Test
    fun `select and deselect all tabs`() {
        val tabs = listOf<Tab>(mockk(), mockk())
        val store = CollectionCreationStore(
            CollectionCreationState(
                tabs = tabs,
                selectedTabs = emptySet(),
            ),
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
                selectedTabs = setOf(tab2),
            ),
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
                defaultCollectionNumber = 1,
            ),
        )

        store.dispatch(
            CollectionCreationAction.StepChanged(
                saveCollectionStep = SaveCollectionStep.RenameCollection,
                defaultCollectionNumber = 3,
            ),
        ).joinBlocking()
        assertEquals(SaveCollectionStep.RenameCollection, store.state.saveCollectionStep)
        assertEquals(3, store.state.defaultCollectionNumber)
    }

    @Test
    fun `GIVEN no selected tab ids WHEN create initial state THEN only tab will be selected`() {
        val result = createInitialCollectionCreationState(
            browserState = state,
            tabCollectionStorage = tabCollectionStorage,
            publicSuffixList = publicSuffixList,
            saveCollectionStep = SaveCollectionStep.NameCollection,
            tabIds = arrayOf(SESSION_ID_MOZILLA),
            selectedTabIds = null,
            selectedTabCollectionId = 0,
        )

        assertEquals(SaveCollectionStep.NameCollection, result.saveCollectionStep)
        assertEquals(1, result.tabs.size)
        assertEquals(SESSION_ID_MOZILLA, result.tabs[0].sessionId)
        assertEquals(1, result.selectedTabs.size)
        assertEquals(SESSION_ID_MOZILLA, result.selectedTabs.first().sessionId)
    }

    @Test
    fun `GIVEN no selected tab ids WHEN create initial state with many tabs THEN nothing will be selected`() {
        val result = createInitialCollectionCreationState(
            browserState = state,
            tabCollectionStorage = tabCollectionStorage,
            publicSuffixList = publicSuffixList,
            saveCollectionStep = SaveCollectionStep.NameCollection,
            tabIds = arrayOf(SESSION_ID_MOZILLA, SESSION_ID_BCC),
            selectedTabIds = null,
            selectedTabCollectionId = 0,
        )

        assertEquals(SaveCollectionStep.NameCollection, result.saveCollectionStep)
        assertEquals(2, result.tabs.size)
        assertEquals(SESSION_ID_MOZILLA, result.tabs[0].sessionId)
        assertEquals(SESSION_ID_BCC, result.tabs[1].sessionId)
        assertEquals(0, result.selectedTabs.size)
    }

    @Test
    fun `GIVEN selected tab ids WHEN create initial state THEN select tabs`() {
        val result = createInitialCollectionCreationState(
            browserState = state,
            tabCollectionStorage = tabCollectionStorage,
            publicSuffixList = publicSuffixList,
            saveCollectionStep = SaveCollectionStep.RenameCollection,
            tabIds = arrayOf(SESSION_ID_MOZILLA, SESSION_ID_BCC),
            selectedTabIds = arrayOf(SESSION_ID_BCC),
            selectedTabCollectionId = 0,
        )

        assertEquals(SaveCollectionStep.RenameCollection, result.saveCollectionStep)
        assertEquals(2, result.tabs.size)
        assertEquals(SESSION_ID_MOZILLA, result.tabs[0].sessionId)
        assertEquals(SESSION_ID_BCC, result.tabs[1].sessionId)
        assertEquals(1, result.selectedTabs.size)
        assertEquals(SESSION_ID_BCC, result.selectedTabs.first().sessionId)
    }

    @Test
    fun `GIVEN tabs are present in state WHEN getTabs is called THEN tabs will be returned`() {
        val tabs = state.getTabs(arrayOf(SESSION_ID_MOZILLA, SESSION_ID_BCC), publicSuffixList)

        val hosts = tabs.map { it.hostname }

        assertEquals(URL_MOZILLA, hosts[0])
        assertEquals(URL_BCC, hosts[1])
    }

    @Test
    fun `GIVEN some tabs are present in state WHEN getTabs is called THEN only valid tabs will be returned`() {
        val tabs = state.getTabs(arrayOf(SESSION_ID_MOZILLA, SESSION_ID_BAD_1), publicSuffixList)

        val hosts = tabs.map { it.hostname }

        assertEquals(URL_MOZILLA, hosts[0])
        assertEquals(1, hosts.size)
    }

    @Test
    fun `GIVEN tabs are not present in state WHEN getTabs is called THEN an empty list will be returned`() {
        val tabs = state.getTabs(arrayOf(SESSION_ID_BAD_1, SESSION_ID_BAD_2), publicSuffixList)

        assertEquals(emptyList<Tab>(), tabs)
    }

    @Test
    fun `WHEN getTabs is called will null tabIds THEN an empty list will be returned`() {
        val tabs = state.getTabs(null, publicSuffixList)

        assertEquals(emptyList<Tab>(), tabs)
    }

    @Test
    fun `toTab uses active reader URL`() {
        val tabWithoutReaderState = createTab(url = "https://example.com", id = "1")

        val tabWithInactiveReaderState = createTab(
            url = "https://blog.mozilla.org",
            id = "2",
            readerState = ReaderState(active = false, activeUrl = null),
        )

        val tabWithActiveReaderState = createTab(
            url = "moz-extension://123",
            id = "3",
            readerState = ReaderState(active = true, activeUrl = "https://blog.mozilla.org/123"),
        )

        val state = BrowserState(
            tabs = listOf(tabWithoutReaderState, tabWithInactiveReaderState, tabWithActiveReaderState),
        )
        val tabs = state.getTabs(
            arrayOf(tabWithoutReaderState.id, tabWithInactiveReaderState.id, tabWithActiveReaderState.id),
            publicSuffixList,
        )

        assertEquals(tabWithoutReaderState.content.url, tabs[0].url)
        assertEquals(tabWithInactiveReaderState.content.url, tabs[1].url)
        assertEquals("https://blog.mozilla.org/123", tabs[2].url)
    }
}
