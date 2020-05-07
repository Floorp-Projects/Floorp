/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray

import androidx.recyclerview.widget.DividerItemDecoration
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.tabstray.Tabs
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class BrowserTabsTrayTest {

    @Test
    fun `TabsTray concept methods are forwarded to adapter`() {
        val adapter: TabsAdapter = mock()
        val tabsTray = BrowserTabsTray(testContext, tabsAdapter = adapter)

        val tabs = Tabs(emptyList(), -1)

        tabsTray.updateTabs(tabs)
        verify(adapter).updateTabs(tabs)

        tabsTray.onTabsInserted(2, 5)
        verify(adapter).onTabsInserted(2, 5)

        tabsTray.onTabsRemoved(4, 1)
        verify(adapter).onTabsRemoved(4, 1)

        tabsTray.onTabsMoved(7, 1)
        verify(adapter).onTabsMoved(7, 1)

        tabsTray.onTabsChanged(0, 1)
        verify(adapter).onTabsChanged(0, 1)
    }

    @Test
    fun `TabsTray is set on adapter`() {
        val adapter = TabsAdapter()
        val tabsTray = BrowserTabsTray(testContext, tabsAdapter = adapter)

        assertEquals(tabsTray, adapter.tabsTray)
    }

    @Test
    fun `itemDecoration is set on recycler`() {
        val adapter = TabsAdapter()
        val decoration = DividerItemDecoration(
            testContext,
            DividerItemDecoration.VERTICAL
        )
        val tabsTray = BrowserTabsTray(testContext, tabsAdapter = adapter, itemDecoration = decoration)

        assertEquals(decoration, tabsTray.getItemDecorationAt(0))
        assertEquals(decoration, adapter.tabsTray.getItemDecorationAt(0))
    }
}
