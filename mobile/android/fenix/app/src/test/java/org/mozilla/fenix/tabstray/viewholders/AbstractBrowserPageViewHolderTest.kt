/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.viewholders

import android.view.LayoutInflater
import android.view.View.GONE
import android.view.View.VISIBLE
import android.widget.TextView
import io.mockk.every
import io.mockk.mockk
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.tabstray.browser.AbstractBrowserTrayList
import org.mozilla.fenix.tabstray.browser.BrowserTabsAdapter

@RunWith(FenixRobolectricTestRunner::class)
class AbstractBrowserPageViewHolderTest {
    val tabsTrayStore: TabsTrayStore = TabsTrayStore()
    val browserStore = BrowserStore()
    val interactor = mockk<TabsTrayInteractor>(relaxed = true)
    init {
        every { testContext.components.core.thumbnailStorage } returns mockk()
        every { testContext.components.settings } returns mockk(relaxed = true)
    }

    val adapter =
        BrowserTabsAdapter(testContext, interactor, tabsTrayStore, "Test", mockk())

    @Test
    fun `WHEN tabs inserted THEN show tray`() {
        val itemView =
            LayoutInflater.from(testContext).inflate(R.layout.normal_browser_tray_list, null)
        val viewHolder = PrivateBrowserPageViewHolder(itemView, tabsTrayStore, browserStore, interactor)
        val trayList: AbstractBrowserTrayList = itemView.findViewById(R.id.tray_list_item)
        val emptyList: TextView = itemView.findViewById(R.id.tab_tray_empty_view)

        viewHolder.bind(adapter)
        viewHolder.attachedToWindow()

        adapter.updateTabs(listOf(createTab(url = "url", id = "tab1")), null, "tab1")

        assertTrue(trayList.visibility == VISIBLE)
        assertTrue(emptyList.visibility == GONE)
    }

    @Test
    fun `WHEN no tabs THEN show empty view`() {
        val itemView =
            LayoutInflater.from(testContext).inflate(R.layout.normal_browser_tray_list, null)
        val viewHolder = PrivateBrowserPageViewHolder(itemView, tabsTrayStore, browserStore, interactor)
        val trayList: AbstractBrowserTrayList = itemView.findViewById(R.id.tray_list_item)
        val emptyList: TextView = itemView.findViewById(R.id.tab_tray_empty_view)

        viewHolder.bind(adapter)
        viewHolder.attachedToWindow()

        adapter.updateTabs(emptyList(), null, "")

        assertTrue(trayList.visibility == GONE)
        assertTrue(emptyList.visibility == VISIBLE)
    }
}
