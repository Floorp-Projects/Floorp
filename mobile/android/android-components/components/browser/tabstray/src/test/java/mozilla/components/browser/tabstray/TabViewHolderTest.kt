/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.tabstray

import android.graphics.Bitmap
import android.view.LayoutInflater
import android.view.View
import android.widget.ImageView
import android.widget.TextView
import mozilla.components.browser.session.Session
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.support.test.mock
import mozilla.components.support.base.observer.ObserverRegistry
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class TabViewHolderTest {
    @Test
    fun `URL from session is assigned to view`() {
        val view = LayoutInflater.from(RuntimeEnvironment.application).inflate(R.layout.mozac_browser_tabstray_item, null)
        val urlView = view.findViewById<TextView>(R.id.mozac_browser_tabstray_url)

        val holder = TabViewHolder(view)

        assertEquals("", urlView.text)

        val session = Session("https://www.mozilla.org")

        holder.bind(session, isSelected = false, observable = mock())

        assertEquals("https://www.mozilla.org", urlView.text)
    }

    @Test
    fun `Holder registers to session and updates view`() {
        val view = LayoutInflater.from(RuntimeEnvironment.application).inflate(R.layout.mozac_browser_tabstray_item, null)
        val urlView = view.findViewById<TextView>(R.id.mozac_browser_tabstray_url)

        val holder = TabViewHolder(view)
        val session = Session("https://www.mozilla.org")

        holder.bind(session, isSelected = false, observable = mock())

        assertEquals("https://www.mozilla.org", urlView.text)

        session.url = "https://getpocket.com"

        assertEquals("https://getpocket.com", urlView.text)
    }

    @Test
    fun `After unbind holder is no longer registered to session`() {
        val view = LayoutInflater.from(RuntimeEnvironment.application).inflate(R.layout.mozac_browser_tabstray_item, null)
        val urlView = view.findViewById<TextView>(R.id.mozac_browser_tabstray_url)

        val holder = TabViewHolder(view)
        val session = Session("https://www.mozilla.org")

        holder.bind(session, isSelected = false, observable = mock())

        assertEquals("https://www.mozilla.org", urlView.text)

        holder.unbind()

        session.url = "https://getpocket.com"
        assertEquals("https://www.mozilla.org", urlView.text)
    }

    @Test
    fun `observer gets notified if item is clicked`() {
        val observer: TabsTray.Observer = mock()
        val registry = ObserverRegistry<TabsTray.Observer>().also {
            it.register(observer)
        }

        val view = LayoutInflater.from(RuntimeEnvironment.application).inflate(R.layout.mozac_browser_tabstray_item, null)
        val holder = TabViewHolder(view)

        val session = Session("https://www.mozilla.org")
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

        val view = LayoutInflater.from(RuntimeEnvironment.application).inflate(R.layout.mozac_browser_tabstray_item, null)
        val holder = TabViewHolder(view)

        val session = Session("https://www.mozilla.org")
        holder.bind(session, isSelected = true, observable = registry)

        view.findViewById<View>(R.id.mozac_browser_tabstray_close).performClick()

        verify(observer).onTabClosed(session)
    }

    @Test
    fun `thumbnail from session is assigned to thumbnail image view`() {
        val view = LayoutInflater.from(RuntimeEnvironment.application).inflate(R.layout.mozac_browser_tabstray_item, null)
        val thumbnailView = view.findViewById<ImageView>(R.id.mozac_browser_tabstray_thumbnail)

        val holder = TabViewHolder(view)
        assertEquals(null, thumbnailView.drawable)

        val session = Session("https://www.mozilla.org")
        val emptyBitmap = Bitmap.createBitmap(40, 40, Bitmap.Config.ARGB_8888)
        session.thumbnail = emptyBitmap
        holder.bind(session, isSelected = false, observable = mock())
        assertTrue(thumbnailView.drawable != null)
    }
}
