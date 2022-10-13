/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabGroup
import mozilla.components.browser.state.state.TabPartition
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.state.getGroupById
import mozilla.components.browser.state.state.getGroupByName
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test

class TabGroupActionTest {

    @Test
    fun `AddTabGroupAction - Adds provided group and creates partition if needed`() {
        val store = BrowserStore()

        val partition = "testFeaturePartition"
        val testGroup = TabGroup("test", "testGroup")
        store.dispatch(TabGroupAction.AddTabGroupAction(partition = partition, group = testGroup)).joinBlocking()

        val expectedPartition = store.state.tabPartitions[partition]
        assertNotNull(expectedPartition)
        assertSame(testGroup, expectedPartition?.getGroupById(testGroup.id))
        assertSame(testGroup, expectedPartition?.getGroupByName(testGroup.name))
    }

    @Test
    fun `AddTabGroupAction - Adds provided group with tabs`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "tab1", url = "https://firefox.com"),
                    createTab(id = "tab2", url = "https://mozilla.org"),
                ),
            ),
        )

        val partition = "testFeaturePartition"
        val testGroup = TabGroup("test", tabIds = listOf("tab1", "tab2"))
        store.dispatch(TabGroupAction.AddTabGroupAction(partition = partition, group = testGroup)).joinBlocking()

        val expectedPartition = store.state.tabPartitions[partition]
        assertNotNull(expectedPartition)
        assertSame(testGroup, expectedPartition?.getGroupById(testGroup.id))
        assertEquals(listOf("tab1", "tab2"), expectedPartition?.getGroupById(testGroup.id)?.tabIds)
    }

    @Test
    fun `RemoveTabGroupAction - Removes provided group`() {
        val tabGroup1 = TabGroup("test1", tabIds = listOf("tab1", "tab2"))
        val tabGroup2 = TabGroup("test2", tabIds = listOf("tab1", "tab2"))
        val tabPartition = TabPartition("testFeaturePartition", tabGroups = listOf(tabGroup1, tabGroup2))

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "tab1", url = "https://firefox.com"),
                    createTab(id = "tab2", url = "https://mozilla.org"),
                ),
                tabPartitions = mapOf("testFeaturePartition" to tabPartition),
            ),
        )

        assertNotNull(store.state.tabPartitions[tabPartition.id]?.getGroupById(tabGroup1.id))
        assertNotNull(store.state.tabPartitions[tabPartition.id]?.getGroupById(tabGroup2.id))
        store.dispatch(TabGroupAction.RemoveTabGroupAction(tabPartition.id, tabGroup1.id)).joinBlocking()
        assertNull(store.state.tabPartitions[tabPartition.id]?.getGroupById(tabGroup1.id))
        assertNotNull(store.state.tabPartitions[tabPartition.id]?.getGroupById(tabGroup2.id))
    }

    @Test
    fun `RemoveTabGroupAction - Empty partitions are removed`() {
        val tabGroup = TabGroup("test1", tabIds = listOf("tab1", "tab2"))
        val tabPartition = TabPartition("testFeaturePartition", tabGroups = listOf(tabGroup))

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "tab1", url = "https://firefox.com"),
                    createTab(id = "tab2", url = "https://mozilla.org"),
                ),
                tabPartitions = mapOf("testFeaturePartition" to tabPartition),
            ),
        )

        assertNotNull(store.state.tabPartitions[tabPartition.id]?.getGroupById(tabGroup.id))
        store.dispatch(TabGroupAction.RemoveTabGroupAction(tabPartition.id, tabGroup.id)).joinBlocking()
        assertNull(store.state.tabPartitions[tabPartition.id])
    }

    @Test
    fun `AddTabAction - Adds provided tab to groups`() {
        val tabGroup = TabGroup("test1", tabIds = emptyList())
        val tabPartition = TabPartition("testFeaturePartition", tabGroups = listOf(tabGroup))
        val tab = createTab(id = "tab1", url = "https://firefox.com")

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(tab),
                tabPartitions = mapOf("testFeaturePartition" to tabPartition),
            ),
        )

        store.dispatch(TabGroupAction.AddTabAction(tabPartition.id, tabGroup.id, tab.id)).joinBlocking()

        val expectedPartition = store.state.tabPartitions[tabPartition.id]
        assertNotNull(expectedPartition)
        val expectedGroup = expectedPartition!!.getGroupById(tabGroup.id)
        assertNotNull(expectedGroup)
        assertTrue(expectedGroup!!.tabIds.contains(tab.id))
    }

    @Test
    fun `AddTabAction - Creates partition if needed`() {
        val tabGroup = TabGroup("test1", tabIds = emptyList())
        val tabPartition = TabPartition("testFeaturePartition")
        val tab = createTab(id = "tab1", url = "https://firefox.com")

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(tab),
            ),
        )

        store.dispatch(TabGroupAction.AddTabAction(tabPartition.id, tabGroup.id, tab.id)).joinBlocking()

        val expectedPartition = store.state.tabPartitions[tabPartition.id]
        assertNotNull(expectedPartition)
        val expectedGroup = expectedPartition!!.getGroupById(tabGroup.id)
        assertNotNull(expectedGroup)
        assertTrue(expectedGroup!!.tabIds.contains(tab.id))
    }

    @Test
    fun `AddTabAction - Doesn't add tab if already in group`() {
        val tabGroup = TabGroup("test1", tabIds = listOf("tab1"))
        val tabPartition = TabPartition("testFeaturePartition", tabGroups = listOf(tabGroup))
        val tab = createTab(id = "tab1", url = "https://firefox.com")

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(tab),
                tabPartitions = mapOf("testFeaturePartition" to tabPartition),
            ),
        )

        store.dispatch(TabGroupAction.AddTabAction(tabPartition.id, tabGroup.id, tab.id)).joinBlocking()

        val expectedPartition = store.state.tabPartitions[tabPartition.id]
        assertNotNull(expectedPartition)
        val expectedGroup = expectedPartition!!.getGroupById(tabGroup.id)
        assertNotNull(expectedGroup)
        assertTrue(expectedGroup!!.tabIds.contains(tab.id))
        assertEquals(1, expectedGroup.tabIds.size)
    }

    @Test
    fun `AddTabsAction - Adds provided tab to groups`() {
        val tabGroup = TabGroup("test1", tabIds = emptyList())
        val tabPartition = TabPartition("testFeaturePartition", tabGroups = listOf(tabGroup))
        val tab1 = createTab(id = "tab1", url = "https://firefox.com")
        val tab2 = createTab(id = "tab2", url = "https://mozilla.org")

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(tab1, tab2),
                tabPartitions = mapOf("testFeaturePartition" to tabPartition),
            ),
        )

        store.dispatch(
            TabGroupAction.AddTabsAction(tabPartition.id, tabGroup.id, listOf(tab1.id, tab2.id)),
        ).joinBlocking()

        val expectedPartition = store.state.tabPartitions[tabPartition.id]
        assertNotNull(expectedPartition)
        val expectedGroup = expectedPartition!!.getGroupById(tabGroup.id)
        assertNotNull(expectedGroup)
        assertTrue(expectedGroup!!.tabIds.contains(tab1.id))
        assertTrue(expectedGroup.tabIds.contains(tab2.id))
    }

    @Test
    fun `AddTabsAction - Creates partition if needed`() {
        val tabGroup = TabGroup("test1", tabIds = emptyList())
        val tabPartition = TabPartition("testFeaturePartition")
        val tab1 = createTab(id = "tab1", url = "https://firefox.com")
        val tab2 = createTab(id = "tab2", url = "https://mozilla.org")

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(tab1, tab2),
            ),
        )

        store.dispatch(
            TabGroupAction.AddTabsAction(tabPartition.id, tabGroup.id, listOf(tab1.id, tab2.id)),
        ).joinBlocking()

        val expectedPartition = store.state.tabPartitions[tabPartition.id]
        assertNotNull(expectedPartition)
        val expectedGroup = expectedPartition!!.getGroupById(tabGroup.id)
        assertNotNull(expectedGroup)
        assertTrue(expectedGroup!!.tabIds.contains(tab1.id))
        assertTrue(expectedGroup.tabIds.contains(tab2.id))
    }

    @Test
    fun `AddTabsAction - Doesn't add tabs if already in group`() {
        val tabGroup = TabGroup("test1", tabIds = listOf("tab1"))
        val tabPartition = TabPartition("testFeaturePartition", tabGroups = listOf(tabGroup))
        val tab1 = createTab(id = "tab1", url = "https://firefox.com")
        val tab2 = createTab(id = "tab2", url = "https://mozilla.org")

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(tab1, tab2),
                tabPartitions = mapOf("testFeaturePartition" to tabPartition),
            ),
        )

        store.dispatch(
            TabGroupAction.AddTabsAction(tabPartition.id, tabGroup.id, listOf(tab1.id, tab2.id)),
        ).joinBlocking()

        val expectedPartition = store.state.tabPartitions[tabPartition.id]
        assertNotNull(expectedPartition)
        val expectedGroup = expectedPartition!!.getGroupById(tabGroup.id)
        assertNotNull(expectedGroup)
        assertTrue(expectedGroup!!.tabIds.contains(tab1.id))
        assertTrue(expectedGroup.tabIds.contains(tab2.id))
        assertEquals(2, expectedGroup.tabIds.size)
    }

    @Test
    fun `AddTabsAction - Creates partition if needed but only adds distinct tabs`() {
        val tabGroup = TabGroup("test1", tabIds = emptyList())
        val tabPartition = TabPartition("testFeaturePartition")
        val tab1 = createTab(id = "tab1", url = "https://firefox.com")

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(tab1),
            ),
        )

        store.dispatch(
            TabGroupAction.AddTabsAction(tabPartition.id, tabGroup.id, listOf(tab1.id, tab1.id)),
        ).joinBlocking()

        val expectedPartition = store.state.tabPartitions[tabPartition.id]
        assertNotNull(expectedPartition)
        val expectedGroup = expectedPartition!!.getGroupById(tabGroup.id)
        assertNotNull(expectedGroup)
        assertTrue(expectedGroup!!.tabIds.contains(tab1.id))
        assertEquals(1, expectedGroup.tabIds.size)
    }

    @Test
    fun `RemoveTabAction - Removes tab from group`() {
        val tab1 = createTab(id = "tab1", url = "https://firefox.com")
        val tab2 = createTab(id = "tab2", url = "https://mozilla.org")
        val tabGroup = TabGroup("test1", tabIds = listOf(tab1.id, tab2.id))
        val tabPartition = TabPartition("testFeaturePartition", tabGroups = listOf(tabGroup))

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(tab1, tab2),
                tabPartitions = mapOf("testFeaturePartition" to tabPartition),
            ),
        )

        store.dispatch(TabGroupAction.RemoveTabAction(tabPartition.id, tabGroup.id, tab1.id)).joinBlocking()

        val expectedPartition = store.state.tabPartitions[tabPartition.id]
        assertNotNull(expectedPartition)
        val expectedGroup = expectedPartition!!.getGroupById(tabGroup.id)
        assertNotNull(expectedGroup)
        assertFalse(expectedGroup!!.tabIds.contains(tab1.id))
        assertTrue(expectedGroup.tabIds.contains(tab2.id))
    }

    @Test
    fun `RemoveTabsAction - Removes tabs from group`() {
        val tab1 = createTab(id = "tab1", url = "https://firefox.com")
        val tab2 = createTab(id = "tab2", url = "https://mozilla.org")
        val tabGroup = TabGroup("test1", tabIds = listOf(tab1.id, tab2.id))
        val tabPartition = TabPartition("testFeaturePartition", tabGroups = listOf(tabGroup))

        val store = BrowserStore(
            BrowserState(
                tabs = listOf(tab1, tab2),
                tabPartitions = mapOf("testFeaturePartition" to tabPartition),
            ),
        )

        store.dispatch(
            TabGroupAction.RemoveTabsAction(tabPartition.id, tabGroup.id, listOf(tab1.id, tab2.id)),
        ).joinBlocking()

        val expectedPartition = store.state.tabPartitions[tabPartition.id]
        assertNotNull(expectedPartition)
        val expectedGroup = expectedPartition!!.getGroupById(tabGroup.id)
        assertNotNull(expectedGroup)
        assertTrue(expectedGroup!!.tabIds.isEmpty())
    }
}
