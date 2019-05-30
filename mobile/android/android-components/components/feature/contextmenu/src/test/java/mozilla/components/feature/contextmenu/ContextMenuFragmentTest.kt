/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.contextmenu

import android.view.LayoutInflater
import android.widget.LinearLayout
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.browser.session.Session
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ContextMenuFragmentTest {

    @Test
    fun `Build dialog`() {
        val ids = listOf("A", "B", "C")
        val labels = listOf("Item A", "Item B", "Item C")
        val title = "Hello World"
        val session = Session("https://www.mozilla.org")

        val fragment = spy(ContextMenuFragment.create(session, title, ids, labels))
        doReturn(testContext).`when`(fragment).requireContext()

        val dialog = fragment.onCreateDialog(null)

        assertNotNull(dialog)

        verify(fragment).createDialogTitleView(any())
        verify(fragment).createDialogContentView(any())
    }

    @Test
    fun `Dialog title view`() {
        val ids = listOf("A", "B", "C")
        val labels = listOf("Item A", "Item B", "Item C")
        val title = "Hello World"
        val session = Session("https://www.mozilla.org")

        val fragment = spy(ContextMenuFragment.create(session, title, ids, labels))

        val inflater = LayoutInflater.from(testContext)
        val view = fragment.createDialogTitleView(inflater)

        assertEquals(
            "Hello World",
            view.findViewById<TextView>(R.id.titleView).text
        )
    }

    @Test
    fun `Dialog content view`() {
        val ids = listOf("A", "B", "C")
        val labels = listOf("Item A", "Item B", "Item C")
        val title = "Hello World"
        val session = Session("https://www.mozilla.org")

        val fragment = ContextMenuFragment.create(session, title, ids, labels)

        val inflater = LayoutInflater.from(testContext)
        val view = fragment.createDialogContentView(inflater)

        val adapter = view.findViewById<RecyclerView>(R.id.recyclerView).adapter as ContextMenuAdapter

        assertEquals(3, adapter.itemCount)

        val parent = LinearLayout(testContext)

        val holder = adapter.onCreateViewHolder(parent, 0)

        adapter.bindViewHolder(holder, 0)
        assertEquals("Item A", holder.labelView.text)

        adapter.bindViewHolder(holder, 1)
        assertEquals("Item B", holder.labelView.text)

        adapter.bindViewHolder(holder, 2)
        assertEquals("Item C", holder.labelView.text)
    }

    @Test
    fun `Clicking context menu item notifies fragment`() {
        val ids = listOf("A", "B", "C")
        val labels = listOf("Item A", "Item B", "Item C")
        val title = "Hello World"
        val session = Session("https://www.mozilla.org")

        val fragment = spy(ContextMenuFragment.create(session, title, ids, labels))
        doNothing().`when`(fragment).dismiss()

        val inflater = LayoutInflater.from(testContext)
        val view = fragment.createDialogContentView(inflater)

        val adapter = view.findViewById<RecyclerView>(R.id.recyclerView).adapter as ContextMenuAdapter
        val holder = adapter.onCreateViewHolder(LinearLayout(testContext), 0)
        adapter.bindViewHolder(holder, 0)

        holder.labelView.performClick()

        verify(fragment).onItemSelected(0)
    }

    @Test
    fun `On selection fragment notifies feature and dismisses dialog`() {
        val ids = listOf("A", "B", "C")
        val labels = listOf("Item A", "Item B", "Item C")
        val title = "Hello World"
        val session = Session("https://www.mozilla.org")

        val feature: ContextMenuFeature = mock()

        val fragment = spy(ContextMenuFragment.create(session, title, ids, labels))
        fragment.feature = feature
        doNothing().`when`(fragment).dismiss()

        fragment.onItemSelected(0)

        verify(feature).onMenuItemSelected(session.id, "A")
        verify(fragment).dismiss()
    }
}
