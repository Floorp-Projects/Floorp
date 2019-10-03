/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.toolbar

import android.graphics.Color
import android.graphics.drawable.Drawable
import android.view.View
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.base.android.Padding
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class WebExtensionToolbarTest {

    @Test
    fun bind() {
        val drawable: Drawable = mock()
        val imageView: ImageView = mock()
        val textView: TextView = mock()
        val view: View = mock()

        whenever(view.findViewById<ImageView>(R.id.action_image)).thenReturn(imageView)
        whenever(view.findViewById<TextView>(R.id.badge_text)).thenReturn(textView)

        val action = WebExtensionToolbarAction(
            imageDrawable = drawable,
            contentDescription = "title",
            enabled = true,
            badgeText = "badgeText",
            badgeTextColor = Color.WHITE,
            badgeBackgroundColor = Color.BLUE
        ) {}

        action.bind(view)

        verify(imageView).setImageDrawable(drawable)
        verify(imageView).contentDescription = "title"
        verify(textView).setText("badgeText")
        verify(textView).setTextColor(Color.WHITE)
        verify(textView).setBackgroundColor(Color.BLUE)
    }

    @Test
    fun createView() {
        val drawable: Drawable = mock()
        var listenerWasClicked = false
        val action = WebExtensionToolbarAction(
            imageDrawable = drawable,
            contentDescription = "title",
            enabled = false,
            badgeText = "badgeText",
            padding = Padding(1, 2, 3, 4)
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
}
