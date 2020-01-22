/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.drawable.BitmapDrawable
import android.view.LayoutInflater
import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.menu.R
import mozilla.components.browser.menu.WebExtensionBrowserMenu
import mozilla.components.concept.engine.webextension.BrowserAction
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Assert
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class WebExtensionBrowserMenuItemTest {

    private val testDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    @Test
    fun `web extension menu item is visible by default`() {
        val webExtMenuItem = WebExtensionBrowserMenuItem(mock(), mock())

        assertTrue(webExtMenuItem.visible())
    }

    @Test
    fun `layout resource can be inflated`() {
        val webExtMenuItem = WebExtensionBrowserMenuItem(mock(), mock())

        val view = LayoutInflater.from(testContext)
            .inflate(webExtMenuItem.getLayoutResource(), null)

        assertNotNull(view)
    }

    @Test
    fun `view is disabled if browser action is disabled`() {
        val icon: Bitmap = mock()
        val imageView: ImageView = mock()
        val badgeView: TextView = mock()
        val labelView: TextView = mock()
        val view: View = mock()

        whenever(view.findViewById<ImageView>(R.id.action_image)).thenReturn(imageView)
        whenever(view.findViewById<TextView>(R.id.badge_text)).thenReturn(badgeView)
        whenever(view.findViewById<TextView>(R.id.action_label)).thenReturn(labelView)
        whenever(view.context).thenReturn(mock())

        val browserAction = BrowserAction(
            title = "title",
            loadIcon = { icon },
            enabled = false,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE
        ) {}

        val action = WebExtensionBrowserMenuItem(browserAction) {}
        action.bind(mock(), view)
        testDispatcher.advanceUntilIdle()

        assertFalse(view.isEnabled)
    }

    @Test
    fun bind() {
        val icon: Bitmap = mock()
        val imageView: ImageView = mock()
        val badgeView: TextView = mock()
        val labelView: TextView = mock()
        val view: View = mock()

        whenever(view.findViewById<ImageView>(R.id.action_image)).thenReturn(imageView)
        whenever(view.findViewById<TextView>(R.id.badge_text)).thenReturn(badgeView)
        whenever(view.findViewById<TextView>(R.id.action_label)).thenReturn(labelView)
        whenever(view.context).thenReturn(mock())

        val browserAction = BrowserAction(
            title = "title",
            loadIcon = { icon },
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE
        ) {}

        val action = WebExtensionBrowserMenuItem(browserAction) {}
        action.bind(mock(), view)
        testDispatcher.advanceUntilIdle()

        val iconCaptor = argumentCaptor<BitmapDrawable>()
        verify(imageView).setImageDrawable(iconCaptor.capture())
        Assert.assertEquals(icon, iconCaptor.value.bitmap)

        verify(imageView).contentDescription = "title"
        verify(labelView).setText("title")
        verify(badgeView).setText("badgeText")
        verify(badgeView).setTextColor(Color.WHITE)
        verify(badgeView).setBackgroundColor(Color.BLUE)
    }

    @Test
    fun fallbackToDefaultIcon() {
        val imageView: ImageView = mock()
        val badgeView: TextView = mock()
        val labelView: TextView = mock()
        val view: View = mock()

        whenever(view.findViewById<ImageView>(R.id.action_image)).thenReturn(imageView)
        whenever(view.findViewById<TextView>(R.id.badge_text)).thenReturn(badgeView)
        whenever(view.findViewById<TextView>(R.id.action_label)).thenReturn(labelView)
        whenever(view.context).thenReturn(mock())

        val browserAction = BrowserAction(
                title = "title",
                loadIcon = { throw IllegalArgumentException() },
                enabled = true,
                badgeText = "badgeText",
                badgeTextColor = Color.WHITE,
                badgeBackgroundColor = Color.BLUE
        ) {}

        val action = WebExtensionBrowserMenuItem(browserAction) {}
        action.bind(mock(), view)
        testDispatcher.advanceUntilIdle()

        verify(imageView).setImageResource(R.drawable.mozac_ic_web_extension_default_icon)
    }

    @Test
    fun `clicking item view invokes callback and dismisses menu`() {
        var callbackInvoked = false

        val icon: Bitmap = mock()
        val imageView: ImageView = mock()
        val badgeView: TextView = mock()
        val labelView = TextView(testContext)
        val view: View = mock()

        whenever(view.findViewById<ImageView>(R.id.action_image)).thenReturn(imageView)
        whenever(view.findViewById<TextView>(R.id.badge_text)).thenReturn(badgeView)
        whenever(view.findViewById<TextView>(R.id.action_label)).thenReturn(labelView)
        whenever(view.context).thenReturn(mock())

        val browserAction = BrowserAction(
            title = "title",
            loadIcon = { icon },
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE
        ) {}

        val item = WebExtensionBrowserMenuItem(browserAction) {
            callbackInvoked = true
        }

        val menu: WebExtensionBrowserMenu = mock()

        item.bind(menu, view)
        testDispatcher.advanceUntilIdle()

        labelView.performClick()

        assertTrue(callbackInvoked)
        verify(menu).dismiss()
    }

    @Test
    fun `labelView and badgeView redraws when invalidate is triggered`() {
        val icon: Bitmap = mock()
        val imageView: ImageView = mock()
        val badgeView: TextView = mock()
        val labelView: TextView = mock()
        val view: View = mock()

        whenever(view.findViewById<ImageView>(R.id.action_image)).thenReturn(imageView)
        whenever(view.findViewById<TextView>(R.id.badge_text)).thenReturn(badgeView)
        whenever(view.findViewById<TextView>(R.id.action_label)).thenReturn(labelView)
        whenever(view.context).thenReturn(mock())

        val browserAction = BrowserAction(
            title = "title",
            loadIcon = { icon },
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE
        ) {}

        val item = WebExtensionBrowserMenuItem(browserAction) {}

        val menu: WebExtensionBrowserMenu = mock()

        item.bind(menu, view)
        testDispatcher.advanceUntilIdle()

        verify(labelView).setText("title")
        verify(badgeView).setText("badgeText")

        val browserActionOverride = BrowserAction(
                title = "override",
                loadIcon = { icon },
                enabled = true,
                badgeText = "overrideBadge",
                badgeTextColor = Color.WHITE,
                badgeBackgroundColor = Color.BLUE
        ) {}

        item.browserAction = browserActionOverride
        item.invalidate(view)

        verify(labelView).setText("override")
        verify(badgeView).setText("overrideBadge")
        verify(labelView).invalidate()
        verify(badgeView).invalidate()
    }
}
