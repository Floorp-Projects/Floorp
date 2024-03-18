/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.ext

import android.content.res.ColorStateList
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.Drawable
import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.menu.candidate.ContainerStyle
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.HighPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.LowPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.TextStyle
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyFloat
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.clearInvocations
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class ViewTest {

    @Test
    fun `apply container style affects visible and enabled`() {
        val view: View = mock()
        view.applyStyle(ContainerStyle(), ContainerStyle())
        verify(view, never()).visibility = View.VISIBLE
        verify(view, never()).isEnabled = true

        view.applyStyle(ContainerStyle(isVisible = false), ContainerStyle())
        verify(view).visibility = View.GONE
        verify(view).isEnabled = true

        view.applyStyle(ContainerStyle(isEnabled = false), ContainerStyle())
        verify(view).visibility = View.VISIBLE
        verify(view).isEnabled = false
    }

    @Test
    fun `apply text style affects text styling`() {
        val view: TextView = mock()
        view.applyStyle(TextStyle(), TextStyle())
        verify(view, never()).textSize = anyFloat()
        verify(view, never()).setTextColor(anyInt())
        verify(view, never()).setTypeface(any(), anyInt())
        verify(view, never()).textAlignment = anyInt()

        view.applyStyle(TextStyle(), TextStyle(size = 0f))
        verify(view, never()).textSize = anyFloat()
        verify(view, never()).setTextColor(anyInt())
        verify(view).setTypeface(any(), eq(Typeface.NORMAL))
        verify(view).textAlignment = View.TEXT_ALIGNMENT_INHERIT

        view.applyStyle(
            TextStyle(
                size = 4f,
                color = Color.RED,
                textStyle = Typeface.ITALIC,
                textAlignment = View.TEXT_ALIGNMENT_CENTER,
            ),
            TextStyle(),
        )
        verify(view).textSize = 4f
        verify(view).setTextColor(Color.RED)
        verify(view).setTypeface(any(), eq(Typeface.ITALIC))
        verify(view).textAlignment = View.TEXT_ALIGNMENT_CENTER
    }

    @Test
    fun `apply drawable icon`() {
        val view: ImageView = mock()
        view.applyIcon(DrawableMenuIcon(null), DrawableMenuIcon(null))
        verify(view, never()).setImageDrawable(any())
        verify(view, never()).imageTintList = any()

        val drawable: Drawable = mock()
        view.applyIcon(DrawableMenuIcon(drawable), DrawableMenuIcon(null))
        verify(view).setImageDrawable(drawable)
        verify(view).imageTintList = null

        view.applyIcon(DrawableMenuIcon(null, Color.RED), DrawableMenuIcon(drawable))
        verify(view).setImageDrawable(drawable)
        verify(view).imageTintList = ColorStateList.valueOf(Color.RED)
    }

    @Test
    fun `apply notification effect`() {
        val view: ImageView = mock()
        view.applyNotificationEffect(LowPriorityHighlightEffect(Color.BLUE), LowPriorityHighlightEffect(Color.BLUE))
        view.applyNotificationEffect(null, null)
        verify(view, never()).visibility = anyInt()
        verify(view, never()).imageTintList = any()

        view.applyNotificationEffect(LowPriorityHighlightEffect(Color.BLUE), null)
        verify(view).visibility = View.VISIBLE
        verify(view).imageTintList = ColorStateList.valueOf(Color.BLUE)

        view.applyNotificationEffect(null, LowPriorityHighlightEffect(Color.BLUE))
        verify(view).visibility = View.GONE
        verify(view).imageTintList = null
    }

    @Test
    fun `sets highlight effect`() {
        val view: View = mock()
        doReturn(testContext).`when`(view).context

        view.applyBackgroundEffect(null, null)
        verify(view, never()).setBackgroundColor(anyInt())
        verify(view, never()).setBackgroundResource(anyInt())
        verify(view, never()).foreground = any()

        view.applyBackgroundEffect(HighPriorityHighlightEffect(Color.RED), HighPriorityHighlightEffect(Color.RED))
        verify(view, never()).setBackgroundColor(anyInt())
        verify(view, never()).setBackgroundResource(anyInt())
        verify(view, never()).foreground = any()

        view.applyBackgroundEffect(HighPriorityHighlightEffect(Color.RED), null)
        verify(view).setBackgroundColor(Color.RED)
        verify(view, never()).setBackgroundResource(anyInt())
        verify(view).foreground = any()

        clearInvocations(view)

        view.applyBackgroundEffect(null, HighPriorityHighlightEffect(Color.RED))
        verify(view, never()).setBackgroundColor(anyInt())
        verify(view).setBackgroundResource(anyInt())
        verify(view).foreground = null
    }
}
