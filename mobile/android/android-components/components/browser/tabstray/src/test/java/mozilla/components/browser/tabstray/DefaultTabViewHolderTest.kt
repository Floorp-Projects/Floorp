/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray

import android.content.res.ColorStateList
import android.graphics.Bitmap
import android.graphics.drawable.ColorDrawable
import android.view.LayoutInflater
import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.base.images.ImageLoadRequest
import mozilla.components.concept.base.images.ImageLoader
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.nullable
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class DefaultTabViewHolderTest {

    @Test
    fun `URL from session is assigned to view`() {
        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val titleView = view.findViewById<TextView>(R.id.mozac_browser_tabstray_title)
        val urlView = view.findViewById<TextView>(R.id.mozac_browser_tabstray_url)

        val holder = DefaultTabViewHolder(view)

        assertEquals("", titleView.text)

        val session = createTab(id = "a", url = "https://www.mozilla.org")

        holder.bind(session, isSelected = false, styling = mock(), mock())

        assertEquals("https://www.mozilla.org", titleView.text)
        assertEquals("www.mozilla.org", urlView.text)
    }

    @Test
    fun `URL text is set to tab URL when exception is thrown`() {
        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val urlView = view.findViewById<TextView>(R.id.mozac_browser_tabstray_url)
        val holder = DefaultTabViewHolder(view)
        val session = createTab(id = "a", url = "about:home")

        holder.bind(session, isSelected = false, styling = mock(), mock())

        assertEquals("about:home", urlView.text)
    }

    @Test
    fun `observer gets notified if item is clicked`() {
        val delegate: TabsTray.Delegate = mock()

        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val holder = DefaultTabViewHolder(view)

        val session = createTab(url = "https://www.mozilla.org", id = "a")
        holder.bind(session, isSelected = false, styling = mock(), delegate)

        view.performClick()

        verify(delegate).onTabSelected(session)
    }

    @Test
    fun `observer gets notified if tab gets closed`() {
        val delegate: TabsTray.Delegate = mock()

        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val holder = DefaultTabViewHolder(view)

        val session = createTab(url = "https://www.mozilla.org", id = "a")
        holder.bind(session, isSelected = true, styling = mock(), delegate)

        view.findViewById<View>(R.id.mozac_browser_tabstray_close).performClick()

        verify(delegate).onTabClosed(session)
    }

    @Test
    fun `url from session is displayed by default`() {
        val delegate: TabsTray.Delegate = mock()
        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val holder = DefaultTabViewHolder(view)

        val session = createTab(url = "https://www.mozilla.org", id = "a")
        val titleView = holder.itemView.findViewById<TextView>(R.id.mozac_browser_tabstray_title)
        val urlView = view.findViewById<TextView>(R.id.mozac_browser_tabstray_url)

        holder.bind(session, isSelected = true, styling = mock(), delegate)

        assertEquals(session.content.url, titleView.text)
        assertEquals("www.mozilla.org", urlView.text)
    }

    @Test
    fun `title from session is displayed if available`() {
        val delegate: TabsTray.Delegate = mock()
        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val holder = DefaultTabViewHolder(view)

        val session = createTab(url = "https://www.mozilla.org", title = "Mozilla Firefox", id = "a")
        val titleView = holder.itemView.findViewById<TextView>(R.id.mozac_browser_tabstray_title)
        val urlView = view.findViewById<TextView>(R.id.mozac_browser_tabstray_url)

        holder.bind(session, isSelected = true, styling = mock(), delegate)
        assertEquals("Mozilla Firefox", titleView.text)
        assertEquals("www.mozilla.org", urlView.text)
    }

    @Test
    fun `thumbnail from session is assigned to thumbnail image view`() {
        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val thumbnailView = view.findViewById<ImageView>(R.id.mozac_browser_tabstray_thumbnail)

        val holder = DefaultTabViewHolder(view)
        assertEquals(null, thumbnailView.drawable)

        val emptyBitmap = Bitmap.createBitmap(40, 40, Bitmap.Config.ARGB_8888)
        val session = createTab(id = "a", url = "https://www.mozilla.org", thumbnail = emptyBitmap)

        holder.bind(session, isSelected = false, styling = mock(), mock())
        assertTrue(thumbnailView.drawable != null)
    }

    @Test
    fun `thumbnail is set from loader`() {
        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val loader: ImageLoader = mock()
        val viewHolder = DefaultTabViewHolder(view, loader)
        val tabWithThumbnail =
            createTab(id = "123", url = "https://example.com", thumbnail = mock())
        val tab = createTab(id = "123", url = "https://example.com")

        viewHolder.bind(tabWithThumbnail, false, mock(), mock())

        verify(loader, never()).loadIntoView(any(), eq(ImageLoadRequest("123", 100)), nullable(), nullable())

        viewHolder.bind(tab, false, mock(), mock())

        verify(loader).loadIntoView(any(), eq(ImageLoadRequest("123", 100)), nullable(), nullable())
    }

    @Test
    fun `thumbnailView does not change when there is no cache or new thumbnail`() {
        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val viewHolder = DefaultTabViewHolder(view)
        val tab = createTab(id = "123", url = "https://example.com")
        val thumbnailView = view.findViewById<ImageView>(R.id.mozac_browser_tabstray_thumbnail)

        thumbnailView.setImageBitmap(mock())
        val drawable = thumbnailView.drawable

        viewHolder.bind(tab, false, mock(), mock())

        assertEquals(drawable, thumbnailView.drawable)
    }

    @Test
    fun `bind sets the values for this instance's Tab and TabsTrayStyling properties`() {
        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val viewHolder = DefaultTabViewHolder(view)
        val tab = createTab(id = "123", url = "https://example.com")
        val styling: TabsTrayStyling = mock()

        assertNull(viewHolder.tab)
        assertNull(viewHolder.styling)

        viewHolder.bind(tab, false, styling, mock())

        assertSame(tab, viewHolder.tab)
        assertSame(styling, viewHolder.styling)
    }

    @Test
    fun `bind shows an item as selected or not`() {
        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val tab = createTab(id = "123", url = "https://example.com")
        val viewHolder = spy(DefaultTabViewHolder(view))

        viewHolder.bind(tab, false, mock(), mock())
        verify(viewHolder).updateSelectedTabIndicator(false)

        viewHolder.bind(tab, true, mock(), mock())
        verify(viewHolder).updateSelectedTabIndicator(true)
    }

    @Test
    fun `updateSelectedTabIndicator should further delegate to the appropriate method`() {
        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val viewHolder = spy(DefaultTabViewHolder(view))

        viewHolder.updateSelectedTabIndicator(showAsSelected = true)
        verify(viewHolder).showItemAsSelected()

        viewHolder.updateSelectedTabIndicator(showAsSelected = false)
        verify(viewHolder).showItemAsNotSelected()
    }

    @Test
    fun `showItemAsSelected should use TabsTrayStyling for indicating that an item is currently selected`() {
        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val viewHolder = spy(DefaultTabViewHolder(view))
        val tab = createTab(id = "123", url = "https://example.com")
        val styling = TabsTrayStyling()

        // Need to be called first to set the styling for this holder
        viewHolder.bind(tab, false, TabsTrayStyling(), mock())
        viewHolder.updateSelectedTabIndicator(true)

        assertEquals(styling.selectedItemTextColor, viewHolder.titleView.textColors.defaultColor)
        assertEquals(styling.selectedItemBackgroundColor, (viewHolder.itemView.background as ColorDrawable).color)
        assertEquals(ColorStateList.valueOf(styling.selectedItemTextColor), viewHolder.closeView.imageTintList)
    }

    @Test
    fun `showItemAsSelected should use TabsTrayStyling for indicating that an item is not currently selected`() {
        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val viewHolder = spy(DefaultTabViewHolder(view))
        val tab = createTab(id = "123", url = "https://example.com")
        val styling = TabsTrayStyling()

        // Need to be called first to set the styling for this holder
        viewHolder.bind(tab, true, TabsTrayStyling(), mock())
        viewHolder.updateSelectedTabIndicator(false)

        assertEquals(styling.itemTextColor, viewHolder.titleView.textColors.defaultColor)
        assertEquals(styling.itemBackgroundColor, (viewHolder.itemView.background as ColorDrawable).color)
        assertEquals(ColorStateList.valueOf(styling.itemTextColor), viewHolder.closeView.imageTintList)
    }
}
