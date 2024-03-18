/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.contextmenu

import android.view.LayoutInflater
import android.widget.LinearLayout
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.engine.HitResult
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

@RunWith(AndroidJUnit4::class)
class ContextMenuFragmentTest {
    @Test
    fun `Build dialog`() {
        val ids = listOf("A", "B", "C")
        val labels = listOf("Item A", "Item B", "Item C")
        val title = "Hello World"
        val tab = createTab("https://www.mozilla.org")
        val additionalNote = "Additional note"

        val fragment = spy(ContextMenuFragment.create(tab, title, ids, labels, additionalNote))
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
        val tab = createTab("https://www.mozilla.org")
        val additionalNote = "Additional note"

        val fragment = spy(ContextMenuFragment.create(tab, title, ids, labels, additionalNote))

        val inflater = LayoutInflater.from(testContext)
        val view = fragment.createDialogTitleView(inflater)

        assertEquals(
            "Hello World",
            view.findViewById<TextView>(R.id.titleView).text,
        )
    }

    @Test
    fun `CLicking title view expands title`() {
        val ids = listOf("A", "B", "C")
        val labels = listOf("Item A", "Item B", "Item C")
        val title = "Hello World"
        val tab = createTab("https://www.mozilla.org")
        val additionalNote = "Additional note"

        val fragment = spy(ContextMenuFragment.create(tab, title, ids, labels, additionalNote))

        val inflater = LayoutInflater.from(testContext)
        val view = fragment.createDialogTitleView(inflater)
        val titleView = view.findViewById<TextView>(R.id.titleView)

        titleView.performClick()

        assertEquals(
            15,
            titleView.maxLines,
        )
    }

    @Test
    fun `Dialog content view`() {
        val ids = listOf("A", "B", "C")
        val labels = listOf("Item A", "Item B", "Item C")
        val title = "Hello World"
        val tab = createTab("https://www.mozilla.org")
        val additionalNote = "Additional note"

        val fragment = spy(ContextMenuFragment.create(tab, title, ids, labels, additionalNote))
        doReturn(testContext).`when`(fragment).requireContext()

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
        val tab = createTab("https://www.mozilla.org")
        val additionalNote = "Additional note"

        val fragment = spy(ContextMenuFragment.create(tab, title, ids, labels, additionalNote))
        doReturn(testContext).`when`(fragment).requireContext()
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
        val tab = createTab("https://www.mozilla.org")
        val additionalNote = "Additional note"

        val feature: ContextMenuFeature = mock()

        val fragment = spy(ContextMenuFragment.create(tab, title, ids, labels, additionalNote))
        fragment.feature = feature
        doNothing().`when`(fragment).dismiss()

        fragment.onItemSelected(0)

        verify(feature).onMenuItemSelected(tab.id, "A")
        verify(fragment).dismiss()
    }

    @Test
    fun `Fragment shows correct title for IMAGE HitResult`() {
        val ids = listOf("A", "B", "C")
        val labels = listOf("Item A", "Item B", "Item C")
        val tab = createTab("https://www.mozilla.org")
        val titleString = "test title"
        val additionalNote = "Additional note"

        val hitResult = HitResult.IMAGE("https://www.mozilla.org", titleString)
        val title = hitResult.getLink()

        val fragment = spy(ContextMenuFragment.create(tab, title, ids, labels, additionalNote))

        assertEquals(titleString, fragment.title)
    }

    @Test
    fun `Fragment shows src as title for IMAGE HitResult with blank title`() {
        val ids = listOf("A", "B", "C")
        val labels = listOf("Item A", "Item B", "Item C")
        val tab = createTab("https://www.mozilla.org")
        val titleString = " "
        val additionalNote = "Additional note"

        val hitResult = HitResult.IMAGE("https://www.mozilla.org", titleString)
        val title = hitResult.getLink()

        val fragment = spy(ContextMenuFragment.create(tab, title, ids, labels, additionalNote))

        assertEquals("https://www.mozilla.org", fragment.title)
    }

    @Test
    fun `Fragment shows src as title for IMAGE HitResult with null title`() {
        val ids = listOf("A", "B", "C")
        val labels = listOf("Item A", "Item B", "Item C")
        val tab = createTab("https://www.mozilla.org")
        val additionalNote = "Additional note"

        val hitResult = HitResult.IMAGE("https://www.mozilla.org")
        val title = hitResult.getLink()

        val fragment = spy(ContextMenuFragment.create(tab, title, ids, labels, additionalNote))

        assertEquals("https://www.mozilla.org", fragment.title)
    }

    @Test
    fun `Fragment shows uri as title for IMAGE_SRC HitResult`() {
        val ids = listOf("A", "B", "C")
        val labels = listOf("Item A", "Item B", "Item C")
        val tab = createTab("https://www.mozilla.org")
        val additionalNote = "Additional note"

        val hitResult = HitResult.IMAGE_SRC("https://www.mozilla.org", "https://another.com")
        val title = hitResult.getLink()

        val fragment = spy(ContextMenuFragment.create(tab, title, ids, labels, additionalNote))

        assertEquals("https://another.com", fragment.title)
    }

    @Test
    fun `Fragment shows src as title for UNKNOWN HitResult`() {
        val ids = listOf("A", "B", "C")
        val labels = listOf("Item A", "Item B", "Item C")
        val tab = createTab("https://www.mozilla.org")
        val additionalNote = "Additional note"

        val hitResult = HitResult.UNKNOWN("https://www.mozilla.org")
        val title = hitResult.getLink()

        val fragment = spy(ContextMenuFragment.create(tab, title, ids, labels, additionalNote))

        assertEquals("https://www.mozilla.org", fragment.title)
    }

    @Test
    fun `Fragment shows src as title for AUDIO HitResult with blank title`() {
        val ids = listOf("A", "B", "C")
        val labels = listOf("Item A", "Item B", "Item C")
        val tab = createTab("https://www.mozilla.org")
        val titleString = " "
        val additionalNote = "Additional note"

        val hitResult = HitResult.AUDIO("https://www.mozilla.org", titleString)
        val title = hitResult.getLink()

        val fragment = spy(ContextMenuFragment.create(tab, title, ids, labels, additionalNote))

        assertEquals("https://www.mozilla.org", fragment.title)
    }

    @Test
    fun `Fragment shows src as title for AUDIO HitResult with null title`() {
        val ids = listOf("A", "B", "C")
        val labels = listOf("Item A", "Item B", "Item C")
        val tab = createTab("https://www.mozilla.org")
        val additionalNote = "Additional note"

        val hitResult = HitResult.AUDIO("https://www.mozilla.org")
        val title = hitResult.getLink()

        val fragment = spy(ContextMenuFragment.create(tab, title, ids, labels, additionalNote))

        assertEquals("https://www.mozilla.org", fragment.title)
    }

    @Test
    fun `Fragment shows src as title for VIDEO HitResult with blank title`() {
        val ids = listOf("A", "B", "C")
        val labels = listOf("Item A", "Item B", "Item C")
        val tab = createTab("https://www.mozilla.org")
        val titleString = " "
        val additionalNote = "Additional note"

        val hitResult = HitResult.VIDEO("https://www.mozilla.org", titleString)
        val title = hitResult.getLink()

        val fragment = spy(ContextMenuFragment.create(tab, title, ids, labels, additionalNote))

        assertEquals("https://www.mozilla.org", fragment.title)
    }

    @Test
    fun `Fragment shows src as title for VIDEO HitResult with null title`() {
        val ids = listOf("A", "B", "C")
        val labels = listOf("Item A", "Item B", "Item C")
        val tab = createTab("https://www.mozilla.org")
        val additionalNote = "Additional note"

        val hitResult = HitResult.VIDEO("https://www.mozilla.org")
        val title = hitResult.getLink()

        val fragment = spy(ContextMenuFragment.create(tab, title, ids, labels, additionalNote))

        assertEquals("https://www.mozilla.org", fragment.title)
    }

    @Test
    fun `Fragment shows about blank as title for EMAIL HitResult`() {
        val ids = listOf("A", "B", "C")
        val labels = listOf("Item A", "Item B", "Item C")
        val tab = createTab("https://www.mozilla.org")
        val additionalNote = "Additional note"

        val hitResult = HitResult.EMAIL("https://www.mozilla.org")
        val title = hitResult.getLink()

        val fragment = spy(ContextMenuFragment.create(tab, title, ids, labels, additionalNote))

        assertEquals("about:blank", fragment.title)
    }

    @Test
    fun `Fragment shows about blank as title for GEO HitResult`() {
        val ids = listOf("A", "B", "C")
        val labels = listOf("Item A", "Item B", "Item C")
        val tab = createTab("https://www.mozilla.org")
        val additionalNote = "Additional note"

        val hitResult = HitResult.GEO("https://www.mozilla.org")
        val title = hitResult.getLink()

        val fragment = spy(ContextMenuFragment.create(tab, title, ids, labels, additionalNote))

        assertEquals("about:blank", fragment.title)
    }

    @Test
    fun `Fragment shows about blank as title for PHONE HitResult`() {
        val ids = listOf("A", "B", "C")
        val labels = listOf("Item A", "Item B", "Item C")
        val tab = createTab("https://www.mozilla.org")
        val additionalNote = "Additional note"

        val hitResult = HitResult.PHONE("https://www.mozilla.org")
        val title = hitResult.getLink()

        val fragment = spy(ContextMenuFragment.create(tab, title, ids, labels, additionalNote))

        assertEquals("about:blank", fragment.title)
    }
}
