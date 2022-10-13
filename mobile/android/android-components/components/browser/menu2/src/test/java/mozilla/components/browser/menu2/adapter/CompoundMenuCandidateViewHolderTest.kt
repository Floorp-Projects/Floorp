/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter

import android.content.Context
import android.content.res.Resources
import android.graphics.Color
import android.graphics.Typeface
import android.view.View
import android.widget.CompoundButton
import android.widget.TextView
import androidx.constraintlayout.widget.ConstraintLayout
import mozilla.components.browser.menu2.R
import mozilla.components.concept.menu.candidate.CompoundMenuCandidate
import mozilla.components.concept.menu.candidate.HighPriorityHighlightEffect
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.clearInvocations
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class CompoundMenuCandidateViewHolderTest {

    private val baseCandidate = CompoundMenuCandidate(
        "hello",
        isChecked = false,
        end = CompoundMenuCandidate.ButtonType.CHECKBOX,
    )
    private lateinit var view: ConstraintLayout
    private lateinit var compoundButton: CompoundButton

    @Before
    fun setup() {
        val context: Context = mock()
        view = mock()
        compoundButton = mock()

        doReturn(context).`when`(view).context
        doReturn(compoundButton).`when`(view).findViewById<TextView>(R.id.label)

        val resources: Resources = mock()
        doReturn(resources).`when`(context).resources

        doReturn(mock<Resources.Theme>()).`when`(context).theme
    }

    @Test
    fun `sets container state and text on view`() {
        val holder = CompoundCheckboxMenuCandidateViewHolder(view, mock(), mock())

        holder.bind(baseCandidate)
        verify(view).visibility = View.VISIBLE
        verify(view).isEnabled = true
        verify(compoundButton).text = "hello"
        verify(compoundButton).isChecked = false
        verify(compoundButton).setTypeface(any(), eq(Typeface.NORMAL))
        verify(compoundButton).textAlignment = View.TEXT_ALIGNMENT_INHERIT
    }

    @Test
    fun `sets highlight effect`() {
        val holder = CompoundSwitchMenuCandidateViewHolder(view, mock(), mock())

        holder.bind(baseCandidate)
        verify(view, never()).setBackgroundColor(anyInt())
        verify(view, never()).setBackgroundResource(anyInt())

        holder.bind(baseCandidate.copy(effect = HighPriorityHighlightEffect(Color.RED)))
        verify(view).setBackgroundColor(Color.RED)
        verify(view, never()).setBackgroundResource(anyInt())

        clearInvocations(view)

        holder.bind(baseCandidate.copy(effect = null))
        verify(view, never()).setBackgroundColor(anyInt())
        verify(view).setBackgroundResource(anyInt())
    }

    @Test
    fun `sets change listener`() {
        var dismissed = false
        val holder = CompoundSwitchMenuCandidateViewHolder(view, mock()) { dismissed = true }

        val candidate = baseCandidate.copy(onCheckedChange = mock())
        holder.bind(candidate)
        holder.onCheckedChanged(compoundButton, false)

        assertTrue(dismissed)
        verify(candidate.onCheckedChange).invoke(false)
    }
}
