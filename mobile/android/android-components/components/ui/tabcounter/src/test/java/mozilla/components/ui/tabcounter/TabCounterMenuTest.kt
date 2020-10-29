/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.ui.tabcounter

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class TabCounterMenuTest {

    @Test
    fun `return only the new tab item`() {
        val onItemTapped: (TabCounterMenu.Item) -> Unit = spy { Unit }
        val menu = TabCounterMenu(testContext, onItemTapped)

        val item = menu.newTabItem
        assertEquals("New tab", item.text)
        item.onClick()

        verify(onItemTapped).invoke(TabCounterMenu.Item.NewTab)
    }

    @Test
    fun `return only the new private tab item`() {
        val onItemTapped: (TabCounterMenu.Item) -> Unit = spy { Unit }
        val menu = TabCounterMenu(testContext, onItemTapped)

        val item = menu.newPrivateTabItem
        assertEquals("New private tab", item.text)
        item.onClick()

        verify(onItemTapped).invoke(TabCounterMenu.Item.NewPrivateTab)
    }

    @Test
    fun `return a close button`() {
        val onItemTapped: (TabCounterMenu.Item) -> Unit = spy { Unit }
        val menu = TabCounterMenu(testContext, onItemTapped)

        val item = menu.closeTabItem
        assertEquals("Close tab", item.text)
        item.onClick()

        verify(onItemTapped).invoke(TabCounterMenu.Item.CloseTab)
    }
}
