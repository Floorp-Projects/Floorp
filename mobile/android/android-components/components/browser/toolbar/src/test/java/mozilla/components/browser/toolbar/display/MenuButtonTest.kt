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
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.concept.menu.MenuController
import mozilla.components.concept.menu.candidate.DecorativeTextMenuCandidate
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mock
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.mockito.MockitoAnnotations

@RunWith(AndroidJUnit4::class)
class MenuButtonTest {
    @Mock private lateinit var menuBuilder: BrowserMenuBuilder

    @Mock private lateinit var menuController: MenuController

    @Mock private lateinit var menu: BrowserMenu

    @Mock private lateinit var menuButtonInternal: mozilla.components.browser.menu.view.MenuButton
    private lateinit var menuButton: MenuButton

    @Before
    fun setup() {
        MockitoAnnotations.openMocks(this)
        `when`(menuBuilder.build(testContext)).thenReturn(menu)
        `when`(menuButtonInternal.context).thenReturn(testContext)

        menuButton = MenuButton(menuButtonInternal)
    }

    @Test
    fun `menu button is visible only if menu builder attached`() {
        verify(menuButtonInternal).visibility = View.GONE

        `when`(menuButtonInternal.menuBuilder).thenReturn(mock())
        assertTrue(menuButton.shouldBeVisible())

        `when`(menuButtonInternal.menuBuilder).thenReturn(null)
        assertFalse(menuButton.shouldBeVisible())
    }

    @Suppress("Deprecation")
    @Test
    fun `menu button sets onDismiss action`() {
        val action = {}
        menuButton.setMenuDismissAction(action)

        verify(menuButtonInternal).onDismiss = action
    }

    @Test
    fun `icon displays dot if low highlighted item is present in menu`() {
        verify(menuButtonInternal, never()).invalidateBrowserMenu()
        verify(menuButtonInternal, never()).setHighlight(any())

        var isHighlighted = false
        val highlight = BrowserMenuHighlight.LowPriority(Color.YELLOW)
        val highlightMenuBuilder = spy(
            BrowserMenuBuilder(
                listOf(
                    BrowserMenuHighlightableItem(
                        label = "Test",
                        startImageResource = 0,
                        highlight = highlight,
                        isHighlighted = { isHighlighted },
                    ),
                ),
            ),
        )
        doReturn(menu).`when`(highlightMenuBuilder).build(testContext)

        menuButton.menuBuilder = highlightMenuBuilder
        `when`(menuButtonInternal.menuBuilder).thenReturn(highlightMenuBuilder)
        menuButton.invalidateMenu()

        verify(menuButtonInternal).setHighlight(null)

        isHighlighted = true
        menuButton.invalidateMenu()

        assertEquals(highlight, highlightMenuBuilder.items.getHighlight())
        verify(menuButtonInternal).setHighlight(highlight)
    }

    @Test
    fun `invalidateMenu should invalidate the internal menu`() {
        `when`(menuButtonInternal.menuController).thenReturn(null)
        `when`(menuButtonInternal.menuBuilder).thenReturn(mock())
        verify(menuButtonInternal, never()).invalidateBrowserMenu()

        menuButton.invalidateMenu()

        verify(menuButtonInternal).invalidateBrowserMenu()
    }

    @Test
    fun `invalidateMenu should do nothing if using the menu controller`() {
        `when`(menuButtonInternal.menuController).thenReturn(menuController)
        `when`(menuButtonInternal.menuBuilder).thenReturn(null)
        verify(menuButtonInternal, never()).invalidateBrowserMenu()

        menuButton.invalidateMenu()

        verify(menuButtonInternal, never()).invalidateBrowserMenu()
    }

    @Test
    fun `invalidateMenu should automatically upgrade menu items if both builder and controller are present`() {
        val onClick = {}
        `when`(menuButtonInternal.menuController).thenReturn(menuController)
        `when`(menuButtonInternal.menuBuilder).thenReturn(
            BrowserMenuBuilder(
                listOf(
                    SimpleBrowserMenuItem("Item 1", listener = onClick),
                    SimpleBrowserMenuItem("Item 2"),
                ),
            ),
        )
        verify(menuButtonInternal, never()).invalidateBrowserMenu()

        menuButton.invalidateMenu()

        verify(menuButtonInternal, never()).invalidateBrowserMenu()
        verify(menuController).submitList(
            listOf(
                TextMenuCandidate("Item 1", onClick = onClick),
                DecorativeTextMenuCandidate("Item 2"),
            ),
        )
    }
}
