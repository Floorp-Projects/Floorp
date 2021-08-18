/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.view.LayoutInflater
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.tabstray.TabsAdapter.Companion.PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM
import mozilla.components.browser.tabstray.TabsAdapter.Companion.PAYLOAD_HIGHLIGHT_SELECTED_ITEM
import mozilla.components.concept.tabstray.Tab
import mozilla.components.concept.tabstray.Tabs
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.databinding.TabTrayItemBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.selection.SelectionHolder
import org.mozilla.fenix.tabstray.TabsTrayStore

@RunWith(FenixRobolectricTestRunner::class)
class BrowserTabsAdapterTest {

    private val context = testContext
    private val interactor = mockk<BrowserTrayInteractor>(relaxed = true)
    private val store = TabsTrayStore()

    @Test
    fun `WHEN bind with payloads is called THEN update the holder`() {
        val adapter = BrowserTabsAdapter(context, interactor, store)
        val holder = mockk<AbstractBrowserTabViewHolder>(relaxed = true)

        adapter.updateTabs(
            Tabs(
                list = listOf(
                    createTab("tab1")
                ),
                selectedIndex = 0
            )
        )

        adapter.onBindViewHolder(holder, 0, listOf(PAYLOAD_HIGHLIGHT_SELECTED_ITEM))

        verify { holder.updateSelectedTabIndicator(true) }

        adapter.onBindViewHolder(holder, 0, listOf(PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM))

        verify { holder.updateSelectedTabIndicator(false) }
    }

    @Test
    fun `WHEN the selection holder is set THEN update the selected tab`() {
        val adapter = BrowserTabsAdapter(context, interactor, store)
        val binding = TabTrayItemBinding.inflate(LayoutInflater.from(testContext))
        val holder = spyk(
            BrowserTabListViewHolder(
                imageLoader = mockk(),
                browserTrayInteractor = interactor,
                store = store,
                selectionHolder = null,
                itemView = binding.root
            )
        )
        val tab = createTab("tab1")

        every { holder.tab }.answers { tab }

        testSelectionHolder.internalState.add(tab)
        adapter.selectionHolder = testSelectionHolder

        adapter.updateTabs(
            Tabs(
                list = listOf(
                    tab
                ),
                selectedIndex = 0
            )
        )

        adapter.onBindViewHolder(holder, 0, listOf(PAYLOAD_DONT_HIGHLIGHT_SELECTED_ITEM))

        verify { holder.showTabIsMultiSelectEnabled(any(), true) }
    }

    private val testSelectionHolder = object : SelectionHolder<Tab> {
        override val selectedItems: Set<Tab>
            get() = internalState

        val internalState = mutableSetOf<Tab>()
    }
}
