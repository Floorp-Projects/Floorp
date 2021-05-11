/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.viewholders

import android.view.LayoutInflater
import android.view.View.GONE
import android.view.View.VISIBLE
import android.widget.TextView
import androidx.recyclerview.widget.LinearLayoutManager
import io.mockk.mockk
import mozilla.components.concept.tabstray.Tabs
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.tabstray.TabsTrayInteractor
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.tabstray.browser.BrowserTrayList
import org.mozilla.fenix.tabstray.browser.BrowserTabsAdapter
import org.mozilla.fenix.tabstray.browser.BrowserTrayInteractor
import org.mozilla.fenix.tabstray.browser.createTab

@RunWith(FenixRobolectricTestRunner::class)
class AbstractBrowserPageViewHolderTest {
    val store: TabsTrayStore = TabsTrayStore()
    val interactor = mockk<TabsTrayInteractor>(relaxed = true)
    val browserTrayInteractor = mockk<BrowserTrayInteractor>(relaxed = true)
    val adapter = BrowserTabsAdapter(testContext, browserTrayInteractor, store)

    @Test
    fun `WHEN tabs inserted THEN show tray`() {
        val itemView =
            LayoutInflater.from(testContext).inflate(R.layout.normal_browser_tray_list, null)
        val viewHolder = NormalBrowserPageViewHolder(itemView, store, interactor, 5)
        val trayList: BrowserTrayList = itemView.findViewById(R.id.tray_list_item)
        val emptyList: TextView = itemView.findViewById(R.id.tab_tray_empty_view)

        viewHolder.bind(adapter, LinearLayoutManager(testContext))

        adapter.updateTabs(
            Tabs(
                list = listOf(
                    createTab("tab1")
                ),
                selectedIndex = 0
            )
        )
        adapter.onTabsInserted(0, 1)

        assertTrue(trayList.visibility == VISIBLE)
        assertTrue(emptyList.visibility == GONE)
    }

    @Test
    fun `WHEN no tabs THEN show empty view`() {
        val itemView =
            LayoutInflater.from(testContext).inflate(R.layout.normal_browser_tray_list, null)
        val viewHolder = NormalBrowserPageViewHolder(itemView, store, interactor, 5)
        val trayList: BrowserTrayList = itemView.findViewById(R.id.tray_list_item)
        val emptyList: TextView = itemView.findViewById(R.id.tab_tray_empty_view)

        viewHolder.bind(adapter, LinearLayoutManager(testContext))

        adapter.updateTabs(
            Tabs(
                list = emptyList(),
                selectedIndex = 0
            )
        )
        adapter.onTabsInserted(0, 0)

        assertTrue(trayList.visibility == GONE)
        assertTrue(emptyList.visibility == VISIBLE)
    }
}
