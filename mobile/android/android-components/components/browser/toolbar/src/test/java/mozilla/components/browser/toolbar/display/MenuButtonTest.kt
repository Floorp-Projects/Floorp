/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import android.graphics.Color
import android.view.View
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.BrowserMenuHighlight
import mozilla.components.browser.menu.ext.getHighlight
import mozilla.components.browser.menu.item.BrowserMenuHighlightableItem
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class MenuButtonTest {
    private lateinit var menuBuilder: BrowserMenuBuilder
    private lateinit var menu: BrowserMenu
    private lateinit var menuButtonInternal: mozilla.components.browser.menu.view.MenuButton
    private lateinit var menuButton: MenuButton

    @Before
    fun setup() {
        menu = mock()
        menuBuilder = mock()
        doReturn(menu).`when`(menuBuilder).build(testContext)

        menuButtonInternal = mock()
        menuButton = MenuButton(menuButtonInternal)
    }

    @Test
    fun `menu button is visible only if menu builder attached`() {
        verify(menuButtonInternal).visibility = View.GONE

        menuButton.menuBuilder = mock()
        verify(menuButtonInternal).visibility = View.VISIBLE

        menuButton.menuBuilder = null
        verify(menuButtonInternal, times(2)).visibility = View.GONE
    }

    @Test
    fun `icon displays dot if low highlighted item is present in menu`() {
        verify(menuButtonInternal, never()).invalidateBrowserMenu()
        verify(menuButtonInternal, never()).setHighlight(any())

        var isHighlighted = false
        val highlight = BrowserMenuHighlight.LowPriority(Color.YELLOW)
        val highlightMenuBuilder = spy(BrowserMenuBuilder(listOf(
            BrowserMenuHighlightableItem(
                label = "Test",
                startImageResource = 0,
                highlight = highlight,
                isHighlighted = { isHighlighted }
            )
        )))
        `when`(highlightMenuBuilder.build(testContext)).thenReturn(menu)

        menuButton.menuBuilder = highlightMenuBuilder
        doReturn(highlightMenuBuilder).`when`(menuButtonInternal).menuBuilder
        menuButton.invalidateMenu()

        verify(menuButtonInternal).setHighlight(null)

        isHighlighted = true
        menuButton.invalidateMenu()

        assertEquals(highlight, highlightMenuBuilder.items.getHighlight())
        verify(menuButtonInternal).setHighlight(highlight)
    }
}
