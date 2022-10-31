/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray

import android.view.View
import android.widget.FrameLayout
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.tabstray.TabsAdapter.Companion.PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM
import mozilla.components.browser.tabstray.TabsAdapter.Companion.PAYLOAD_HIGHLIGHT_SELECTED_ITEM
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
    override var tab: TabSessionState? = null
    override fun bind(
        tab: TabSessionState,
        isSelected: Boolean,
        styling: TabsTrayStyling,
        delegate: TabsTray.Delegate,
    ) { /* noop */
    }

    override fun updateSelectedTabIndicator(showAsSelected: Boolean) { /* noop */
    }
}

@RunWith(AndroidJUnit4::class)
class TabsAdapterTest {

    @Test
    fun `onCreateViewHolder will create a DefaultTabViewHolder`() {
        val adapter = TabsAdapter(delegate = mock())

        val type = adapter.onCreateViewHolder(FrameLayout(testContext), 0)

        assertTrue(type is DefaultTabViewHolder)
    }

    @Test
    fun `onCreateViewHolder will create whatever TabViewHolder is provided`() {
        val adapter = TabsAdapter(
            viewHolderProvider = { _ -> TestTabViewHolder(View(testContext)) },
            delegate = mock(),
        )

        val type = adapter.onCreateViewHolder(FrameLayout(testContext), 0)

        assertTrue(type is TestTabViewHolder)
    }

    @Test
    fun `itemCount will reflect number of sessions`() {
        val adapter = TabsAdapter(delegate = mock())
        assertEquals(0, adapter.itemCount)

        adapter.updateTabs(
            listOf(
                createTab(id = "A", url = "https://www.mozilla.org"),
                createTab(id = "B", url = "https://www.firefox.com"),
            ),
            tabPartition = null,
            selectedTabId = "A",
        )
        assertEquals(2, adapter.itemCount)
    }

    @Test
    fun `onBindViewHolder calls bind on matching holder`() {
        val styling = TabsTrayStyling()
        val delegate = mock<TabsTray.Delegate>()
        val adapter = TabsAdapter(delegate = delegate, styling = styling)

        val holder: TabViewHolder = mock()

        val tab = createTab(id = "A", url = "https://www.mozilla.org")

        adapter.updateTabs(
            listOf(tab),
            null,
            "A",
        )

        adapter.onBindViewHolder(holder, 0)

        verify(holder).bind(tab, true, styling, delegate)
    }

    @Test
    fun `onBindViewHolder will use payloads to indicate if this item is selected`() {
        val adapter = TabsAdapter(delegate = mock())
        val holder = spy(TestTabViewHolder(View(testContext)))
        val tab = createTab(id = "A", url = "https://www.mozilla.org")

        adapter.updateTabs(listOf(mock(), tab), tabPartition = null, selectedTabId = "A")

        adapter.onBindViewHolder(holder, 0, listOf(PAYLOAD_HIGHLIGHT_SELECTED_ITEM))
        verify(holder, never()).updateSelectedTabIndicator(ArgumentMatchers.anyBoolean())

        adapter.onBindViewHolder(holder, 1, listOf(PAYLOAD_HIGHLIGHT_SELECTED_ITEM))
        verify(holder).updateSelectedTabIndicator(true)
    }

    @Test
    fun `onBindViewHolder will use payloads to indicate if this item is not selected`() {
        val adapter = TabsAdapter(delegate = mock())
        val holder = spy(TestTabViewHolder(View(testContext)))
        val tab = createTab(id = "A", url = "https://www.mozilla.org")
        adapter.updateTabs(listOf(mock(), tab), tabPartition = null, selectedTabId = "A")

        adapter.onBindViewHolder(holder, 0, listOf(PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM))
        verify(holder, never()).updateSelectedTabIndicator(ArgumentMatchers.anyBoolean())

        adapter.onBindViewHolder(holder, 1, listOf(PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM))
        verify(holder).updateSelectedTabIndicator(false)
    }

    @Test
    fun `onBindViewHolder with payloads will return early if there are currently no open tabs`() {
        val adapter = TabsAdapter(delegate = mock())
        val holder = TestTabViewHolder(View(testContext))
        val payloads = spy(arrayListOf(PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM))

        adapter.onBindViewHolder(holder, 0, payloads)
        // verify that calls we expect further down are not happening after the null check
        verify(payloads, never()).isEmpty()
        verify(payloads, never()).contains(ArgumentMatchers.anyInt())

        adapter.updateTabs(emptyList(), tabPartition = null, selectedTabId = null)
        adapter.onBindViewHolder(holder, 0, payloads)
        // verify that calls we expect further down are not happening after the null check
        verify(payloads, never()).isEmpty()
        verify(payloads, never()).contains(ArgumentMatchers.anyInt())
    }

    @Test
    fun `onBindViewHolder with empty payloads will call onBindViewHolder and return early for a full bind`() {
        val adapter = TabsAdapter(delegate = mock())
        val holder = TestTabViewHolder(View(testContext))
        val emptyPayloads = spy(arrayListOf<String>())

        adapter.updateTabs(listOf(mock()), tabPartition = null, selectedTabId = null)

        adapter.onBindViewHolder(holder, 0, emptyPayloads)

        verify(emptyPayloads).isEmpty()
        // verify that calls we expect further down are not happening after the isEmpty check
        verify(emptyPayloads, never()).contains(ArgumentMatchers.anyString())
    }
}
