/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Drawable
import android.view.LayoutInflater
import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.menu.R
import mozilla.components.browser.menu.WebExtensionBrowserMenu
import mozilla.components.concept.engine.webextension.Action
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.notNull
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
@OptIn(ExperimentalCoroutinesApi::class)
class WebExtensionBrowserMenuItemTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher

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
        val container = View(testContext)
        val view: View = mock()

        whenever(view.findViewById<ImageView>(R.id.action_image)).thenReturn(imageView)
        whenever(view.findViewById<TextView>(R.id.badge_text)).thenReturn(badgeView)
        whenever(view.findViewById<TextView>(R.id.action_label)).thenReturn(labelView)
        whenever(view.findViewById<View>(R.id.container)).thenReturn(container)
        whenever(view.context).thenReturn(mock())

        val browserAction = Action(
            title = "title",
            loadIcon = { icon },
            enabled = false,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE,
        ) {}

        val action = WebExtensionBrowserMenuItem(browserAction, {})
        action.bind(mock(), view)
        dispatcher.scheduler.advanceUntilIdle()

        assertFalse(view.isEnabled)
    }

    @Test
    fun bind() {
        val icon: Bitmap = mock()
        val imageView: ImageView = mock()
        val badgeView: TextView = mock()
        val background: Drawable = mock()
        val labelView: TextView = mock()
        val container = View(testContext)
        val view: View = mock()

        whenever(badgeView.background).thenReturn(background)
        whenever(view.findViewById<ImageView>(R.id.action_image)).thenReturn(imageView)
        whenever(view.findViewById<TextView>(R.id.badge_text)).thenReturn(badgeView)
        whenever(view.findViewById<TextView>(R.id.action_label)).thenReturn(labelView)
        whenever(view.findViewById<View>(R.id.container)).thenReturn(container)
        whenever(view.context).thenReturn(testContext)

        val browserAction = Action(
            title = "title",
            loadIcon = { icon },
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE,
        ) {}

        val action = WebExtensionBrowserMenuItem(browserAction, {})
        action.bind(mock(), view)
        dispatcher.scheduler.advanceUntilIdle()

        val iconCaptor = argumentCaptor<BitmapDrawable>()
        verify(imageView).setImageDrawable(iconCaptor.capture())
        assertEquals(icon, iconCaptor.value.bitmap)

        verify(imageView).contentDescription = "title"
        verify(labelView).text = "title"
        verify(badgeView).text = "badgeText"
        verify(badgeView).setTextColor(Color.WHITE)
        verify(background).setTint(Color.BLUE)
    }

    @Test
    fun `badge text view is invisible if action badge text is empty`() {
        val icon: Bitmap = mock()
        val imageView: ImageView = mock()
        val badgeView: TextView = spy(TextView(testContext))
        val labelView: TextView = mock()
        val container = View(testContext)
        val view: View = mock()

        whenever(view.findViewById<ImageView>(R.id.action_image)).thenReturn(imageView)
        whenever(view.findViewById<TextView>(R.id.badge_text)).thenReturn(badgeView)
        whenever(view.findViewById<TextView>(R.id.action_label)).thenReturn(labelView)
        whenever(view.findViewById<View>(R.id.container)).thenReturn(container)
        whenever(view.context).thenReturn(testContext)

        val badgeText = ""
        val browserAction = Action(
            title = "title",
            loadIcon = { icon },
            enabled = true,
            badgeText = badgeText,
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE,
        ) {}

        val action = WebExtensionBrowserMenuItem(browserAction, {})
        action.bind(mock(), view)
        dispatcher.scheduler.advanceUntilIdle()

        verify(badgeView).setBadgeText(badgeText)
        assertEquals(View.INVISIBLE, badgeView.visibility)
    }

    @Test
    fun fallbackToDefaultIcon() {
        val imageView: ImageView = mock()
        val badgeView: TextView = mock()
        val labelView: TextView = mock()
        val container = View(testContext)
        val view: View = mock()

        whenever(view.findViewById<ImageView>(R.id.action_image)).thenReturn(imageView)
        whenever(view.findViewById<TextView>(R.id.badge_text)).thenReturn(badgeView)
        whenever(view.findViewById<TextView>(R.id.action_label)).thenReturn(labelView)
        whenever(view.findViewById<View>(R.id.container)).thenReturn(container)
        whenever(view.context).thenReturn(testContext)

        val browserAction = Action(
            title = "title",
            loadIcon = { throw IllegalArgumentException() },
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE,
        ) {}

        val action = WebExtensionBrowserMenuItem(browserAction, {})
        action.bind(mock(), view)
        dispatcher.scheduler.advanceUntilIdle()

        verify(imageView).setImageDrawable(notNull())
    }

    @Test
    fun `clicking item view invokes callback and dismisses menu`() {
        var callbackInvoked = false

        val icon: Bitmap = mock()
        val imageView: ImageView = mock()
        val badgeView: TextView = mock()
        val labelView = TextView(testContext)
        val container = View(testContext)
        val view: View = mock()

        whenever(view.findViewById<ImageView>(R.id.action_image)).thenReturn(imageView)
        whenever(view.findViewById<TextView>(R.id.badge_text)).thenReturn(badgeView)
        whenever(view.findViewById<TextView>(R.id.action_label)).thenReturn(labelView)
        whenever(view.findViewById<View>(R.id.container)).thenReturn(container)
        whenever(view.context).thenReturn(mock())

        val browserAction = Action(
            title = "title",
            loadIcon = { icon },
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE,
        ) {}

        val item = WebExtensionBrowserMenuItem(browserAction, { callbackInvoked = true })

        val menu: WebExtensionBrowserMenu = mock()

        item.bind(menu, view)
        dispatcher.scheduler.advanceUntilIdle()

        container.performClick()

        assertTrue(callbackInvoked)
        verify(menu).dismiss()
    }

    @Test
    fun `labelView and badgeView redraws when invalidate is triggered`() {
        val icon: Bitmap = mock()
        val imageView: ImageView = mock()
        val badgeView: TextView = mock()
        val labelView: TextView = mock()
        val container = View(testContext)
        val view: View = mock()

        whenever(view.findViewById<ImageView>(R.id.action_image)).thenReturn(imageView)
        whenever(view.findViewById<TextView>(R.id.badge_text)).thenReturn(badgeView)
        whenever(view.findViewById<TextView>(R.id.action_label)).thenReturn(labelView)
        whenever(view.findViewById<View>(R.id.container)).thenReturn(container)
        whenever(view.context).thenReturn(mock())

        val browserAction = Action(
            title = "title",
            loadIcon = { icon },
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE,
        ) {}

        val item = WebExtensionBrowserMenuItem(browserAction, {})

        val menu: WebExtensionBrowserMenu = mock()

        item.bind(menu, view)
        dispatcher.scheduler.advanceUntilIdle()

        verify(labelView).text = "title"
        verify(badgeView).text = "badgeText"

        val browserActionOverride = Action(
            title = "override",
            loadIcon = { icon },
            enabled = true,
            badgeText = "overrideBadge",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE,
        ) {}

        item.action = browserActionOverride
        item.invalidate(view)

        verify(labelView).text = "override"
        verify(badgeView).text = "overrideBadge"
        verify(labelView).invalidate()
        verify(badgeView).invalidate()
    }

    @Test
    fun `GIVEN setIcon was called, WHEN bind is called, icon setup uses the tint set`() = runTest {
        val webExtMenuItem = spy(WebExtensionBrowserMenuItem(mock(), mock()))
        val testIconTintColorResource = R.color.accent_material_dark
        val menu: WebExtensionBrowserMenu = mock()
        val imageView: ImageView = mock()
        val badgeView: TextView = mock()
        val labelView: TextView = mock()
        val container = View(testContext)
        val view: View = mock()

        whenever(view.findViewById<ImageView>(R.id.action_image)).thenReturn(imageView)
        whenever(view.findViewById<TextView>(R.id.badge_text)).thenReturn(badgeView)
        whenever(view.findViewById<TextView>(R.id.action_label)).thenReturn(labelView)
        whenever(view.findViewById<View>(R.id.container)).thenReturn(container)
        whenever(view.context).thenReturn(mock())
        whenever(imageView.measuredHeight).thenReturn(2)

        webExtMenuItem.setIconTint(testIconTintColorResource)
        webExtMenuItem.bind(menu, view)

        val viewCaptor = argumentCaptor<View>()
        val imageViewCaptor = argumentCaptor<ImageView>()
        val tintCaptor = argumentCaptor<Int>()

        verify(webExtMenuItem).setupIcon(viewCaptor.capture(), imageViewCaptor.capture(), tintCaptor.capture())

        assertEquals(testIconTintColorResource, tintCaptor.value)
        assertEquals(view, viewCaptor.value)
        assertEquals(imageView, imageViewCaptor.value)

        assertEquals(testIconTintColorResource, webExtMenuItem.iconTintColorResource)
    }
}
