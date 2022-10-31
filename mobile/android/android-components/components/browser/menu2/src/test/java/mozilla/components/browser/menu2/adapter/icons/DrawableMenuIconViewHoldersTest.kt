/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter.icons

import android.graphics.drawable.Drawable
import android.view.LayoutInflater
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.TextView
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.menu2.R
import mozilla.components.concept.menu.Side
import mozilla.components.concept.menu.candidate.AsyncDrawableMenuIcon
import mozilla.components.concept.menu.candidate.DrawableButtonMenuIcon
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.clearInvocations
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class DrawableMenuIconViewHoldersTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var parent: ConstraintLayout
    private lateinit var layoutInflater: LayoutInflater
    private lateinit var imageView: ImageView
    private lateinit var imageButton: ImageButton

    @Before
    fun setup() {
        parent = mock()
        layoutInflater = mock()
        imageView = mock()
        imageButton = mock()

        doReturn(testContext).`when`(parent).context
        doReturn(testContext.resources).`when`(parent).resources
        doReturn(imageView).`when`(layoutInflater).inflate(DrawableMenuIconViewHolder.layoutResource, parent, false)
        doReturn(imageView).`when`(layoutInflater).inflate(AsyncDrawableMenuIconViewHolder.layoutResource, parent, false)
        doReturn(imageButton).`when`(layoutInflater).inflate(DrawableButtonMenuIconViewHolder.layoutResource, parent, false)
        doReturn(imageView).`when`(imageView).findViewById<TextView>(R.id.icon)
        doReturn(imageButton).`when`(imageButton).findViewById<TextView>(R.id.icon)
    }

    @Test
    fun `icon view holder sets icon on view`() {
        val holder = DrawableMenuIconViewHolder(parent, layoutInflater, Side.END)

        val drawable = mock<Drawable>()
        holder.bindAndCast(DrawableMenuIcon(drawable), null)
        verify(imageView).setImageDrawable(drawable)
        verify(imageView).imageTintList = null
    }

    @Test
    fun `button view holder sets icon on view`() {
        val holder = DrawableButtonMenuIconViewHolder(parent, layoutInflater, Side.START) {}
        verify(imageButton).setOnClickListener(holder)

        val drawable = mock<Drawable>()
        holder.bindAndCast(DrawableButtonMenuIcon(drawable), null)
        verify(imageButton).setImageDrawable(drawable)
        verify(imageButton).imageTintList = null
    }

    @Test
    fun `async view holder sets icon on view`() = runTestOnMain {
        val holder = AsyncDrawableMenuIconViewHolder(parent, layoutInflater, Side.END)

        val drawable = mock<Drawable>()
        holder.bindAndCast(AsyncDrawableMenuIcon(loadDrawable = { _, _ -> drawable }), null)
        verify(imageView).setImageDrawable(null)
        verify(imageView).setImageDrawable(drawable)
    }

    @Test
    fun `async view holder uses loading icon and fallback icon`() = runTestOnMain {
        val logger = mock<Logger>()
        val holder = AsyncDrawableMenuIconViewHolder(parent, layoutInflater, Side.END, logger)

        val loading = mock<Drawable>()
        val fallback = mock<Drawable>()
        holder.bindAndCast(
            AsyncDrawableMenuIcon(
                loadDrawable = { _, _ -> throw Exception() },
                loadingDrawable = loading,
                fallbackDrawable = fallback,
            ),
            null,
        )
        verify(imageView, never()).setImageDrawable(null)
        verify(imageView).setImageDrawable(loading)
        verify(imageView).setImageDrawable(fallback)
    }

    @Test
    fun `icon holder removes image view on disconnect`() {
        val holder = DrawableMenuIconViewHolder(parent, layoutInflater, Side.START)

        verify(parent).setConstraintSet(any())
        verify(parent).addView(imageView)
        clearInvocations(parent)

        holder.disconnect()

        verify(parent).setConstraintSet(any())
        verify(parent).removeView(imageView)
    }

    @Test
    fun `button holder removes image view on disconnect`() {
        val holder = DrawableButtonMenuIconViewHolder(parent, layoutInflater, Side.END) {}

        verify(parent).setConstraintSet(any())
        verify(parent).addView(imageButton)
        clearInvocations(parent)

        holder.disconnect()

        verify(parent).setConstraintSet(any())
        verify(parent).removeView(imageButton)
    }

    @Test
    fun `async holder removes image view on disconnect`() {
        val holder = AsyncDrawableMenuIconViewHolder(parent, layoutInflater, Side.START)

        verify(parent).setConstraintSet(any())
        verify(parent).addView(imageView)
        clearInvocations(parent)

        holder.disconnect()

        verify(parent).setConstraintSet(any())
        verify(parent).removeView(imageView)
    }

    @Test
    fun `button view holder calls dismiss when clicked`() {
        var dismissed = false
        var clicked = false

        val holder = DrawableButtonMenuIconViewHolder(parent, layoutInflater, Side.START) {
            dismissed = true
        }

        holder.onClick(imageButton)
        assertTrue(dismissed)
        assertFalse(clicked)

        val button = DrawableButtonMenuIcon(mock(), onClick = { clicked = true })
        holder.bindAndCast(button, null)
        holder.onClick(imageButton)
        assertTrue(dismissed)
        assertTrue(clicked)
    }
}
