/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import androidx.recyclerview.widget.ConcatAdapter
import io.mockk.every
import io.mockk.mockk
import mozilla.components.browser.state.state.createTab
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.tabstray.TrayPagerAdapter.Companion.INACTIVE_TABS_FEATURE_NAME
import org.mozilla.fenix.tabstray.TrayPagerAdapter.Companion.TABS_TRAY_FEATURE_NAME
import org.mozilla.fenix.tabstray.TrayPagerAdapter.Companion.TAB_GROUP_FEATURE_NAME
import org.mozilla.fenix.tabstray.ext.browserAdapter
import org.mozilla.fenix.tabstray.ext.inactiveTabsAdapter
import org.mozilla.fenix.tabstray.ext.tabGroupAdapter
import org.mozilla.fenix.tabstray.ext.titleHeaderAdapter
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class TabSorterTest {
    private val context = testContext
    private val settings: Settings = mockk()
    private val metrics: MetricController = mockk()
    private var inactiveTimestamp = 0L
    private val tabsTrayStore = TabsTrayStore()

    @Before
    fun setUp() {
        every { settings.inactiveTabsAreEnabled }.answers { true }
        every { settings.searchTermTabGroupsAreEnabled }.answers { true }
        every { metrics.track(any()) }.answers { } // do nothing
    }

    @Test
    fun `WHEN updated with one normal tab THEN adapter have only one normal tab and no header`() {
        val adapter = ConcatAdapter(
            InactiveTabsAdapter(context, tabsTrayStore, mock(), mock(), INACTIVE_TABS_FEATURE_NAME, settings),
            TabGroupAdapter(context, mock(), mock(), TAB_GROUP_FEATURE_NAME),
            TitleHeaderAdapter(),
            BrowserTabsAdapter(context, mock(), mock(), TABS_TRAY_FEATURE_NAME)
        )
        val tabSorter = TabSorter(settings, metrics, adapter)

        tabSorter.updateTabs(
            listOf(
                createTab(url = "url", id = "tab1", lastAccess = System.currentTimeMillis())
            ),
            selectedTabId = "tab1"
        )

        assertEquals(adapter.itemCount, 1)
        assertEquals(adapter.inactiveTabsAdapter.inActiveTabsCount, 0)
        assertEquals(adapter.tabGroupAdapter.itemCount, 0)
        assertEquals(adapter.titleHeaderAdapter.itemCount, 0)
        assertEquals(adapter.browserAdapter.itemCount, 1)
    }

    @Test
    fun `WHEN updated with one normal tab and two search term tab THEN adapter have normal tab and a search group`() {
        val adapter = ConcatAdapter(
            InactiveTabsAdapter(context, tabsTrayStore, mock(), mock(), INACTIVE_TABS_FEATURE_NAME, settings),
            TabGroupAdapter(context, mock(), mock(), TAB_GROUP_FEATURE_NAME),
            TitleHeaderAdapter(),
            BrowserTabsAdapter(context, mock(), mock(), TABS_TRAY_FEATURE_NAME)
        )
        val tabSorter = TabSorter(settings, metrics, adapter)

        tabSorter.updateTabs(
            listOf(
                createTab(url = "url", id = "tab1", lastAccess = System.currentTimeMillis()),
                createTab(
                    url = "url",
                    id = "tab2",
                    lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                ),
                createTab(
                    url = "url",
                    id = "tab3",
                    lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                )
            ),
            selectedTabId = "tab1"
        )

        assertEquals(adapter.itemCount, 3)
        assertEquals(adapter.inactiveTabsAdapter.inActiveTabsCount, 0)
        assertEquals(adapter.tabGroupAdapter.itemCount, 1)
        assertEquals(adapter.titleHeaderAdapter.itemCount, 1)
        assertEquals(adapter.browserAdapter.itemCount, 1)
    }

    @Test
    fun `WHEN updated with one normal tab, one inactive tab and two search term tab THEN adapter have normal tab, inactive tab and a search group`() {
        val adapter = ConcatAdapter(
            InactiveTabsAdapter(context, tabsTrayStore, mock(), mock(), INACTIVE_TABS_FEATURE_NAME, settings),
            TabGroupAdapter(context, mock(), mock(), TAB_GROUP_FEATURE_NAME),
            TitleHeaderAdapter(),
            BrowserTabsAdapter(context, mock(), mock(), TABS_TRAY_FEATURE_NAME)
        )
        val tabSorter = TabSorter(settings, metrics, adapter)

        tabSorter.updateTabs(
            listOf(
                createTab(url = "url", id = "tab1", lastAccess = System.currentTimeMillis()),
                createTab(
                    url = "url",
                    id = "tab2",
                    lastAccess = inactiveTimestamp,
                    createdAt = inactiveTimestamp
                ),
                createTab(
                    url = "url",
                    id = "tab3",
                    lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                ),
                createTab(
                    url = "url",
                    id = "tab4",
                    lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                )
            ),
            selectedTabId = "tab1"
        )

        assertEquals(adapter.itemCount, 4)
        assertEquals(adapter.inactiveTabsAdapter.inActiveTabsCount, 1)
        assertEquals(adapter.tabGroupAdapter.itemCount, 1)
        assertEquals(adapter.titleHeaderAdapter.itemCount, 1)
        assertEquals(adapter.browserAdapter.itemCount, 1)
    }

    @Test
    fun `WHEN inactive tabs is off THEN adapter have no inactive tab`() {
        every { settings.inactiveTabsAreEnabled }.answers { false }
        val adapter = ConcatAdapter(
            InactiveTabsAdapter(context, tabsTrayStore, mock(), mock(), INACTIVE_TABS_FEATURE_NAME, settings),
            TabGroupAdapter(context, mock(), mock(), TAB_GROUP_FEATURE_NAME),
            TitleHeaderAdapter(),
            BrowserTabsAdapter(context, mock(), mock(), TABS_TRAY_FEATURE_NAME)
        )
        val tabSorter = TabSorter(settings, metrics, adapter)

        tabSorter.updateTabs(
            listOf(
                createTab(url = "url", id = "tab1", lastAccess = System.currentTimeMillis()),
                createTab(
                    url = "url",
                    id = "tab2",
                    lastAccess = inactiveTimestamp,
                    createdAt = inactiveTimestamp
                ),
                createTab(
                    url = "url",
                    id = "tab3",
                    lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                ),
                createTab(
                    url = "url",
                    id = "tab4",
                    lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                )
            ),
            selectedTabId = "tab1"
        )

        assertEquals(adapter.itemCount, 4)
        assertEquals(adapter.inactiveTabsAdapter.inActiveTabsCount, 0)
        assertEquals(adapter.tabGroupAdapter.itemCount, 1)
        assertEquals(adapter.titleHeaderAdapter.itemCount, 1)
        assertEquals(adapter.browserAdapter.itemCount, 2)
    }

    @Test
    fun `WHEN search term tabs is off THEN adapter have no search term group`() {
        every { settings.searchTermTabGroupsAreEnabled }.answers { false }
        val adapter = ConcatAdapter(
            InactiveTabsAdapter(context, tabsTrayStore, mock(), mock(), INACTIVE_TABS_FEATURE_NAME, settings),
            TabGroupAdapter(context, mock(), mock(), TAB_GROUP_FEATURE_NAME),
            TitleHeaderAdapter(),
            BrowserTabsAdapter(context, mock(), mock(), TABS_TRAY_FEATURE_NAME)
        )
        val tabSorter = TabSorter(settings, metrics, adapter)

        tabSorter.updateTabs(
            listOf(
                createTab(url = "url", id = "tab1", lastAccess = System.currentTimeMillis()),
                createTab(
                    url = "url",
                    id = "tab2",
                    lastAccess = inactiveTimestamp,
                    createdAt = inactiveTimestamp
                ),
                createTab(
                    url = "url",
                    id = "tab3",
                    lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                ),
                createTab(
                    url = "url",
                    id = "tab4",
                    lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                )
            ),
            selectedTabId = "tab1"
        )

        assertEquals(adapter.itemCount, 4)
        assertEquals(adapter.inactiveTabsAdapter.inActiveTabsCount, 1)
        assertEquals(adapter.tabGroupAdapter.itemCount, 0)
        assertEquals(adapter.titleHeaderAdapter.itemCount, 0)
        assertEquals(adapter.browserAdapter.itemCount, 3)
    }

    @Test
    fun `WHEN both inactive tabs and search term tabs are off THEN adapter have only normal tabs`() {
        every { settings.inactiveTabsAreEnabled }.answers { false }
        every { settings.searchTermTabGroupsAreEnabled }.answers { false }
        val adapter = ConcatAdapter(
            InactiveTabsAdapter(context, tabsTrayStore, mock(), mock(), INACTIVE_TABS_FEATURE_NAME, settings),
            TabGroupAdapter(context, mock(), mock(), TAB_GROUP_FEATURE_NAME),
            TitleHeaderAdapter(),
            BrowserTabsAdapter(context, mock(), mock(), TABS_TRAY_FEATURE_NAME)
        )
        val tabSorter = TabSorter(settings, metrics, adapter)

        tabSorter.updateTabs(
            listOf(
                createTab(url = "url", id = "tab1", lastAccess = System.currentTimeMillis()),
                createTab(
                    url = "url",
                    id = "tab2",
                    lastAccess = inactiveTimestamp
                ),
                createTab(
                    url = "url",
                    id = "tab3",
                    lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                ),
                createTab(
                    url = "url",
                    id = "tab4",
                    lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                )
            ),
            selectedTabId = "tab1"
        )

        assertEquals(adapter.itemCount, 4)
        assertEquals(adapter.inactiveTabsAdapter.inActiveTabsCount, 0)
        assertEquals(adapter.tabGroupAdapter.itemCount, 0)
        assertEquals(adapter.titleHeaderAdapter.itemCount, 0)
        assertEquals(adapter.browserAdapter.itemCount, 4)
    }

    @Test
    fun `WHEN only one search term tab THEN there is no search group`() {
        val adapter = ConcatAdapter(
            InactiveTabsAdapter(context, tabsTrayStore, mock(), mock(), INACTIVE_TABS_FEATURE_NAME, settings),
            TabGroupAdapter(context, mock(), mock(), TAB_GROUP_FEATURE_NAME),
            TitleHeaderAdapter(),
            BrowserTabsAdapter(context, mock(), mock(), TABS_TRAY_FEATURE_NAME)
        )
        val tabSorter = TabSorter(settings, metrics, adapter)

        tabSorter.updateTabs(
            listOf(
                createTab(
                    url = "url", id = "tab1", lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                )
            ),
            selectedTabId = "tab1"
        )

        assertEquals(adapter.itemCount, 1)
        assertEquals(adapter.inactiveTabsAdapter.inActiveTabsCount, 0)
        assertEquals(adapter.tabGroupAdapter.itemCount, 0)
        assertEquals(adapter.titleHeaderAdapter.itemCount, 0)
        assertEquals(adapter.browserAdapter.itemCount, 1)
    }

    @Test
    fun `WHEN remove second last one search term tab THEN search group is kept even if there's only one tab`() {
        val adapter = ConcatAdapter(
            InactiveTabsAdapter(context, tabsTrayStore, mock(), mock(), INACTIVE_TABS_FEATURE_NAME, settings),
            TabGroupAdapter(context, mock(), mock(), TAB_GROUP_FEATURE_NAME),
            TitleHeaderAdapter(),
            BrowserTabsAdapter(context, mock(), mock(), TABS_TRAY_FEATURE_NAME)
        )
        val tabSorter = TabSorter(settings, metrics, adapter)

        tabSorter.updateTabs(
            listOf(
                createTab(
                    url = "url", id = "tab1", lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                ),
                createTab(
                    url = "url", id = "tab2", lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                )
            ),
            selectedTabId = "tab1"
        )

        assertEquals(adapter.itemCount, 1)
        assertEquals(adapter.inactiveTabsAdapter.inActiveTabsCount, 0)
        assertEquals(adapter.tabGroupAdapter.itemCount, 1)
        assertEquals(adapter.titleHeaderAdapter.itemCount, 0)
        assertEquals(adapter.browserAdapter.itemCount, 0)

        tabSorter.updateTabs(
            listOf(
                createTab(
                    url = "url", id = "tab1", lastAccess = System.currentTimeMillis(),
                    searchTerms = "mozilla"
                )
            ),
            selectedTabId = "tab1"
        )

        assertEquals(adapter.itemCount, 1)
        assertEquals(adapter.inactiveTabsAdapter.inActiveTabsCount, 0)
        assertEquals(adapter.tabGroupAdapter.itemCount, 1)
        assertEquals(adapter.titleHeaderAdapter.itemCount, 0)
        assertEquals(adapter.browserAdapter.itemCount, 0)
    }
}
