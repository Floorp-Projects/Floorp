/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter

import android.content.Context
import android.content.res.Resources
import android.graphics.Color
import android.graphics.Typeface
import android.view.View
import android.widget.TextView
import androidx.constraintlayout.widget.ConstraintLayout
import mozilla.components.browser.menu2.R
import mozilla.components.concept.menu.candidate.HighPriorityHighlightEffect
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.clearInvocations
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class TextMenuCandidateViewHolderTest {

    private lateinit var view: ConstraintLayout
    private lateinit var textView: TextView

    @Before
    fun setup() {
        val context: Context = mock()
        view = mock()
        textView = mock()

        doReturn(context).`when`(view).context
        doReturn(textView).`when`(view).findViewById<TextView>(R.id.label)

        val resources: Resources = mock()
        doReturn(resources).`when`(context).resources

        doReturn(mock<Resources.Theme>()).`when`(context).theme
    }

    @Test
    fun `sets container state and text on view`() {
        val holder = TextMenuCandidateViewHolder(view, mock(), mock())

        verify(view).setOnClickListener(holder)

        holder.bind(TextMenuCandidate("hello"))
        verify(view).visibility = View.VISIBLE
        verify(view).isEnabled = true
        verify(textView).text = "hello"
        verify(textView).setTypeface(any(), eq(Typeface.NORMAL))
        verify(textView).textAlignment = View.TEXT_ALIGNMENT_INHERIT
    }

    @Test
    fun `sets on click listener`() {
        var dismissed = false
        var clicked = false
        val holder = TextMenuCandidateViewHolder(view, mock()) { dismissed = true }

        holder.onClick(null)
        assertTrue(dismissed)
        assertFalse(clicked)

        holder.bind(TextMenuCandidate("hello"))
        dismissed = false

        holder.onClick(null)
        assertTrue(dismissed)
        assertFalse(clicked)

        dismissed = false
        holder.bind(TextMenuCandidate("hello") { clicked = true })

        holder.onClick(null)
        assertTrue(dismissed)
        assertTrue(clicked)
    }

    @Test
    fun `sets highlight effect`() {
        val holder = TextMenuCandidateViewHolder(view, mock(), mock())

        holder.bind(TextMenuCandidate("hello"))
        verify(view, never()).setBackgroundColor(anyInt())
        verify(view, never()).setBackgroundResource(anyInt())

        holder.bind(
            TextMenuCandidate(
                "hello",
                effect = HighPriorityHighlightEffect(Color.RED),
            ),
        )
        verify(view).setBackgroundColor(Color.RED)
        verify(view, never()).setBackgroundResource(anyInt())

        clearInvocations(view)

        holder.bind(
            TextMenuCandidate(
                "hello",
                effect = null,
            ),
        )
        verify(view, never()).setBackgroundColor(anyInt())
        verify(view).setBackgroundResource(anyInt())
    }
}
