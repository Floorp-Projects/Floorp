/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter

import android.content.res.Resources
import android.graphics.Typeface
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.menu.candidate.DecorativeTextMenuCandidate
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyInt
import org.mockito.Mockito.clearInvocations
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class DecorativeTextMenuCandidateViewHolderTest {

    @Test
    fun `sets container state and text on view`() {
        val view: TextView = mock()
        val holder = DecorativeTextMenuCandidateViewHolder(view, mock())

        holder.bind(DecorativeTextMenuCandidate("hello"))
        verify(view).visibility = View.VISIBLE
        verify(view).isEnabled = true
        verify(view).text = "hello"
        verify(view).setTypeface(any(), eq(Typeface.NORMAL))
        verify(view).textAlignment = View.TEXT_ALIGNMENT_INHERIT
        verify(view, never()).layoutParams = any()
    }

    @Test
    fun `sets view height`() {
        val view: TextView = mock()
        val params = ViewGroup.LayoutParams(0, 0)
        val resources: Resources = mock()
        doReturn(params).`when`(view).layoutParams
        doReturn(resources).`when`(view).resources
        doReturn(48).`when`(resources).getDimensionPixelSize(anyInt())

        val holder = DecorativeTextMenuCandidateViewHolder(view, mock())

        holder.bind(DecorativeTextMenuCandidate("hello", height = 30))
        assertEquals(30, params.height)
        verify(view).layoutParams = params

        clearInvocations(view)

        holder.bind(DecorativeTextMenuCandidate("hello"))
        assertEquals(48, params.height)
        verify(view).layoutParams = params
    }
}
