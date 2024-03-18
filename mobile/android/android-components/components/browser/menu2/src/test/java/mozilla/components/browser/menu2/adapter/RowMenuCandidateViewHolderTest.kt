/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter

import android.view.LayoutInflater
import android.view.View
import android.widget.LinearLayout
import androidx.appcompat.widget.AppCompatImageButton
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.RowMenuCandidate
import mozilla.components.concept.menu.candidate.SmallMenuCandidate
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.clearInvocations
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class RowMenuCandidateViewHolderTest {

    private lateinit var view: LinearLayout
    private lateinit var button: AppCompatImageButton
    private lateinit var inflater: LayoutInflater

    @Before
    fun setup() {
        view = mock()
        button = spy(AppCompatImageButton(testContext))
        inflater = mock()

        doReturn(button).`when`(inflater).inflate(
            SmallMenuCandidateViewHolder.layoutResource,
            view,
            false,
        )
    }

    @Test
    fun `sets container state on view`() {
        val holder = RowMenuCandidateViewHolder(view, inflater, mock())

        holder.bind(RowMenuCandidate(emptyList()))
        verify(view).visibility = View.VISIBLE
        verify(view).isEnabled = true
        verify(view, never()).addView(button)
    }

    @Test
    fun `creates buttons for small items`() {
        val holder = RowMenuCandidateViewHolder(view, inflater, mock())

        holder.bind(
            RowMenuCandidate(
                listOf(
                    SmallMenuCandidate("hello", DrawableMenuIcon(null)),
                    SmallMenuCandidate("hello", DrawableMenuIcon(null)),
                ),
            ),
        )
        verify(view, times(2)).addView(button)

        clearInvocations(view)

        holder.bind(
            RowMenuCandidate(
                listOf(
                    SmallMenuCandidate("test", DrawableMenuIcon(null)),
                    SmallMenuCandidate("hello", DrawableMenuIcon(null)),
                ),
            ),
        )
        verify(view, never()).removeAllViews()
        verify(view, never()).addView(button)
    }

    @Test
    fun `binds buttons for small items`() {
        val holder = RowMenuCandidateViewHolder(view, inflater, mock())

        holder.bind(
            RowMenuCandidate(
                listOf(
                    SmallMenuCandidate("hello", DrawableMenuIcon(null)),
                ),
            ),
        )

        verify(button).contentDescription = "hello"
        verify(button).setImageDrawable(null)
        verify(button).imageTintList = null
        verify(button).visibility = View.VISIBLE
        verify(button).isEnabled = true
    }
}
