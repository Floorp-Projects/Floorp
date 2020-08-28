/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.appcompat.view.ContextThemeWrapper
import io.mockk.mockk
import io.mockk.verifyAll
import mozilla.components.concept.menu.candidate.DividerMenuCandidate
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class TabCounterMenuTest {

    private lateinit var context: Context
    private lateinit var metrics: MetricController
    private lateinit var onItemTapped: (TabCounterMenu.Item) -> Unit
    private lateinit var menu: TabCounterMenu

    @Before
    fun setup() {
        context = ContextThemeWrapper(testContext, R.style.NormalTheme)
        metrics = mockk(relaxed = true)
        onItemTapped = mockk(relaxed = true)
        menu = TabCounterMenu(context, metrics, onItemTapped)
    }

    @Test
    fun `all items use primary text color styling`() {
        val items = menu.menuItems(showOnly = null)
        assertEquals(4, items.size)

        val textItems = items.mapNotNull { it as? TextMenuCandidate }
        assertEquals(3, textItems.size)

        val primaryTextColor = context.getColor(R.color.primary_text_normal_theme)
        for (item in textItems) {
            assertEquals(primaryTextColor, item.textStyle.color)
            assertEquals(primaryTextColor, (item.start as DrawableMenuIcon).tint)
        }
    }

    @Test
    fun `return only the new tab item`() {
        val items = menu.menuItems(showOnly = BrowsingMode.Normal)
        assertEquals(1, items.size)

        val item = items[0] as TextMenuCandidate
        assertEquals("New tab", item.text)
        item.onClick()

        verifyAll {
            metrics.track(Event.TabCounterMenuItemTapped(Event.TabCounterMenuItemTapped.Item.NEW_TAB))
            onItemTapped(TabCounterMenu.Item.NewTab(BrowsingMode.Normal))
        }
    }

    @Test
    fun `return only the new private tab item`() {
        val items = menu.menuItems(showOnly = BrowsingMode.Private)
        assertEquals(1, items.size)

        val item = items[0] as TextMenuCandidate
        assertEquals("New private tab", item.text)
        item.onClick()

        verifyAll {
            metrics.track(Event.TabCounterMenuItemTapped(Event.TabCounterMenuItemTapped.Item.NEW_PRIVATE_TAB))
            onItemTapped(TabCounterMenu.Item.NewTab(BrowsingMode.Private))
        }
    }

    @Test
    fun `return two new tab items and a close button`() {
        val (newTab, newPrivateTab, divider, closeTab) = menu.menuItems(showOnly = null)

        assertEquals("New tab", (newTab as TextMenuCandidate).text)
        assertEquals("New private tab", (newPrivateTab as TextMenuCandidate).text)
        assertEquals("Close tab", (closeTab as TextMenuCandidate).text)
        assertEquals(DividerMenuCandidate(), divider)
    }
}
