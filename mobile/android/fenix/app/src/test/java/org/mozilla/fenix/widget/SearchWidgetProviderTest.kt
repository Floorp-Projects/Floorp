/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.widget

import android.content.Context
import io.mockk.every
import io.mockk.mockk
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.gecko.search.SearchWidgetProvider
import org.mozilla.gecko.search.SearchWidgetProviderSize

class SearchWidgetProviderTest {

    @Test
    fun testGetLayoutSize() {
        val sizes = mapOf(
            0 to SearchWidgetProviderSize.EXTRA_SMALL_V1,
            10 to SearchWidgetProviderSize.EXTRA_SMALL_V1,
            63 to SearchWidgetProviderSize.EXTRA_SMALL_V1,
            64 to SearchWidgetProviderSize.EXTRA_SMALL_V2,
            99 to SearchWidgetProviderSize.EXTRA_SMALL_V2,
            100 to SearchWidgetProviderSize.SMALL,
            191 to SearchWidgetProviderSize.SMALL,
            192 to SearchWidgetProviderSize.MEDIUM,
            255 to SearchWidgetProviderSize.MEDIUM,
            256 to SearchWidgetProviderSize.LARGE,
            1000 to SearchWidgetProviderSize.LARGE
        )

        for ((dp, layoutSize) in sizes) {
            assertEquals(layoutSize, SearchWidgetProvider.getLayoutSize(dp))
        }
    }

    @Test
    fun testGetLargeLayout() {
        assertEquals(
            R.layout.search_widget_large,
            SearchWidgetProvider.getLayout(SearchWidgetProviderSize.LARGE, showMic = false)
        )
        assertEquals(
            R.layout.search_widget_large,
            SearchWidgetProvider.getLayout(SearchWidgetProviderSize.LARGE, showMic = true)
        )
    }

    @Test
    fun testGetMediumLayout() {
        assertEquals(
            R.layout.search_widget_medium,
            SearchWidgetProvider.getLayout(SearchWidgetProviderSize.MEDIUM, showMic = false)
        )
        assertEquals(
            R.layout.search_widget_medium,
            SearchWidgetProvider.getLayout(SearchWidgetProviderSize.MEDIUM, showMic = true)
        )
    }

    @Test
    fun testGetSmallLayout() {
        assertEquals(
            R.layout.search_widget_small_no_mic,
            SearchWidgetProvider.getLayout(SearchWidgetProviderSize.SMALL, showMic = false)
        )
        assertEquals(
            R.layout.search_widget_small,
            SearchWidgetProvider.getLayout(SearchWidgetProviderSize.SMALL, showMic = true)
        )
    }

    @Test
    fun testGetExtraSmall2Layout() {
        assertEquals(
            R.layout.search_widget_extra_small_v2,
            SearchWidgetProvider.getLayout(SearchWidgetProviderSize.EXTRA_SMALL_V2, showMic = false)
        )
        assertEquals(
            R.layout.search_widget_extra_small_v2,
            SearchWidgetProvider.getLayout(SearchWidgetProviderSize.EXTRA_SMALL_V2, showMic = true)
        )
    }

    @Test
    fun testGetExtraSmall1Layout() {
        assertEquals(
            R.layout.search_widget_extra_small_v1,
            SearchWidgetProvider.getLayout(SearchWidgetProviderSize.EXTRA_SMALL_V1, showMic = false)
        )
        assertEquals(
            R.layout.search_widget_extra_small_v1,
            SearchWidgetProvider.getLayout(SearchWidgetProviderSize.EXTRA_SMALL_V1, showMic = true)
        )
    }

    @Test
    fun testGetText() {
        val context = mockk<Context>()
        every { context.getString(R.string.search_widget_text_short) } returns "Search"
        every { context.getString(R.string.search_widget_text_long) } returns "Search the web"

        assertEquals(
            "Search the web",
            SearchWidgetProvider.getText(SearchWidgetProviderSize.LARGE, context)
        )
        assertEquals(
            "Search",
            SearchWidgetProvider.getText(SearchWidgetProviderSize.MEDIUM, context)
        )
        assertNull(SearchWidgetProvider.getText(SearchWidgetProviderSize.SMALL, context))
        assertNull(SearchWidgetProvider.getText(SearchWidgetProviderSize.EXTRA_SMALL_V1, context))
        assertNull(SearchWidgetProvider.getText(SearchWidgetProviderSize.EXTRA_SMALL_V2, context))
    }
}
