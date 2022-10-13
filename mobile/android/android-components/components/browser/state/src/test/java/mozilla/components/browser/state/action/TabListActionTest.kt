/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.TabGroup
import mozilla.components.browser.state.state.TabPartition
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.state.getGroupById
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.browser.state.state.recover.TabState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class TabListActionTest {

    @Test
    fun `AddTabAction - Adds provided SessionState`() {
        val store = BrowserStore()

        assertEquals(0, store.state.tabs.size)
        assertNull(store.state.selectedTabId)

        val tab = createTab(url = "https://www.mozilla.org")

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()

        assertEquals(1, store.state.tabs.size)
        assertEquals(tab.id, store.state.selectedTabId)
    }

    @Test
    fun `AddTabAction - Add tab and update selection`() {
        val existingTab = createTab("https://www.mozilla.org")

        val state = BrowserState(
            tabs = listOf(existingTab),
            selectedTabId = existingTab.id,
        )

        val store = BrowserStore(state)

        assertEquals(1, store.state.tabs.size)
        assertEquals(existingTab.id, store.state.selectedTabId)

        val newTab = createTab("https://firefox.com")

        store.dispatch(TabListAction.AddTabAction(newTab, select = true)).joinBlocking()

        assertEquals(2, store.state.tabs.size)
        assertEquals(newTab.id, store.state.selectedTabId)
    }

    @Test
    fun `AddTabAction - Select first tab automatically`() {
        val existingTab = createTab("https://www.mozilla.org")

        val store = BrowserStore()

        assertEquals(0, store.state.tabs.size)
        assertNull(existingTab.id, store.state.selectedTabId)

        val newTab = createTab("https://firefox.com")
        store.dispatch(TabListAction.AddTabAction(newTab, select = false)).joinBlocking()

        assertEquals(1, store.state.tabs.size)
        assertEquals(newTab.id, store.state.selectedTabId)
    }

    @Test
    fun `AddTabAction - Specify parent tab`() {
        val store = BrowserStore()

        val tab1 = createTab("https://www.mozilla.org")
        val tab2 = createTab("https://www.firefox.com")
        val tab3 = createTab("https://wiki.mozilla.org", parent = tab1)
        val tab4 = createTab("https://github.com/mozilla-mobile/android-components", parent = tab2)

        store.dispatch(TabListAction.AddTabAction(tab1)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab2)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab3)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab4)).joinBlocking()

        assertEquals(4, store.state.tabs.size)
        assertNull(store.state.tabs[0].parentId)
        assertNull(store.state.tabs[2].parentId)
        assertEquals(tab1.id, store.state.tabs[1].parentId)
        assertEquals(tab2.id, store.state.tabs[3].parentId)
    }

    @Test
    fun `AddTabAction - Specify source`() {
        val store = BrowserStore()

        val tab1 = createTab("https://www.mozilla.org")
        val tab2 = createTab("https://www.firefox.com", source = SessionState.Source.Internal.Menu)

        store.dispatch(TabListAction.AddTabAction(tab1)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab2)).joinBlocking()

        assertEquals(2, store.state.tabs.size)
        assertEquals(SessionState.Source.Internal.None, store.state.tabs[0].source)
        assertEquals(SessionState.Source.Internal.Menu, store.state.tabs[1].source)
    }

    @Test
    fun `AddTabAction - Tabs with parent are added after (next to) parent`() {
        val store = BrowserStore()

        val parent01 = createTab("https://www.mozilla.org")
        val parent02 = createTab("https://getpocket.com")
        val tab1 = createTab("https://www.firefox.com")
        val tab2 = createTab("https://developer.mozilla.org/en-US/")
        val child001 = createTab("https://www.mozilla.org/en-US/internet-health/", parent = parent01)
        val child002 = createTab("https://www.mozilla.org/en-US/technology/", parent = parent01)
        val child003 = createTab("https://getpocket.com/add/", parent = parent02)

        store.dispatch(TabListAction.AddTabAction(parent01)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab1)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(child001)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab2)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(parent02)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(child002)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(child003)).joinBlocking()

        assertEquals(parent01.id, store.state.tabs[0].id) // ├── parent 1
        assertEquals(child002.id, store.state.tabs[1].id) // │   ├── child 2
        assertEquals(child001.id, store.state.tabs[2].id) // │   └── child 1
        assertEquals(tab1.id, store.state.tabs[3].id) //     ├──tab 1
        assertEquals(tab2.id, store.state.tabs[4].id) //     ├──tab 2
        assertEquals(parent02.id, store.state.tabs[5].id) // └── parent 2
        assertEquals(child003.id, store.state.tabs[6].id) //     └── child 3
    }

    @Test
    fun `SelectTabAction - Selects SessionState by id`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org"),
                createTab(id = "b", url = "https://www.firefox.com"),
            ),
        )
        val store = BrowserStore(state)

        assertNull(store.state.selectedTabId)

        store.dispatch(TabListAction.SelectTabAction("a"))
            .joinBlocking()

        assertEquals("a", store.state.selectedTabId)
    }

    @Test
    fun `RemoveTabAction - Removes SessionState`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org"),
                createTab(id = "b", url = "https://www.firefox.com"),
            ),
        )
        val store = BrowserStore(state)

        store.dispatch(TabListAction.RemoveTabAction("a"))
            .joinBlocking()

        assertEquals(1, store.state.tabs.size)
        assertEquals("https://www.firefox.com", store.state.tabs[0].content.url)
    }

    @Test
    fun `RemoveTabAction - Removes tab from partition`() {
        val tabGroup = TabGroup("test1", tabIds = listOf("a", "b"))
        val tabPartition = TabPartition("testPartition", tabGroups = listOf(tabGroup))

        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org"),
                createTab(id = "b", url = "https://www.firefox.com"),
            ),
            tabPartitions = mapOf(tabPartition.id to tabPartition),
        )
        val store = BrowserStore(state)

        store.dispatch(TabListAction.RemoveTabAction("a")).joinBlocking()
        assertEquals(1, store.state.tabs.size)
        assertEquals("https://www.firefox.com", store.state.tabs[0].content.url)
        assertEquals(listOf("b"), store.state.tabPartitions[tabPartition.id]?.getGroupById(tabGroup.id)?.tabIds)
    }

    @Test
    fun `RemoveTabsAction - Removes SessionState`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org"),
                createTab(id = "b", url = "https://www.firefox.com"),
                createTab(id = "c", url = "https://www.getpocket.com"),
            ),
        )
        val store = BrowserStore(state)

        store.dispatch(TabListAction.RemoveTabsAction(listOf("a", "b")))
            .joinBlocking()

        assertEquals(1, store.state.tabs.size)
        assertEquals("https://www.getpocket.com", store.state.tabs[0].content.url)
    }

    @Test
    fun `RemoveTabsAction - Removes tabs from partition`() {
        val tabGroup = TabGroup("test1", tabIds = listOf("a", "b"))
        val tabPartition = TabPartition("testPartition", tabGroups = listOf(tabGroup))

        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org"),
                createTab(id = "b", url = "https://www.firefox.com"),
            ),
            tabPartitions = mapOf(tabPartition.id to tabPartition),
        )
        val store = BrowserStore(state)

        store.dispatch(TabListAction.RemoveTabsAction(listOf("a", "b"))).joinBlocking()
        assertEquals(0, store.state.tabs.size)
        assertEquals(0, store.state.tabPartitions[tabPartition.id]?.getGroupById(tabGroup.id)?.tabIds?.size)
    }

    @Test
    fun `RemoveTabAction - Noop for unknown id`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org"),
                createTab(id = "b", url = "https://www.firefox.com"),
            ),
        )
        val store = BrowserStore(state)

        store.dispatch(TabListAction.RemoveTabAction("c"))
            .joinBlocking()

        assertEquals(2, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)
        assertEquals("https://www.firefox.com", store.state.tabs[1].content.url)
    }

    @Test
    fun `RemoveTabAction - Selected tab id is set to null if selected and last tab is removed`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org"),
            ),
            selectedTabId = "a",
        )

        val store = BrowserStore(state)

        assertEquals("a", store.state.selectedTabId)

        store.dispatch(TabListAction.RemoveTabAction("a")).joinBlocking()

        assertNull(store.state.selectedTabId)
    }

    @Test
    fun `RemoveTabAction - Does not select custom tab`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org"),
            ),
            customTabs = listOf(
                createCustomTab(id = "b", url = "https://www.firefox.com"),
                createCustomTab(id = "c", url = "https://www.firefox.com/hello", source = SessionState.Source.External.CustomTab(mock())),
            ),
            selectedTabId = "a",
        )

        val store = BrowserStore(state)

        assertEquals("a", store.state.selectedTabId)

        store.dispatch(TabListAction.RemoveTabAction("a")).joinBlocking()

        assertNull(store.state.selectedTabId)
    }

    @Test
    fun `RemoveTabAction - Will select next nearby tab after removing selected tab`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org"),
                createTab(id = "b", url = "https://www.firefox.com"),
                createTab(id = "c", url = "https://www.example.org"),
                createTab(id = "d", url = "https://getpocket.com"),
            ),
            customTabs = listOf(
                createCustomTab(id = "a1", url = "https://www.firefox.com"),
            ),
            selectedTabId = "c",
        )

        val store = BrowserStore(state)

        assertEquals("c", store.state.selectedTabId)

        store.dispatch(TabListAction.RemoveTabAction("c")).joinBlocking()
        assertEquals("d", store.state.selectedTabId)

        store.dispatch(TabListAction.RemoveTabAction("a")).joinBlocking()
        assertEquals("d", store.state.selectedTabId)

        store.dispatch(TabListAction.RemoveTabAction("d")).joinBlocking()
        assertEquals("b", store.state.selectedTabId)

        store.dispatch(TabListAction.RemoveTabAction("b")).joinBlocking()
        assertNull(store.state.selectedTabId)
    }

    @Test
    fun `RemoveTabAction - Selects private tab after private tab was removed`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org", private = true),
                createTab(id = "b", url = "https://www.firefox.com", private = false),
                createTab(id = "c", url = "https://www.example.org", private = false),
                createTab(id = "d", url = "https://getpocket.com", private = true),
                createTab(id = "e", url = "https://developer.mozilla.org/", private = true),
            ),
            customTabs = listOf(
                createCustomTab(id = "a1", url = "https://www.firefox.com"),
                createCustomTab(id = "b1", url = "https://hubs.mozilla.com", source = SessionState.Source.External.CustomTab(mock())),
            ),
            selectedTabId = "d",
        )

        val store = BrowserStore(state)

        // [a*, b, c, (d*), e*] -> [a*, b, c, (e*)]
        store.dispatch(TabListAction.RemoveTabAction("d")).joinBlocking()
        assertEquals("e", store.state.selectedTabId)

        // [a*, b, c, (e*)] -> [(a*), b, c]
        store.dispatch(TabListAction.RemoveTabAction("e")).joinBlocking()
        assertEquals("a", store.state.selectedTabId)

        // After removing the last private tab a normal tab will be selected
        // [(a*), b, c] -> [b, (c)]
        store.dispatch(TabListAction.RemoveTabAction("a")).joinBlocking()
        assertEquals("c", store.state.selectedTabId)
    }

    @Test
    fun `RemoveTabAction - Selects normal tab after normal tab was removed`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org", private = false),
                createTab(id = "b", url = "https://www.firefox.com", private = true),
                createTab(id = "c", url = "https://www.example.org", private = true),
                createTab(id = "d", url = "https://getpocket.com", private = false),
                createTab(id = "e", url = "https://developer.mozilla.org/", private = false),
            ),
            customTabs = listOf(
                createCustomTab(id = "a1", url = "https://www.firefox.com"),
                createCustomTab(id = "b1", url = "https://hubs.mozilla.com", source = SessionState.Source.External.CustomTab(mock())),
            ),
            selectedTabId = "d",
        )

        val store = BrowserStore(state)

        // [a, b*, c*, (d), e] -> [a, b*, c* (e)]
        store.dispatch(TabListAction.RemoveTabAction("d")).joinBlocking()
        assertEquals("e", store.state.selectedTabId)

        // [a, b*, c*, (e)] -> [(a), b*, c*]
        store.dispatch(TabListAction.RemoveTabAction("e")).joinBlocking()
        assertEquals("a", store.state.selectedTabId)

        // After removing the last normal tab NO private tab should get selected
        // [(a), b*, c*] -> [b*, c*]
        store.dispatch(TabListAction.RemoveTabAction("a")).joinBlocking()
        assertNull(store.state.selectedTabId)
    }

    @Test
    fun `RemoveTabAction - Parent will be selected if child is removed and flag is set to true (default)`() {
        val store = BrowserStore()

        val parent = createTab("https://www.mozilla.org")
        val tab1 = createTab("https://www.firefox.com")
        val tab2 = createTab("https://getpocket.com")
        val child = createTab("https://www.mozilla.org/en-US/internet-health/", parent = parent)

        store.dispatch(TabListAction.AddTabAction(parent)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab1)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab2)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(child)).joinBlocking()

        store.dispatch(TabListAction.SelectTabAction(child.id)).joinBlocking()
        store.dispatch(TabListAction.RemoveTabAction(child.id, selectParentIfExists = true)).joinBlocking()

        assertEquals(parent.id, store.state.selectedTabId)
        assertEquals("https://www.mozilla.org", store.state.selectedTab?.content?.url)
    }

    @Test
    fun `RemoveTabAction - Parent will not be selected if child is removed and flag is set to false`() {
        val store = BrowserStore()

        val parent = createTab("https://www.mozilla.org")

        val tab1 = createTab("https://www.firefox.com")
        val tab2 = createTab("https://getpocket.com")
        val child1 = createTab("https://www.mozilla.org/en-US/internet-health/", parent = parent)
        val child2 = createTab("https://www.mozilla.org/en-US/technology/", parent = parent)

        store.dispatch(TabListAction.AddTabAction(parent)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab1)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab2)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(child1)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(child2)).joinBlocking()

        store.dispatch(TabListAction.SelectTabAction(child1.id)).joinBlocking()
        store.dispatch(TabListAction.RemoveTabAction(child1.id, selectParentIfExists = false)).joinBlocking()

        assertEquals(tab1.id, store.state.selectedTabId)
        assertEquals("https://www.firefox.com", store.state.selectedTab?.content?.url)
    }

    @Test
    fun `RemoveTabAction - Providing selectParentIfExists when removing tab without parent has no effect`() {
        val store = BrowserStore()

        val tab1 = createTab("https://www.firefox.com")
        val tab2 = createTab("https://getpocket.com")
        val tab3 = createTab("https://www.mozilla.org/en-US/internet-health/")

        store.dispatch(TabListAction.AddTabAction(tab1)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab2)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab3)).joinBlocking()

        store.dispatch(TabListAction.SelectTabAction(tab3.id)).joinBlocking()
        store.dispatch(TabListAction.RemoveTabAction(tab3.id, selectParentIfExists = true)).joinBlocking()

        assertEquals(tab2.id, store.state.selectedTabId)
        assertEquals("https://getpocket.com", store.state.selectedTab?.content?.url)
    }

    @Test
    fun `RemoveTabAction - Children are updated when parent is removed`() {
        val store = BrowserStore()

        val tab0 = createTab("https://www.firefox.com")
        val tab1 = createTab("https://developer.mozilla.org/en-US/", parent = tab0)
        val tab2 = createTab("https://www.mozilla.org/en-US/internet-health/", parent = tab1)
        val tab3 = createTab("https://www.mozilla.org/en-US/technology/", parent = tab2)

        store.dispatch(TabListAction.AddTabAction(tab0)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab1)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab2)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab3)).joinBlocking()

        // tab0 <- tab1 <- tab2 <- tab3
        assertEquals(tab0.id, store.state.tabs[0].id)
        assertEquals(tab1.id, store.state.tabs[1].id)
        assertEquals(tab2.id, store.state.tabs[2].id)
        assertEquals(tab3.id, store.state.tabs[3].id)

        assertNull(store.state.tabs[0].parentId)
        assertEquals(tab0.id, store.state.tabs[1].parentId)
        assertEquals(tab1.id, store.state.tabs[2].parentId)
        assertEquals(tab2.id, store.state.tabs[3].parentId)

        store.dispatch(TabListAction.RemoveTabAction(tab2.id)).joinBlocking()

        // tab0 <- tab1 <- tab3
        assertEquals(tab0.id, store.state.tabs[0].id)
        assertEquals(tab1.id, store.state.tabs[1].id)
        assertEquals(tab3.id, store.state.tabs[2].id)

        assertNull(store.state.tabs[0].parentId)
        assertEquals(tab0.id, store.state.tabs[1].parentId)
        assertEquals(tab1.id, store.state.tabs[2].parentId)

        store.dispatch(TabListAction.RemoveTabAction(tab0.id)).joinBlocking()

        // tab1 <- tab3
        assertEquals(tab1.id, store.state.tabs[0].id)
        assertEquals(tab3.id, store.state.tabs[1].id)

        assertNull(store.state.tabs[0].parentId)
        assertEquals(tab1.id, store.state.tabs[1].parentId)
    }

    @Test
    fun `RestoreAction - Adds restored tabs and updates selected tab`() {
        val store = BrowserStore()

        assertEquals(0, store.state.tabs.size)

        store.dispatch(
            TabListAction.RestoreAction(
                tabs = listOf(
                    RecoverableTab(
                        engineSessionState = null,
                        state = TabState(id = "a", url = "https://www.mozilla.org", private = false),
                    ),
                    RecoverableTab(
                        engineSessionState = null,
                        state = TabState(id = "b", url = "https://www.firefox.com", private = true),
                    ),
                    RecoverableTab(
                        engineSessionState = null,
                        state = TabState(id = "c", url = "https://www.example.org", private = true),
                    ),
                    RecoverableTab(
                        engineSessionState = null,
                        state = TabState(id = "d", url = "https://getpocket.com", private = false),
                    ),
                ),
                selectedTabId = "d",
                restoreLocation = TabListAction.RestoreAction.RestoreLocation.BEGINNING,
            ),
        ).joinBlocking()

        assertEquals(4, store.state.tabs.size)
        assertEquals("a", store.state.tabs[0].id)
        assertEquals("b", store.state.tabs[1].id)
        assertEquals("c", store.state.tabs[2].id)
        assertEquals("d", store.state.tabs[3].id)
        assertEquals("d", store.state.selectedTabId)
    }

    @Test
    fun `RestoreAction - Adds restored tabs to the beginning of existing tabs without updating selection`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org", private = false),
                    createTab(id = "b", url = "https://www.firefox.com", private = true),
                ),
                selectedTabId = "a",
            ),
        )

        assertEquals(2, store.state.tabs.size)

        store.dispatch(
            TabListAction.RestoreAction(
                tabs = listOf(
                    RecoverableTab(
                        engineSessionState = null,
                        state = TabState(id = "c", url = "https://www.example.org", private = true),
                    ),
                    RecoverableTab(
                        engineSessionState = null,
                        state = TabState(id = "d", url = "https://getpocket.com", private = false),
                    ),
                ),
                selectedTabId = "d",
                restoreLocation = TabListAction.RestoreAction.RestoreLocation.BEGINNING,
            ),
        ).joinBlocking()

        assertEquals(4, store.state.tabs.size)
        assertEquals("c", store.state.tabs[0].id)
        assertEquals("d", store.state.tabs[1].id)
        assertEquals("a", store.state.tabs[2].id)
        assertEquals("b", store.state.tabs[3].id)
        assertEquals("a", store.state.selectedTabId)
    }

    @Test
    fun `RestoreAction - Adds restored tabs to the end of existing tabs without updating selection`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org", private = false),
                    createTab(id = "b", url = "https://www.firefox.com", private = true),
                ),
                selectedTabId = "a",
            ),
        )

        assertEquals(2, store.state.tabs.size)

        store.dispatch(
            TabListAction.RestoreAction(
                tabs = listOf(
                    RecoverableTab(
                        engineSessionState = null,
                        state = TabState(id = "c", url = "https://www.example.org", private = true),
                    ),
                    RecoverableTab(
                        engineSessionState = null,
                        state = TabState(id = "d", url = "https://getpocket.com", private = false),
                    ),
                ),
                selectedTabId = "d",
                restoreLocation = TabListAction.RestoreAction.RestoreLocation.END,
            ),
        ).joinBlocking()

        assertEquals(4, store.state.tabs.size)
        assertEquals("a", store.state.tabs[0].id)
        assertEquals("b", store.state.tabs[1].id)
        assertEquals("c", store.state.tabs[2].id)
        assertEquals("d", store.state.tabs[3].id)
        assertEquals("a", store.state.selectedTabId)
    }

    @Test
    fun `RestoreAction - Adds restored tabs to beginning of existing tabs with updating selection`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org", private = false),
                    createTab(id = "b", url = "https://www.firefox.com", private = true),
                ),
            ),
        )

        assertEquals(2, store.state.tabs.size)

        store.dispatch(
            TabListAction.RestoreAction(
                tabs = listOf(
                    RecoverableTab(
                        engineSessionState = null,
                        state = TabState(id = "c", url = "https://www.example.org", private = true),
                    ),
                    RecoverableTab(
                        engineSessionState = null,
                        state = TabState(id = "d", url = "https://getpocket.com", private = false),
                    ),
                ),
                selectedTabId = "d",
                restoreLocation = TabListAction.RestoreAction.RestoreLocation.BEGINNING,
            ),
        ).joinBlocking()

        assertEquals(4, store.state.tabs.size)
        assertEquals("c", store.state.tabs[0].id)
        assertEquals("d", store.state.tabs[1].id)
        assertEquals("a", store.state.tabs[2].id)
        assertEquals("b", store.state.tabs[3].id)
        assertEquals("d", store.state.selectedTabId)
    }

    @Test
    fun `RestoreAction - Adds restored tabs to end of existing tabs with updating selection`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org", private = false),
                    createTab(id = "b", url = "https://www.firefox.com", private = true),
                ),
            ),
        )

        assertEquals(2, store.state.tabs.size)

        store.dispatch(
            TabListAction.RestoreAction(
                tabs = listOf(
                    RecoverableTab(
                        engineSessionState = null,
                        state = TabState(id = "c", url = "https://www.example.org", private = true),
                    ),
                    RecoverableTab(
                        engineSessionState = null,
                        state = TabState(id = "d", url = "https://getpocket.com", private = false),
                    ),
                ),
                selectedTabId = "d",
                restoreLocation = TabListAction.RestoreAction.RestoreLocation.END,
            ),
        ).joinBlocking()

        assertEquals(4, store.state.tabs.size)
        assertEquals("a", store.state.tabs[0].id)
        assertEquals("b", store.state.tabs[1].id)
        assertEquals("c", store.state.tabs[2].id)
        assertEquals("d", store.state.tabs[3].id)
        assertEquals("d", store.state.selectedTabId)
    }

    @Test
    fun `RestoreAction - Does not update selection if none was provided`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org", private = false),
                    createTab(id = "b", url = "https://www.firefox.com", private = true),
                ),
            ),
        )

        assertEquals(2, store.state.tabs.size)

        store.dispatch(
            TabListAction.RestoreAction(
                tabs = listOf(
                    RecoverableTab(engineSessionState = null, state = TabState(id = "c", url = "https://www.example.org", private = true)),
                    RecoverableTab(engineSessionState = null, state = TabState(id = "d", url = "https://getpocket.com", private = false)),
                ),
                selectedTabId = null,
                restoreLocation = TabListAction.RestoreAction.RestoreLocation.BEGINNING,
            ),
        ).joinBlocking()

        assertEquals(4, store.state.tabs.size)
        assertEquals("c", store.state.tabs[0].id)
        assertEquals("d", store.state.tabs[1].id)
        assertEquals("a", store.state.tabs[2].id)
        assertEquals("b", store.state.tabs[3].id)
        assertNull(store.state.selectedTabId)
    }

    @Test
    fun `RestoreAction - Add tab back to correct location (beginning)`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org"),
                    createTab(id = "b", url = "https://www.firefox.com"),
                ),
            ),
        )

        assertEquals(2, store.state.tabs.size)

        store.dispatch(
            TabListAction.RestoreAction(
                tabs = listOf(
                    RecoverableTab(engineSessionState = null, state = TabState(id = "c", url = "https://www.example.org", index = 0)),
                ),
                selectedTabId = null,
                restoreLocation = TabListAction.RestoreAction.RestoreLocation.AT_INDEX,
            ),
        ).joinBlocking()

        assertEquals(3, store.state.tabs.size)
        assertEquals("c", store.state.tabs[0].id)
        assertEquals("a", store.state.tabs[1].id)
        assertEquals("b", store.state.tabs[2].id)
    }

    @Test
    fun `RestoreAction - Add tab back to correct location (middle)`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org"),
                    createTab(id = "b", url = "https://www.firefox.com"),
                ),
            ),
        )

        assertEquals(2, store.state.tabs.size)

        store.dispatch(
            TabListAction.RestoreAction(
                tabs = listOf(
                    RecoverableTab(engineSessionState = null, state = TabState(id = "c", url = "https://www.example.org", index = 1)),
                ),
                selectedTabId = null,
                restoreLocation = TabListAction.RestoreAction.RestoreLocation.AT_INDEX,
            ),
        ).joinBlocking()

        assertEquals(3, store.state.tabs.size)
        assertEquals("a", store.state.tabs[0].id)
        assertEquals("c", store.state.tabs[1].id)
        assertEquals("b", store.state.tabs[2].id)
    }

    @Test
    fun `RestoreAction - Add tab back to correct location (end)`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org"),
                    createTab(id = "b", url = "https://www.firefox.com"),
                ),
            ),
        )

        assertEquals(2, store.state.tabs.size)

        store.dispatch(
            TabListAction.RestoreAction(
                tabs = listOf(
                    RecoverableTab(engineSessionState = null, state = TabState(id = "c", url = "https://www.example.org", index = 2)),
                ),
                selectedTabId = null,
                restoreLocation = TabListAction.RestoreAction.RestoreLocation.AT_INDEX,
            ),
        ).joinBlocking()

        assertEquals(3, store.state.tabs.size)
        assertEquals("a", store.state.tabs[0].id)
        assertEquals("b", store.state.tabs[1].id)
        assertEquals("c", store.state.tabs[2].id)
    }

    @Test
    fun `RestoreAction - Add tab back to correct location with index beyond size of total tabs`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org"),
                    createTab(id = "b", url = "https://www.firefox.com"),
                ),
            ),
        )

        assertEquals(2, store.state.tabs.size)

        store.dispatch(
            TabListAction.RestoreAction(
                tabs = listOf(
                    RecoverableTab(engineSessionState = null, state = TabState(id = "c", url = "https://www.example.org", index = 4)),
                ),
                selectedTabId = null,
                restoreLocation = TabListAction.RestoreAction.RestoreLocation.AT_INDEX,
            ),
        ).joinBlocking()

        assertEquals(3, store.state.tabs.size)
        assertEquals("a", store.state.tabs[0].id)
        assertEquals("b", store.state.tabs[1].id)
        assertEquals("c", store.state.tabs[2].id)
    }

    @Test
    fun `RestoreAction - Add tabs back to correct locations`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org"),
                    createTab(id = "b", url = "https://www.firefox.com"),
                ),
            ),
        )

        assertEquals(2, store.state.tabs.size)

        store.dispatch(
            TabListAction.RestoreAction(
                tabs = listOf(
                    RecoverableTab(engineSessionState = null, state = TabState(id = "c", url = "https://www.example.org", index = 3)),
                    RecoverableTab(engineSessionState = null, state = TabState(id = "d", url = "https://www.example.org", index = 0)),
                ),
                selectedTabId = null,
                restoreLocation = TabListAction.RestoreAction.RestoreLocation.AT_INDEX,
            ),
        ).joinBlocking()

        assertEquals(4, store.state.tabs.size)
        assertEquals("d", store.state.tabs[0].id)
        assertEquals("a", store.state.tabs[1].id)
        assertEquals("b", store.state.tabs[2].id)
        assertEquals("c", store.state.tabs[3].id)
    }

    @Test
    fun `RestoreAction - Add tabs with matching indices back to correct locations`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org"),
                    createTab(id = "b", url = "https://www.firefox.com"),
                ),
            ),
        )

        assertEquals(2, store.state.tabs.size)

        store.dispatch(
            TabListAction.RestoreAction(
                tabs = listOf(
                    RecoverableTab(engineSessionState = null, state = TabState(id = "c", url = "https://www.example.org", index = 0)),
                    RecoverableTab(engineSessionState = null, state = TabState(id = "d", url = "https://www.example.org", index = 0)),
                ),
                selectedTabId = null,
                restoreLocation = TabListAction.RestoreAction.RestoreLocation.AT_INDEX,
            ),
        ).joinBlocking()

        assertEquals(4, store.state.tabs.size)
        assertEquals("d", store.state.tabs[0].id)
        assertEquals("c", store.state.tabs[1].id)
        assertEquals("a", store.state.tabs[2].id)
        assertEquals("b", store.state.tabs[3].id)
    }

    @Test
    fun `RestoreAction - Add tabs with a -1 removal index`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org"),
                    createTab(id = "b", url = "https://www.firefox.com"),
                ),
            ),
        )

        assertEquals(2, store.state.tabs.size)

        store.dispatch(
            TabListAction.RestoreAction(
                tabs = listOf(
                    RecoverableTab(engineSessionState = null, state = TabState(id = "c", url = "https://www.example.org", index = -1)),
                    RecoverableTab(engineSessionState = null, state = TabState(id = "d", url = "https://www.example.org")),
                ),
                selectedTabId = null,
                restoreLocation = TabListAction.RestoreAction.RestoreLocation.AT_INDEX,
            ),
        ).joinBlocking()

        assertEquals(4, store.state.tabs.size)
        assertEquals("a", store.state.tabs[0].id)
        assertEquals("b", store.state.tabs[1].id)
        assertEquals("c", store.state.tabs[2].id)
        assertEquals("d", store.state.tabs[3].id)
    }

    @Test
    fun `RemoveAllTabsAction - Removes both private and non-private tabs (but not custom tabs)`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org", private = false),
                createTab(id = "b", url = "https://www.firefox.com", private = true),
            ),
            customTabs = listOf(
                createCustomTab(id = "a1", url = "https://www.firefox.com"),
                createCustomTab(id = "a2", url = "https://www.firefox.com/hello", source = SessionState.Source.External.CustomTab(mock())),
            ),
            selectedTabId = "a",
        )

        val store = BrowserStore(state)
        store.dispatch(TabListAction.RemoveAllTabsAction()).joinBlocking()

        assertTrue(store.state.tabs.isEmpty())
        assertNull(store.state.selectedTabId)
        assertEquals(2, store.state.customTabs.size)
        assertEquals("a2", store.state.customTabs.last().id)
    }

    @Test
    fun `RemoveAllTabsAction - Removes tabs from partition`() {
        val tabGroup = TabGroup("test1", tabIds = listOf("a", "b"))
        val tabPartition = TabPartition("testPartition", tabGroups = listOf(tabGroup))

        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org"),
                createTab(id = "b", url = "https://www.firefox.com", private = true),
            ),
            tabPartitions = mapOf(tabPartition.id to tabPartition),
        )
        val store = BrowserStore(state)

        store.dispatch(TabListAction.RemoveAllTabsAction()).joinBlocking()
        assertEquals(0, store.state.tabs.size)
        assertEquals(0, store.state.tabPartitions[tabPartition.id]?.getGroupById(tabGroup.id)?.tabIds?.size)
    }

    @Test
    fun `RemoveAllPrivateTabsAction - Removes only private tabs`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org", private = false),
                createTab(id = "b", url = "https://www.firefox.com", private = true),
            ),
            customTabs = listOf(
                createCustomTab(id = "a1", url = "https://www.firefox.com"),
            ),
            selectedTabId = "a",
        )

        val store = BrowserStore(state)
        store.dispatch(TabListAction.RemoveAllPrivateTabsAction).joinBlocking()

        assertEquals(1, store.state.tabs.size)
        assertEquals("a", store.state.tabs[0].id)
        assertEquals("a", store.state.selectedTabId)

        assertEquals(1, store.state.customTabs.size)
        assertEquals("a1", store.state.customTabs.last().id)
    }

    @Test
    fun `RemoveAllPrivateTabsAction - Updates selection if affected`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org", private = false),
                createTab(id = "b", url = "https://www.firefox.com", private = true),
            ),
            customTabs = listOf(
                createCustomTab(id = "a1", url = "https://www.firefox.com"),
            ),
            selectedTabId = "b",
        )

        val store = BrowserStore(state)
        store.dispatch(TabListAction.RemoveAllPrivateTabsAction).joinBlocking()

        assertEquals(1, store.state.tabs.size)
        assertEquals("a", store.state.tabs[0].id)
        assertEquals("a", store.state.selectedTabId)

        assertEquals(1, store.state.customTabs.size)
        assertEquals("a1", store.state.customTabs.last().id)
    }

    @Test
    fun `RemoveAllPrivateTabsAction - Removes tabs from partition`() {
        val normalTabGroup = TabGroup("test1", tabIds = listOf("a"))
        val privateTabGroup = TabGroup("test2", tabIds = listOf("b"))
        val tabPartition = TabPartition("testPartition", tabGroups = listOf(normalTabGroup, privateTabGroup))

        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org"),
                createTab(id = "b", url = "https://www.firefox.com", private = true),
            ),
            tabPartitions = mapOf(tabPartition.id to tabPartition),
        )
        val store = BrowserStore(state)

        store.dispatch(TabListAction.RemoveAllPrivateTabsAction).joinBlocking()
        assertEquals(1, store.state.tabs.size)
        assertEquals(1, store.state.tabPartitions[tabPartition.id]?.getGroupById(normalTabGroup.id)?.tabIds?.size)
        assertEquals(0, store.state.tabPartitions[tabPartition.id]?.getGroupById(privateTabGroup.id)?.tabIds?.size)
    }

    @Test
    fun `RemoveAllNormalTabsAction - Removes only normal (non-private) tabs`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org", private = false),
                createTab(id = "b", url = "https://www.firefox.com", private = true),
            ),
            customTabs = listOf(
                createCustomTab(id = "a1", url = "https://www.firefox.com"),
            ),
            selectedTabId = "b",
        )

        val store = BrowserStore(state)
        store.dispatch(TabListAction.RemoveAllNormalTabsAction).joinBlocking()

        assertEquals(1, store.state.tabs.size)
        assertEquals("b", store.state.tabs[0].id)
        assertEquals("b", store.state.selectedTabId)

        assertEquals(1, store.state.customTabs.size)
        assertEquals("a1", store.state.customTabs.last().id)
    }

    @Test
    fun `RemoveAllNormalTabsAction - Updates selection if affected`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org", private = false),
                createTab(id = "b", url = "https://www.firefox.com", private = true),
            ),
            customTabs = listOf(
                createCustomTab(id = "a1", url = "https://www.firefox.com"),
            ),
            selectedTabId = "a",
        )

        val store = BrowserStore(state)
        store.dispatch(TabListAction.RemoveAllNormalTabsAction).joinBlocking()

        assertEquals(1, store.state.tabs.size)
        assertEquals("b", store.state.tabs[0].id)
        // After removing the last normal tab NO private tab should get selected
        assertNull(store.state.selectedTabId)

        assertEquals(1, store.state.customTabs.size)
        assertEquals("a1", store.state.customTabs.last().id)
    }

    @Test
    fun `RemoveAllNormalTabsAction - Removes tabs from partition`() {
        val normalTabGroup = TabGroup("test1", tabIds = listOf("a"))
        val privateTabGroup = TabGroup("test2", tabIds = listOf("b"))
        val tabPartition = TabPartition("testPartition", tabGroups = listOf(normalTabGroup, privateTabGroup))

        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org"),
                createTab(id = "b", url = "https://www.firefox.com", private = true),
            ),
            tabPartitions = mapOf(tabPartition.id to tabPartition),
        )
        val store = BrowserStore(state)

        store.dispatch(TabListAction.RemoveAllNormalTabsAction).joinBlocking()
        assertEquals(1, store.state.tabs.size)
        assertEquals(0, store.state.tabPartitions[tabPartition.id]?.getGroupById(normalTabGroup.id)?.tabIds?.size)
        assertEquals(1, store.state.tabPartitions[tabPartition.id]?.getGroupById(privateTabGroup.id)?.tabIds?.size)
    }

    @Test
    fun `AddMultipleTabsAction - Adds multiple tabs and updates selection`() {
        val store = BrowserStore()

        assertEquals(0, store.state.tabs.size)
        assertNull(store.state.selectedTabId)

        store.dispatch(
            TabListAction.AddMultipleTabsAction(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org", private = false),
                    createTab(id = "b", url = "https://www.firefox.com", private = true),
                ),
            ),
        ).joinBlocking()

        assertEquals(2, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)
        assertEquals("https://www.firefox.com", store.state.tabs[1].content.url)
        assertNotNull(store.state.selectedTabId)
        assertEquals("a", store.state.selectedTabId)
    }

    @Test
    fun `AddMultipleTabsAction - Adds multiple tabs and does not update selection if one exists already`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(createTab(id = "z", url = "https://getpocket.com")),
                selectedTabId = "z",
            ),
        )

        assertEquals(1, store.state.tabs.size)
        assertEquals("z", store.state.selectedTabId)

        store.dispatch(
            TabListAction.AddMultipleTabsAction(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org", private = false),
                    createTab(id = "b", url = "https://www.firefox.com", private = true),
                ),
            ),
        ).joinBlocking()

        assertEquals(3, store.state.tabs.size)
        assertEquals("https://getpocket.com", store.state.tabs[0].content.url)
        assertEquals("https://www.mozilla.org", store.state.tabs[1].content.url)
        assertEquals("https://www.firefox.com", store.state.tabs[2].content.url)
        assertEquals("z", store.state.selectedTabId)
    }

    @Test
    fun `AddMultipleTabsAction - Non private tab will be selected`() {
        val store = BrowserStore()

        assertEquals(0, store.state.tabs.size)
        assertNull(store.state.selectedTabId)

        store.dispatch(
            TabListAction.AddMultipleTabsAction(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org", private = true),
                    createTab(id = "b", url = "https://www.example.org", private = true),
                    createTab(id = "c", url = "https://www.firefox.com", private = false),
                    createTab(id = "d", url = "https://getpocket.com", private = true),
                ),
            ),
        ).joinBlocking()

        assertEquals(4, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)
        assertEquals("https://www.example.org", store.state.tabs[1].content.url)
        assertEquals("https://www.firefox.com", store.state.tabs[2].content.url)
        assertEquals("https://getpocket.com", store.state.tabs[3].content.url)
        assertNotNull(store.state.selectedTabId)
        assertEquals("c", store.state.selectedTabId)
    }

    @Test
    fun `AddMultipleTabsAction - No tab will be selected if only private tabs are added`() {
        val store = BrowserStore()

        assertEquals(0, store.state.tabs.size)
        assertNull(store.state.selectedTabId)

        store.dispatch(
            TabListAction.AddMultipleTabsAction(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org", private = true),
                    createTab(id = "b", url = "https://www.example.org", private = true),
                    createTab(id = "c", url = "https://getpocket.com", private = true),
                ),
            ),
        ).joinBlocking()

        assertEquals(3, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)
        assertEquals("https://www.example.org", store.state.tabs[1].content.url)
        assertEquals("https://getpocket.com", store.state.tabs[2].content.url)
        assertNull(store.state.selectedTabId)
    }

    @Test
    fun `RemoveAllNormalTabsAction with private tab selected`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org", private = true),
                    createTab(id = "b", url = "https://www.example.org", private = false),
                    createTab(id = "c", url = "https://www.firefox.com", private = false),
                    createTab(id = "d", url = "https://getpocket.com", private = true),
                ),
                selectedTabId = "d",
            ),
        )

        store.dispatch(TabListAction.RemoveAllNormalTabsAction).joinBlocking()

        assertEquals(0, store.state.normalTabs.size)
        assertEquals(2, store.state.privateTabs.size)
        assertEquals("d", store.state.selectedTabId)
    }

    @Test
    fun `RemoveAllNormalTabsAction with normal tab selected`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org", private = true),
                    createTab(id = "b", url = "https://www.example.org", private = false),
                    createTab(id = "c", url = "https://www.firefox.com", private = false),
                    createTab(id = "d", url = "https://getpocket.com", private = true),
                ),
                selectedTabId = "b",
            ),
        )

        store.dispatch(TabListAction.RemoveAllNormalTabsAction).joinBlocking()

        assertEquals(0, store.state.normalTabs.size)
        assertEquals(2, store.state.privateTabs.size)
        assertNull(store.state.selectedTabId)
    }

    @Test
    fun `RemoveAllPrivateTabsAction with private tab selected`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org", private = true),
                    createTab(id = "b", url = "https://www.example.org", private = false),
                    createTab(id = "c", url = "https://www.firefox.com", private = false),
                    createTab(id = "d", url = "https://getpocket.com", private = true),
                ),
                selectedTabId = "d",
            ),
        )

        store.dispatch(TabListAction.RemoveAllPrivateTabsAction).joinBlocking()

        assertEquals(2, store.state.normalTabs.size)
        assertEquals(0, store.state.privateTabs.size)
        assertEquals("c", store.state.selectedTabId)
    }

    @Test
    fun `RemoveAllPrivateTabsAction with private tab selected and no normal tabs`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org", private = true),
                    createTab(id = "b", url = "https://getpocket.com", private = true),
                ),
                selectedTabId = "b",
            ),
        )

        store.dispatch(TabListAction.RemoveAllPrivateTabsAction).joinBlocking()

        assertEquals(0, store.state.normalTabs.size)
        assertEquals(0, store.state.privateTabs.size)
        assertNull(store.state.selectedTabId)
    }

    @Test
    fun `RemoveAllPrivateTabsAction with normal tab selected`() {
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(id = "a", url = "https://www.mozilla.org", private = true),
                    createTab(id = "b", url = "https://www.example.org", private = false),
                    createTab(id = "c", url = "https://www.firefox.com", private = false),
                    createTab(id = "d", url = "https://getpocket.com", private = true),
                ),
                selectedTabId = "b",
            ),
        )

        store.dispatch(TabListAction.RemoveAllPrivateTabsAction).joinBlocking()

        assertEquals(2, store.state.normalTabs.size)
        assertEquals(0, store.state.privateTabs.size)
        assertEquals("b", store.state.selectedTabId)
    }

    private fun assertSameTabs(a: BrowserStore, b: List<TabSessionState>, str: String? = null) {
        val aMap = a.state.tabs.map { "<" + it.id + "," + it.content.url + ">\n" }
        val bMap = b.map { "<" + it.id + "," + it.content.url + ">\n" }
        assertEquals(str, aMap.toString(), bMap.toString())
    }
    private fun dispatchJoinMoveAction(store: BrowserStore, tabIds: List<String>, targetTabId: String, placeAfter: Boolean) {
        store.dispatch(
            TabListAction.MoveTabsAction(
                tabIds,
                targetTabId,
                placeAfter,
            ),
        ).joinBlocking()
    }

    @Test
    fun `MoveTabsAction - Tabs move as expected`() {
        val tabList = listOf(
            createTab(id = "a", url = "https://www.mozilla.org"),
            createTab(id = "b", url = "https://www.firefox.com"),
            createTab(id = "c", url = "https://getpocket.com"),
            createTab(id = "d", url = "https://www.example.org"),
        )
        val store = BrowserStore(
            BrowserState(
                tabs = tabList,
                selectedTabId = "a",
            ),
        )

        dispatchJoinMoveAction(store, listOf("a"), "a", false)
        assertSameTabs(store, tabList, "a to a-")
        dispatchJoinMoveAction(store, listOf("a"), "a", true)
        assertSameTabs(store, tabList, "a to a+")
        dispatchJoinMoveAction(store, listOf("a"), "b", false)
        assertSameTabs(store, tabList, "a to b-")

        dispatchJoinMoveAction(store, listOf("a", "b"), "a", false)
        assertSameTabs(store, tabList, "a,b to a-")
        dispatchJoinMoveAction(store, listOf("a", "b"), "a", true)
        assertSameTabs(store, tabList, "a,b to a+")
        dispatchJoinMoveAction(store, listOf("a", "b"), "b", false)
        assertSameTabs(store, tabList, "a,b to b-")
        dispatchJoinMoveAction(store, listOf("a", "b"), "b", true)
        assertSameTabs(store, tabList, "a,b to b+")
        dispatchJoinMoveAction(store, listOf("a", "b"), "c", false)
        assertSameTabs(store, tabList, "a,b to c-")

        dispatchJoinMoveAction(store, listOf("c", "d"), "c", false)
        assertSameTabs(store, tabList, "c,d to c-")
        dispatchJoinMoveAction(store, listOf("c", "d"), "d", true)
        assertSameTabs(store, tabList, "c,d to d+")

        val movedTabList = listOf(
            createTab(id = "b", url = "https://www.firefox.com"),
            createTab(id = "c", url = "https://getpocket.com"),
            createTab(id = "a", url = "https://www.mozilla.org"),
            createTab(id = "d", url = "https://www.example.org"),
        )
        dispatchJoinMoveAction(store, listOf("a"), "d", false)
        assertSameTabs(store, movedTabList, "a to d-")
        dispatchJoinMoveAction(store, listOf("b", "c"), "a", true)
        assertSameTabs(store, tabList, "b,c to a+")

        dispatchJoinMoveAction(store, listOf("a", "d"), "c", true)
        assertSameTabs(store, movedTabList, "a,d to c+")

        dispatchJoinMoveAction(store, listOf("b", "c"), "d", false)
        assertSameTabs(store, tabList, "b,c to d-")
        assertEquals("a", store.state.selectedTabId)
    }

    @Test
    fun `MoveTabsAction - Complex moves work`() {
        val tabList = listOf(
            createTab(id = "a", url = "https://www.mozilla.org"),
            createTab(id = "b", url = "https://www.firefox.com"),
            createTab(id = "c", url = "https://getpocket.com"),
            createTab(id = "d", url = "https://www.example.org"),
            createTab(id = "e", url = "https://www.mozilla.org/en-US/firefox/features/"),
            createTab(id = "f", url = "https://www.mozilla.org/en-US/firefox/products/"),
        )
        val store = BrowserStore(
            BrowserState(
                tabs = tabList,
                selectedTabId = "a",
            ),
        )
        dispatchJoinMoveAction(store, listOf("a", "b", "c", "d", "e", "f"), "a", false)
        assertSameTabs(store, tabList, "all to a-")

        val movedTabList = listOf(
            createTab(id = "a", url = "https://www.mozilla.org"),
            createTab(id = "c", url = "https://getpocket.com"),
            createTab(id = "b", url = "https://www.firefox.com"),
            createTab(id = "e", url = "https://www.mozilla.org/en-US/firefox/features/"),
            createTab(id = "d", url = "https://www.example.org"),
            createTab(id = "f", url = "https://www.mozilla.org/en-US/firefox/products/"),
        )
        dispatchJoinMoveAction(store, listOf("b", "e"), "d", false)
        assertSameTabs(store, movedTabList, "b,e to d-")

        dispatchJoinMoveAction(store, listOf("c", "d"), "b", true)
        assertSameTabs(store, tabList, "c,d to b+")
    }
}
