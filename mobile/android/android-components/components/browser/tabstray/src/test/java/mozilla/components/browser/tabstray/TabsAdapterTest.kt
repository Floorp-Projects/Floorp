/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray

import android.view.View
import android.widget.FrameLayout
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.tabstray.TabsAdapter.Companion.PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM
import mozilla.components.browser.tabstray.TabsAdapter.Companion.PAYLOAD_HIGHLIGHT_SELECTED_ITEM
import mozilla.components.concept.tabstray.Tab
import mozilla.components.concept.tabstray.Tabs
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

private class TestTabViewHolder(view: View) : TabViewHolder(view) {
    override var tab: Tab? = null
    override fun bind(
        tab: Tab,
        isSelected: Boolean,
        styling: TabsTrayStyling,
        observable: Observable<TabsTray.Observer>
    ) { /* noop */ }

    override fun updateSelectedTabIndicator(showAsSelected: Boolean) { /* noop */ }
}

@RunWith(AndroidJUnit4::class)
class TabsAdapterTest {

    @Test
    fun `onCreateViewHolder will create a DefaultTabViewHolder`() {
        val adapter = TabsAdapter()

        val type = adapter.onCreateViewHolder(FrameLayout(testContext), 0)

        assertTrue(type is DefaultTabViewHolder)
    }

    @Test
    fun `onCreateViewHolder will create whatever TabViewHolder is provided`() {
        val adapter = TabsAdapter(viewHolderProvider = { _ -> TestTabViewHolder(View(testContext)) })

        val type = adapter.onCreateViewHolder(FrameLayout(testContext), 0)

        assertTrue(type is TestTabViewHolder)
    }

    @Test
    fun `itemCount will reflect number of sessions`() {
        val adapter = TabsAdapter()
        assertEquals(0, adapter.itemCount)

        adapter.updateTabs(
            Tabs(
                list = listOf(
                    Tab("A", "https://www.mozilla.org"),
                    Tab("B", "https://www.firefox.com")
                ),
                selectedIndex = 0
            )
        )
        assertEquals(2, adapter.itemCount)
    }

    @Test
    fun `onBindViewHolder calls bind on matching holder`() {
        val adapter = TabsAdapter()

        val holder: TabViewHolder = mock()

        val tab = Tab("A", "https://www.mozilla.org")

        adapter.updateTabs(
            Tabs(
                list = listOf(tab),
                selectedIndex = 0
            )
        )

        adapter.onBindViewHolder(holder, 0)

        verify(holder).bind(tab, true, adapter.styling, adapter)
    }

    @Test
    fun `tabs updated notifies observers`() {
        val adapter = TabsAdapter()
        val observer: TabsTray.Observer = mock()

        adapter.register(observer)

        adapter.updateTabs(
            Tabs(
                list = listOf(
                    Tab("A", "https://www.mozilla.org"),
                    Tab("B", "https://www.firefox.com"),
                    Tab("C", "https://getpocket.com")
                ),
                selectedIndex = 0
            )
        )

        verify(observer).onTabsUpdated()
    }

    @Test
    fun `onBindViewHolder will use payloads to indicate if this item is selected`() {
        val adapter = TabsAdapter()
        val holder = spy(TestTabViewHolder(View(testContext)))
        adapter.updateTabs(Tabs(listOf(mock(), mock()), selectedIndex = 1))

        adapter.onBindViewHolder(holder, 0, listOf(PAYLOAD_HIGHLIGHT_SELECTED_ITEM))
        verify(holder, never()).updateSelectedTabIndicator(ArgumentMatchers.anyBoolean())

        adapter.onBindViewHolder(holder, 1, listOf(PAYLOAD_HIGHLIGHT_SELECTED_ITEM))
        verify(holder).updateSelectedTabIndicator(true)
    }

    @Test
    fun `onBindViewHolder will use payloads to indicate if this item is not selected`() {
        val adapter = TabsAdapter()
        val holder = spy(TestTabViewHolder(View(testContext)))
        adapter.updateTabs(Tabs(listOf(mock(), mock()), selectedIndex = 1))

        adapter.onBindViewHolder(holder, 0, listOf(PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM))
        verify(holder, never()).updateSelectedTabIndicator(ArgumentMatchers.anyBoolean())

        adapter.onBindViewHolder(holder, 1, listOf(PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM))
        verify(holder).updateSelectedTabIndicator(false)
    }

    @Test
    fun `onBindViewHolder with payloads will return early if there are currently no open tabs`() {
        val adapter = TabsAdapter()
        val holder = TestTabViewHolder(View(testContext))
        val payloads = spy(arrayListOf(PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM))

        adapter.onBindViewHolder(holder, 0, payloads)
        // verify that calls we expect further down are not happening after the null check
        verify(payloads, never()).isEmpty()
        verify(payloads, never()).contains(ArgumentMatchers.anyInt())

        adapter.updateTabs(Tabs(emptyList(), selectedIndex = 0))
        adapter.onBindViewHolder(holder, 0, payloads)
        // verify that calls we expect further down are not happening after the null check
        verify(payloads, never()).isEmpty()
        verify(payloads, never()).contains(ArgumentMatchers.anyInt())
    }

    @Test
    fun `onBindViewHolder with empty payloads will call onBindViewHolder and return early for a full bind`() {
        val adapter = TabsAdapter()
        val holder = TestTabViewHolder(View(testContext))
        val emptyPayloads = spy(arrayListOf<String>())
        adapter.updateTabs(Tabs(listOf(mock()), selectedIndex = 0))

        adapter.onBindViewHolder(holder, 0, emptyPayloads)

        verify(emptyPayloads).isEmpty()
        // verify that calls we expect further down are not happening after the isEmpty check
        verify(emptyPayloads, never()).contains(ArgumentMatchers.anyString())
    }
}
