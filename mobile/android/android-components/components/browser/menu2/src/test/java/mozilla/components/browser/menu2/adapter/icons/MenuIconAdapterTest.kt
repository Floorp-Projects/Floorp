/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu2.adapter.icons

import android.view.LayoutInflater
import androidx.constraintlayout.widget.ConstraintLayout
import mozilla.components.concept.menu.Side
import mozilla.components.concept.menu.candidate.DrawableButtonMenuIcon
import mozilla.components.concept.menu.candidate.DrawableMenuIcon
import mozilla.components.concept.menu.candidate.TextMenuIcon
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.clearInvocations
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

class MenuIconAdapterTest {

    private lateinit var layout: ConstraintLayout
    private lateinit var layoutInflater: LayoutInflater
    private lateinit var dismiss: () -> Unit
    private lateinit var adapter: MenuIconAdapter

    @Before
    fun setup() {
        layout = mock()
        layoutInflater = mock()
        dismiss = mock()
        adapter = spy(MenuIconAdapter(layout, layoutInflater, Side.START, dismiss))
    }

    @Test
    fun `creates viewholder when icon type changes`() {
        val mockViewHolder: MenuIconViewHolder<*> = mock()
        doReturn(mockViewHolder).`when`(adapter).createViewHolder(any())

        adapter.bind(null, null)
        adapter.bind(TextMenuIcon("hello"), TextMenuIcon("world"))
        adapter.bind(null, TextMenuIcon("world"))
        verify(adapter, never()).createViewHolder(any())

        adapter.bind(TextMenuIcon("hello"), null)
        verify(adapter).createViewHolder(TextMenuIcon("hello"))

        clearInvocations(adapter)
        adapter.bind(TextMenuIcon("hello"), DrawableMenuIcon(mock()))
        verify(adapter).createViewHolder(TextMenuIcon("hello"))
    }

    @Test
    fun `disconnects viewholder when icon is changed`() {
        val mockViewHolder: MenuIconViewHolder<*> = mock()
        doReturn(mockViewHolder).`when`(adapter).createViewHolder(any())
        adapter.bind(TextMenuIcon("hello"), null)

        verify(mockViewHolder, never()).disconnect()
        adapter.bind(DrawableButtonMenuIcon(mock()), TextMenuIcon("hello"))
        verify(mockViewHolder).disconnect()

        clearInvocations(mockViewHolder)
        adapter.bind(null, DrawableButtonMenuIcon(mock()))
        verify(mockViewHolder).disconnect()
    }

    @Test
    fun `always bind new icon`() {
        val mockViewHolder: MenuIconViewHolder<*> = mock()
        doReturn(mockViewHolder).`when`(adapter).createViewHolder(any())

        adapter.bind(TextMenuIcon("hello"), null)
        verify(mockViewHolder).bindAndCast(TextMenuIcon("hello"), null)

        adapter.bind(TextMenuIcon("hello"), TextMenuIcon("hello"))
        verify(mockViewHolder).bindAndCast(TextMenuIcon("hello"), TextMenuIcon("hello"))

        clearInvocations(mockViewHolder)
        adapter.bind(null, TextMenuIcon("hello"))
        verify(mockViewHolder, never()).bindAndCast(any(), any())
    }
}
