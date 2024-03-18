/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter

import android.content.res.ColorStateList
import android.graphics.Color
import android.view.View
import androidx.appcompat.widget.AppCompatImageButton
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.SmallMenuCandidate
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.atLeastOnce
import org.mockito.Mockito.clearInvocations
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class SmallMenuCandidateViewHolderTest {

    private lateinit var view: AppCompatImageButton

    @Before
    fun setup() {
        view = spy(AppCompatImageButton(testContext))
    }

    @Test
    fun `binds button data`() {
        val holder = SmallMenuCandidateViewHolder(view, mock())

        verify(view).setOnClickListener(holder)

        holder.bind(SmallMenuCandidate("hello", DrawableMenuIcon(null)))

        verify(view).contentDescription = "hello"
        verify(view).setImageDrawable(null)
        verify(view).imageTintList = null
        verify(view).visibility = View.VISIBLE
        verify(view).isEnabled = true

        clearInvocations(view)

        holder.bind(
            SmallMenuCandidate(
                "hello",
                DrawableMenuIcon(null, tint = Color.BLUE),
            ),
        )
        verify(view).setImageDrawable(null)
        verify(view).imageTintList = ColorStateList.valueOf(Color.BLUE)
    }

    @Test
    fun `sets on click listener`() {
        var dismissed = false
        var clicked = false
        val holder = SmallMenuCandidateViewHolder(view) { dismissed = true }

        holder.onClick(null)
        assertTrue(dismissed)
        assertFalse(clicked)

        holder.bind(SmallMenuCandidate("hello", DrawableMenuIcon(null)))
        dismissed = false

        holder.onClick(null)
        assertTrue(dismissed)
        assertFalse(clicked)

        dismissed = false
        holder.bind(
            SmallMenuCandidate(
                "hello",
                DrawableMenuIcon(null),
            ) {
                clicked = true
            },
        )

        holder.onClick(null)
        assertTrue(dismissed)
        assertTrue(clicked)
    }

    @Test
    fun `sets on long click listener`() {
        var dismissed = false
        var clicked = false
        val holder = SmallMenuCandidateViewHolder(view) { dismissed = true }

        holder.onLongClick(null)
        assertTrue(dismissed)
        assertFalse(clicked)

        holder.bind(SmallMenuCandidate("hello", DrawableMenuIcon(null)))
        verify(view).setOnClickListener(holder)
        verify(view, atLeastOnce()).isLongClickable = false
        dismissed = false

        assertFalse(holder.onLongClick(null))
        assertTrue(dismissed)
        assertFalse(clicked)

        dismissed = false
        holder.bind(
            SmallMenuCandidate(
                "hello",
                DrawableMenuIcon(null),
                onLongClick = {
                    clicked = true
                    true
                },
            ) {},
        )
        verify(view).isLongClickable = true

        assertTrue(holder.onLongClick(null))
        assertTrue(dismissed)
        assertTrue(clicked)
    }
}
