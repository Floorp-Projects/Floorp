/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.view.View
import android.view.ViewGroup
import mozilla.components.browser.tabstray.TabViewHolder
import mozilla.components.browser.tabstray.TabsTrayStyling
import mozilla.components.concept.tabstray.Tab
import mozilla.components.concept.tabstray.Tabs
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.base.observer.Observable
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class TabsAdapterTest {

    lateinit var adapter: TabsAdapter<TestTabsAdapter.ViewHolder>

    @Before
    fun setup() {
        adapter = TestTabsAdapter()
    }

    @Test
    fun `getItemId gives a new ID for each position`() {
        val (tab1, tab2, tab3) = Triple(createTab(), createTab(), createTab())
        val tabs = Tabs(
            list = listOf(tab1, tab2, tab3),
            selectedIndex = 0
        )

        adapter.updateTabs(tabs)

        val id1 = adapter.getItemId(0)
        val id2 = adapter.getItemId(1)
        val id3 = adapter.getItemId(2)
        val id1Again = adapter.getItemId(0)

        assertEquals(id1, id1Again)
        assertNotEquals(id1, id2)
        assertNotEquals(id1, id3)
        assertNotEquals(id2, id3)
    }

    @Test(expected = IllegalStateException::class)
    fun `getItemId throws if a tab does not exist for the position`() {
        adapter.getItemId(4)
    }

    class TestTabsAdapter : TabsAdapter<TestTabsAdapter.ViewHolder>() {

        inner class ViewHolder(view: View) : TabViewHolder(view) {
            override var tab: Tab? = null

            override fun bind(
                tab: Tab,
                isSelected: Boolean,
                styling: TabsTrayStyling,
                observable: Observable<TabsTray.Observer>
            ) = throw UnsupportedOperationException()
        }

        override fun onCreateViewHolder(
            parent: ViewGroup,
            viewType: Int
        ): ViewHolder = throw UnsupportedOperationException()
    }
}
