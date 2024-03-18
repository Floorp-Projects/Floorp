/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.content.Context
import android.widget.ImageView
import io.mockk.every
import io.mockk.mockk
import io.mockk.slot
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.engine.gecko.fetch.GeckoViewFetchClient
import mozilla.components.browser.icons.BrowserIcons
import mozilla.components.browser.icons.IconRequest
import org.junit.Assert.assertEquals
import org.junit.Test
import java.lang.ref.WeakReference

class BrowserIconsTest {
    @Test
    fun loadIntoViewTest() {
        val myUrl = "https://mozilla.com"
        val request = IconRequest(url = myUrl)
        val imageView = mockk<ImageView>()
        val context = mockk<Context>()
        val weakReference = slot<WeakReference<ImageView>>()

        every { context.assets } returns mockk()
        every { context.resources.getDimensionPixelSize(any()) } returns 100

        val icons = spyk(
            BrowserIcons(context = context, httpClient = mockk<GeckoViewFetchClient>()),
        )

        every { icons.loadIntoViewInternal(any(), any(), any(), any()) } returns mockk()

        icons.loadIntoView(imageView, myUrl)

        verify { icons.loadIntoView(imageView, request) }
        verify { icons.loadIntoViewInternal(capture(weakReference), request, null, null) }
        assertEquals(imageView, weakReference.captured.get())
    }
}
