/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.appcompat.view.ContextThemeWrapper
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.menu.candidate.DividerMenuCandidate
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.ui.tabcounter.TabCounterMenu
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class TabCounterMenuTest {

    private lateinit var context: Context
    private lateinit var onItemTapped: (TabCounterMenu.Item) -> Unit
    private lateinit var menu: FenixTabCounterMenu

    @Before
    fun setup() {
        context = ContextThemeWrapper(testContext, R.style.NormalTheme)
        onItemTapped = mockk(relaxed = true)
        menu = FenixTabCounterMenu(context, onItemTapped)
    }

    @Test
    fun `return only the new tab item`() {
        val items = menu.menuItems(showOnly = BrowsingMode.Normal)
        assertEquals(1, items.size)

        val item = items[0] as TextMenuCandidate
        assertEquals("New tab", item.text)
        item.onClick()

        verify { onItemTapped(TabCounterMenu.Item.NewTab) }
    }

    @Test
    fun `return only the new private tab item`() {
        val items = menu.menuItems(showOnly = BrowsingMode.Private)
        assertEquals(1, items.size)

        val item = items[0] as TextMenuCandidate
        assertEquals("New private tab", item.text)
        item.onClick()

        verify { onItemTapped(TabCounterMenu.Item.NewPrivateTab) }
    }

    @Test
    fun `return two new tab items and a close button`() {
        val (newTab, newPrivateTab, divider, closeTab) = menu.menuItems(ToolbarPosition.TOP)

        assertEquals("New tab", (newTab as TextMenuCandidate).text)
        assertEquals("New private tab", (newPrivateTab as TextMenuCandidate).text)
        assertEquals("Close tab", (closeTab as TextMenuCandidate).text)
        assertEquals(DividerMenuCandidate(), divider)
    }
}
