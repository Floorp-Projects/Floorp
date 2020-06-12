/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray

import android.graphics.Bitmap
import android.view.LayoutInflater
import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.concept.tabstray.Tab
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.base.observer.ObserverRegistry
import mozilla.components.support.images.loader.ImageLoader
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.nullable
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class DefaultTabViewHolderTest {

    @Test
    fun `URL from session is assigned to view`() {
        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val titleView = view.findViewById<TextView>(R.id.mozac_browser_tabstray_title)
        val urlView = view.findViewById<TextView>(R.id.mozac_browser_tabstray_url)

        val holder = DefaultTabViewHolder(view, mockTabsTrayWithStyles())

        assertEquals("", titleView.text)

        val session = Tab("a", "https://www.mozilla.org")

        holder.bind(session, isSelected = false, observable = mock())

        assertEquals("https://www.mozilla.org", titleView.text)
        assertEquals("www.mozilla.org", urlView.text)
    }

    @Test
    fun `URL text is set to tab URL when exception is thrown`() {
        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val urlView = view.findViewById<TextView>(R.id.mozac_browser_tabstray_url)
        val holder = DefaultTabViewHolder(view, mockTabsTrayWithStyles())
        val session = Tab("a", "about:home")

        holder.bind(session, isSelected = false, observable = mock())

        assertEquals("about:home", urlView.text)
    }

    @Test
    fun `observer gets notified if item is clicked`() {
        val observer: TabsTray.Observer = mock()
        val registry = ObserverRegistry<TabsTray.Observer>().also {
            it.register(observer)
        }

        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val holder = DefaultTabViewHolder(view, mockTabsTrayWithStyles())

        val session = Tab("a", "https://www.mozilla.org")
        holder.bind(session, isSelected = false, observable = registry)

        view.performClick()

        verify(observer).onTabSelected(session)
    }

    @Test
    fun `observer gets notified if tab gets closed`() {
        val observer: TabsTray.Observer = mock()
        val registry = ObserverRegistry<TabsTray.Observer>().also {
            it.register(observer)
        }

        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val holder = DefaultTabViewHolder(view, mockTabsTrayWithStyles())

        val session = Tab("a", "https://www.mozilla.org")
        holder.bind(session, isSelected = true, observable = registry)

        view.findViewById<View>(R.id.mozac_browser_tabstray_close).performClick()

        verify(observer).onTabClosed(session)
    }

    @Test
    fun `url from session is displayed by default`() {
        val observer: TabsTray.Observer = mock()
        val registry = ObserverRegistry<TabsTray.Observer>().also {
            it.register(observer)
        }

        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val holder = DefaultTabViewHolder(view, mockTabsTrayWithStyles())

        val session = Tab("a", "https://www.mozilla.org")
        val titleView = holder.itemView.findViewById<TextView>(R.id.mozac_browser_tabstray_title)
        val urlView = view.findViewById<TextView>(R.id.mozac_browser_tabstray_url)

        holder.bind(session, isSelected = true, observable = registry)

        assertEquals(session.url, titleView.text)
        assertEquals("www.mozilla.org", urlView.text)
    }

    @Test
    fun `title from session is displayed if available`() {
        val observer: TabsTray.Observer = mock()
        val registry = ObserverRegistry<TabsTray.Observer>().also {
            it.register(observer)
        }

        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val holder = DefaultTabViewHolder(view, mockTabsTrayWithStyles())

        val session = Tab("a", "https://www.mozilla.org", title = "Mozilla Firefox")
        val titleView = holder.itemView.findViewById<TextView>(R.id.mozac_browser_tabstray_title)
        val urlView = view.findViewById<TextView>(R.id.mozac_browser_tabstray_url)

        holder.bind(session, isSelected = true, observable = registry)

        assertEquals("Mozilla Firefox", titleView.text)
        assertEquals("www.mozilla.org", urlView.text)
    }

    @Test
    fun `thumbnail from session is assigned to thumbnail image view`() {
        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val thumbnailView = view.findViewById<ImageView>(R.id.mozac_browser_tabstray_thumbnail)

        val holder = DefaultTabViewHolder(view, mockTabsTrayWithStyles())
        assertEquals(null, thumbnailView.drawable)

        val emptyBitmap = Bitmap.createBitmap(40, 40, Bitmap.Config.ARGB_8888)
        val session = Tab("a", "https://www.mozilla.org", thumbnail = emptyBitmap)

        holder.bind(session, isSelected = false, observable = mock())
        assertTrue(thumbnailView.drawable != null)
    }

    @Test
    fun `thumbnail is set from loader`() {
        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val loader: ImageLoader = mock()
        val viewHolder = DefaultTabViewHolder(view, mockTabsTrayWithStyles(), loader)
        val tabWithThumbnail = Tab("123", "https://example.com", thumbnail = mock())
        val tab = Tab("123", "https://example.com")

        viewHolder.bind(tabWithThumbnail, false, mock())

        verify(loader, never()).loadIntoView(any(), eq("123"), nullable(), nullable())

        viewHolder.bind(tab, false, mock())

        verify(loader).loadIntoView(any(), eq("123"), nullable(), nullable())
    }

    @Test
    fun `thumbnailView does not change when there is no cache or new thumbnail`() {
        val view = LayoutInflater.from(testContext).inflate(R.layout.mozac_browser_tabstray_item, null)
        val viewHolder = DefaultTabViewHolder(view, mockTabsTrayWithStyles())
        val tab = Tab("123", "https://example.com")
        val thumbnailView = view.findViewById<ImageView>(R.id.mozac_browser_tabstray_thumbnail)

        thumbnailView.setImageBitmap(mock())
        val drawable = thumbnailView.drawable

        viewHolder.bind(tab, false, mock())

        assertEquals(drawable, thumbnailView.drawable)
    }

    companion object {
        fun mockTabsTrayWithStyles(): BrowserTabsTray {
            val styles: TabsTrayStyling = mock()

            val tabsTray: BrowserTabsTray = mock()
            doReturn(styles).`when`(tabsTray).styling

            return tabsTray
        }
    }
}
