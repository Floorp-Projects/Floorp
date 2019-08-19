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
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mock
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mockito.MockitoAnnotations.initMocks

@RunWith(AndroidJUnit4::class)
class MenuButtonTest {

    @Mock private lateinit var parent: View
    @Mock private lateinit var menuBuilder: BrowserMenuBuilder
    @Mock private lateinit var menu: BrowserMenu
    private lateinit var menuButton: MenuButton

    @Before
    fun setup() {
        initMocks(this)
        menuButton = MenuButton(testContext, parent)

        `when`(parent.layoutParams).thenReturn(mock())
        `when`(menuBuilder.build(testContext)).thenReturn(menu)
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
    fun `icon has content description`() {
        val icon = extractIcon()

        assertEquals("Menu", icon.contentDescription)
        assertNotNull(icon.drawable)
    }

    @Test
    fun `icon color filter can be changed`() {
        val icon = extractIcon()
        assertNull(icon.colorFilter)

        menuButton.setColorFilter(0xffffff)
        assertEquals(PorterDuffColorFilter(0xffffff, PorterDuff.Mode.SRC_ATOP), icon.colorFilter)
    }

    @Test
    fun `icon displays highlight if highlighted item is present in menu`() {
        val colorResource = 100
        val context = mockContextWithColorResource(colorResource, 0xffffff)

        val menuButton = MenuButton(context, parent)
        val icon = extractIcon()
        assertNull(icon.background)

        var isHighlighted = false
        val highlightMenuBuilder = spy(BrowserMenuBuilder(listOf(
            BrowserMenuHighlightableItem(
                label = "Test",
                imageResource = 0,
                highlight = BrowserMenuHighlightableItem.Highlight(0, colorResource, colorResource),
                isHighlighted = { isHighlighted }
            )
        )))
        `when`(highlightMenuBuilder.build(context)).thenReturn(menu)

        menuButton.menuBuilder = highlightMenuBuilder
        menuButton.invalidateMenu()

        verify(menu, never()).invalidate()
        assertNull(icon.background)

        isHighlighted = true
        menuButton.invalidateMenu()

        verify(context).getDrawable(R.drawable.mozac_menu_indicator)
    }

    @Test
    fun `menu can be dismissed`() {
        menuButton.menu = menu

        menuButton.dismissMenu()

        verify(menuButton.menu)?.dismiss()
    }

    private fun extractIcon() =
        menuButton.iterator().asSequence().find { it is AppCompatImageView } as AppCompatImageView

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
}
