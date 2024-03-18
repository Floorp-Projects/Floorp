/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter.icons

import android.graphics.Typeface
import android.view.LayoutInflater
import android.view.View
import android.widget.TextView
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.menu2.R
import mozilla.components.concept.menu.Side
import mozilla.components.concept.menu.candidate.TextMenuIcon
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.clearInvocations
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class TextMenuIconViewHolderTest {

    private lateinit var parent: ConstraintLayout
    private lateinit var layoutInflater: LayoutInflater
    private lateinit var textView: TextView

    @Before
    fun setup() {
        parent = mock()
        layoutInflater = mock()
        textView = mock()

        doReturn(testContext).`when`(parent).context
        doReturn(textView).`when`(layoutInflater).inflate(TextMenuIconViewHolder.layoutResource, parent, false)
        doReturn(textView).`when`(textView).findViewById<TextView>(R.id.icon)
    }

    @Test
    fun `sets container state and text on view`() {
        val holder = TextMenuIconViewHolder(parent, layoutInflater, Side.START)

        holder.bindAndCast(TextMenuIcon("hello"), null)
        verify(textView).text = "hello"
        verify(textView).setTypeface(any(), eq(Typeface.NORMAL))
        verify(textView).textAlignment = View.TEXT_ALIGNMENT_INHERIT
    }

    @Test
    fun `removes text view on disconnect`() {
        val holder = TextMenuIconViewHolder(parent, layoutInflater, Side.END)

        verify(parent).setConstraintSet(any())
        verify(parent).addView(textView)
        clearInvocations(parent)

        holder.disconnect()

        verify(parent).setConstraintSet(any())
        verify(parent).removeView(textView)
    }
}
