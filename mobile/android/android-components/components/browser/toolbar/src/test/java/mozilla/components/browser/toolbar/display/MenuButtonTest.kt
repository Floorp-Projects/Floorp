/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.toolbar.display

import android.content.Context
import android.content.res.Resources
import android.graphics.PorterDuff
import android.graphics.PorterDuffColorFilter
import android.graphics.drawable.Drawable
import android.view.View
import androidx.annotation.ColorInt
import androidx.annotation.ColorRes
import androidx.appcompat.widget.AppCompatImageView
import androidx.core.view.isVisible
import androidx.core.view.iterator
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.BrowserMenuHighlightableItem
import mozilla.components.browser.toolbar.R
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
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
    private lateinit var menuButton: MenuButton

    @Before
    fun setup() {
        menu = mock()

        menuBuilder = mock()
        `when`(menuBuilder.build(testContext)).thenReturn(menu)

        menuButton = MenuButton(testContext)
    }

    @Test
    fun `menu button is visible only if menu builder attached`() {
        assertEquals(View.GONE, menuButton.visibility)

        menuButton.menuBuilder = mock()
        assertEquals(View.VISIBLE, menuButton.visibility)

        menuButton.menuBuilder = null
        assertEquals(View.GONE, menuButton.visibility)
    }

    @Test
    fun `changing menu builder dismisses old menu`() {
        menuButton.menuBuilder = menuBuilder
        menuButton.performClick()

        verify(menu).show(eq(menuButton), any(), anyBoolean(), any())

        menuButton.menuBuilder = mock()
        verify(menu).dismiss()
    }

    @Test
    fun `trying to open a new menu when we already have one will dismiss the current`() {
        menuButton.menuBuilder = menuBuilder

        menuButton.performClick()
        menuButton.performClick()

        verify(menu, times(1)).show(eq(menuButton), any(), anyBoolean(), any())
        verify(menu, times(1)).dismiss()
    }

    @Test
    fun `icon has content description`() {
        val icon = menuButton.extractIcon()

        assertEquals("Menu", icon.contentDescription)
        assertNotNull(icon.drawable)
    }

    @Test
    fun `icon color filter can be changed`() {
        val icon = menuButton.extractIcon()
        assertNull(icon.colorFilter)

        menuButton.setColorFilter(0xffffff)
        assertEquals(PorterDuffColorFilter(0xffffff, PorterDuff.Mode.SRC_ATOP), icon.colorFilter)
    }

    @Test
    fun `icon displays highlight if highlighted item is present in menu`() {
        val colorResource = 100
        val context = mockContextWithColorResource(colorResource, 0xffffff)

        val menuButton = MenuButton(context)
        val highlight = menuButton.extractHighlight()
        assertFalse(highlight.isVisible)

        var isHighlighted = false
        val highlightMenuBuilder = spy(BrowserMenuBuilder(listOf(
            BrowserMenuHighlightableItem(
                label = "Test",
                startImageResource = 0,
                highlight = BrowserMenuHighlightableItem.Highlight(0, 0, colorResource, colorResource),
                isHighlighted = { isHighlighted }
            )
        )))
        `when`(highlightMenuBuilder.build(context)).thenReturn(menu)

        menuButton.menuBuilder = highlightMenuBuilder
        menuButton.invalidateMenu()

        verify(menu, never()).invalidate()
        assertFalse(highlight.isVisible)

        isHighlighted = true
        menuButton.invalidateMenu()

        assertTrue(highlight.isVisible)
        verify(context).getDrawable(R.drawable.mozac_menu_indicator)
    }

    @Test
    fun `menu can be dismissed`() {
        menuButton.menu = menu

        menuButton.dismissMenu()

        verify(menuButton.menu)?.dismiss()
    }

    private fun mockContextWithColorResource(@ColorRes resId: Int, @ColorInt color: Int): Context {
        val context: Context = spy(testContext)

        val drawable: Drawable = mock()
        doReturn(drawable).`when`(drawable).mutate()
        doReturn(drawable).`when`(context).getDrawable(R.drawable.mozac_menu_indicator)

        val res: Resources = spy(testContext.resources)
        doReturn(res).`when`(context).resources

        doReturn(color).`when`(context).getColor(resId)
        @Suppress("Deprecation")
        doReturn(color).`when`(res).getColor(resId)

        return context
    }

    private fun MenuButton.extractIcon() =
        iterator().asSequence().last { it is AppCompatImageView } as AppCompatImageView

    private fun MenuButton.extractHighlight() =
        iterator().asSequence().first { it is AppCompatImageView } as AppCompatImageView
}
