/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.drawable.BitmapDrawable
import android.view.View
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.delay
import mozilla.components.concept.engine.webextension.Action
import mozilla.components.support.base.android.Padding
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.ext.joinBlocking
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
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class WebExtensionToolbarTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val testDispatcher = coroutinesTestRule.testDispatcher

    @Test
    fun bind() {
        val icon: Bitmap = mock()
        val imageView: ImageView = mock()
        val textView: TextView = mock()
        val view: View = mock()

        whenever(view.findViewById<ImageView>(R.id.action_image)).thenReturn(imageView)
        whenever(view.findViewById<TextView>(R.id.badge_text)).thenReturn(textView)
        whenever(view.context).thenReturn(mock())

        val browserAction = Action(
            title = "title",
            loadIcon = { icon },
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE,
        ) {}

        val action = WebExtensionToolbarAction(browserAction, iconJobDispatcher = testDispatcher) {}
        action.bind(view)
        action.iconJob?.joinBlocking()
        testDispatcher.scheduler.advanceUntilIdle()

        val iconCaptor = argumentCaptor<BitmapDrawable>()
        verify(imageView).setImageDrawable(iconCaptor.capture())
        assertEquals(icon, iconCaptor.value.bitmap)

        verify(imageView).contentDescription = "title"
        verify(textView).setText("badgeText")
        verify(textView).setTextColor(Color.WHITE)
        verify(textView).setBackgroundColor(Color.BLUE)
    }

    @Test
    fun fallbackToDefaultIcon() {
        val imageView: ImageView = mock()
        val textView: TextView = mock()
        val view: View = mock()

        whenever(view.findViewById<ImageView>(R.id.action_image)).thenReturn(imageView)
        whenever(view.findViewById<TextView>(R.id.badge_text)).thenReturn(textView)
        whenever(view.context).thenReturn(mock())

        val browserAction = Action(
            title = "title",
            loadIcon = { throw IllegalArgumentException() },
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE,
        ) {}

        val action = WebExtensionToolbarAction(browserAction, iconJobDispatcher = testDispatcher) {}
        action.bind(view)
        action.iconJob?.joinBlocking()
        testDispatcher.scheduler.advanceUntilIdle()

        verify(imageView).setImageResource(R.drawable.mozac_ic_web_extension_default_icon)
    }

    @Test
    fun createView() {
        var listenerWasClicked = false

        val browserAction = Action(
            title = "title",
            loadIcon = { mock() },
            enabled = false,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE,
        ) {}

        val action = WebExtensionToolbarAction(
            browserAction,
            padding = Padding(1, 2, 3, 4),
            iconJobDispatcher = testDispatcher,
        ) {
            listenerWasClicked = true
        }

        val rootView = action.createView(LinearLayout(testContext))
        rootView.performClick()

        assertFalse(rootView.isEnabled)
        assertTrue(listenerWasClicked)
        assertEquals(rootView.paddingLeft, 1)
        assertEquals(rootView.paddingTop, 2)
        assertEquals(rootView.paddingRight, 3)
        assertEquals(rootView.paddingBottom, 4)
    }

    @Test
    fun cancelLoadIconWhenViewIsDetached() {
        val view: View = mock()
        val imageView: ImageView = mock()
        val textView: TextView = mock()

        whenever(view.findViewById<ImageView>(R.id.action_image)).thenReturn(imageView)
        whenever(view.findViewById<TextView>(R.id.badge_text)).thenReturn(textView)
        whenever(view.context).thenReturn(mock())

        val browserAction = Action(
            title = "title",
            loadIcon = @Suppress("UNREACHABLE_CODE") {
                while (true) { delay(10) }
                mock()
            },
            enabled = false,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE,
        ) {}

        val action = WebExtensionToolbarAction(
            browserAction,
            padding = Padding(1, 2, 3, 4),
            iconJobDispatcher = testDispatcher,
        ) {}

        val attachListenerCaptor = argumentCaptor<View.OnAttachStateChangeListener>()
        val parent = spy(LinearLayout(testContext))
        action.createView(parent)
        verify(parent).addOnAttachStateChangeListener(attachListenerCaptor.capture())

        action.bind(view)
        assertNotNull(action.iconJob)
        assertFalse(action.iconJob?.isCancelled!!)

        attachListenerCaptor.value.onViewDetachedFromWindow(parent)
        testDispatcher.scheduler.advanceUntilIdle()
        assertTrue(action.iconJob?.isCancelled!!)
    }
}
