/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.widget

import android.appwidget.AppWidgetManager
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.unmockkStatic
import io.mockk.verify
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.gecko.search.SearchWidgetProvider
import org.mozilla.gecko.search.SearchWidgetProviderSize

@RunWith(FenixRobolectricTestRunner::class)
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
            1000 to SearchWidgetProviderSize.LARGE,
        )

        for ((dp, layoutSize) in sizes) {
            assertEquals(layoutSize, SearchWidgetProvider.getLayoutSize(dp))
        }
    }

    @Test
    fun testGetLargeLayout() {
        assertEquals(
            R.layout.search_widget_large,
            SearchWidgetProvider.getLayout(SearchWidgetProviderSize.LARGE, showMic = false),
        )
        assertEquals(
            R.layout.search_widget_large,
            SearchWidgetProvider.getLayout(SearchWidgetProviderSize.LARGE, showMic = true),
        )
    }

    @Test
    fun testGetMediumLayout() {
        assertEquals(
            R.layout.search_widget_medium,
            SearchWidgetProvider.getLayout(SearchWidgetProviderSize.MEDIUM, showMic = false),
        )
        assertEquals(
            R.layout.search_widget_medium,
            SearchWidgetProvider.getLayout(SearchWidgetProviderSize.MEDIUM, showMic = true),
        )
    }

    @Test
    fun testGetSmallLayout() {
        assertEquals(
            R.layout.search_widget_small_no_mic,
            SearchWidgetProvider.getLayout(SearchWidgetProviderSize.SMALL, showMic = false),
        )
        assertEquals(
            R.layout.search_widget_small,
            SearchWidgetProvider.getLayout(SearchWidgetProviderSize.SMALL, showMic = true),
        )
    }

    @Test
    fun testGetExtraSmall2Layout() {
        assertEquals(
            R.layout.search_widget_extra_small_v2,
            SearchWidgetProvider.getLayout(SearchWidgetProviderSize.EXTRA_SMALL_V2, showMic = false),
        )
        assertEquals(
            R.layout.search_widget_extra_small_v2,
            SearchWidgetProvider.getLayout(SearchWidgetProviderSize.EXTRA_SMALL_V2, showMic = true),
        )
    }

    @Test
    fun testGetExtraSmall1Layout() {
        assertEquals(
            R.layout.search_widget_extra_small_v1,
            SearchWidgetProvider.getLayout(SearchWidgetProviderSize.EXTRA_SMALL_V1, showMic = false),
        )
        assertEquals(
            R.layout.search_widget_extra_small_v1,
            SearchWidgetProvider.getLayout(SearchWidgetProviderSize.EXTRA_SMALL_V1, showMic = true),
        )
    }

    @Test
    fun testGetText() {
        val context = mockk<Context>()
        every { context.getString(R.string.search_widget_text_short) } returns "Search"
        every { context.getString(R.string.search_widget_text_long) } returns "Search the web"

        assertEquals(
            "Search the web",
            SearchWidgetProvider.getText(SearchWidgetProviderSize.LARGE, context),
        )
        assertEquals(
            "Search",
            SearchWidgetProvider.getText(SearchWidgetProviderSize.MEDIUM, context),
        )
        assertNull(SearchWidgetProvider.getText(SearchWidgetProviderSize.SMALL, context))
        assertNull(SearchWidgetProvider.getText(SearchWidgetProviderSize.EXTRA_SMALL_V1, context))
        assertNull(SearchWidgetProvider.getText(SearchWidgetProviderSize.EXTRA_SMALL_V2, context))
    }

    @Test
    fun `GIVEN voice search is disabled WHEN createVoiceSearchIntent is called THEN it returns null`() {
        val widgetProvider = SearchWidgetProvider()
        val context: Context = mockk {
            every { settings().shouldShowVoiceSearch } returns false
        }

        val result = widgetProvider.createVoiceSearchIntent(context)

        assertNull(result)
    }

    @Test
    fun `GIVEN widgets set on screen shown WHEN updateAllWidgets is called THEN it sends a broadcast to update all widgets`() {
        try {
            mockkStatic(AppWidgetManager::class)
            val widgetManager: AppWidgetManager = mockk()
            every { AppWidgetManager.getInstance(any()) } returns widgetManager
            val componentNameCaptor = slot<ComponentName>()
            val widgetsToUpdate = intArrayOf(1, 2)
            every { widgetManager.getAppWidgetIds(capture(componentNameCaptor)) } returns widgetsToUpdate
            val context: Context = mockk(relaxed = true)
            val intentCaptor = slot<Intent>()
            every { context.sendBroadcast(capture(intentCaptor)) } just Runs

            SearchWidgetProvider.updateAllWidgets(context)

            verify { context.sendBroadcast(any()) }
            assertEquals(SearchWidgetProvider::class.java.name, componentNameCaptor.captured.className)
            assertEquals(SearchWidgetProvider::class.java.name, intentCaptor.captured.component!!.className)
            assertEquals(AppWidgetManager.ACTION_APPWIDGET_UPDATE, intentCaptor.captured.action)
            @Suppress("DEPRECATION")
            assertEquals(widgetsToUpdate, intentCaptor.captured.extras!!.get(AppWidgetManager.EXTRA_APPWIDGET_IDS))
        } finally {
            unmockkStatic(AppWidgetManager::class)
        }
    }

    @Test
    fun `GIVEN no widgets set shown WHEN updateAllWidgets is called THEN it does not try to update widgets`() {
        try {
            mockkStatic(AppWidgetManager::class)
            val widgetManager: AppWidgetManager = mockk()
            every { AppWidgetManager.getInstance(any()) } returns widgetManager
            val componentNameCaptor = slot<ComponentName>()
            val widgetsToUpdate = intArrayOf()
            every { widgetManager.getAppWidgetIds(capture(componentNameCaptor)) } returns widgetsToUpdate
            val context: Context = mockk(relaxed = true)
            val intentCaptor = slot<Intent>()
            every { context.sendBroadcast(capture(intentCaptor)) } just Runs

            SearchWidgetProvider.updateAllWidgets(context)

            verify(exactly = 0) { context.sendBroadcast(any()) }
        } finally {
            unmockkStatic(AppWidgetManager::class)
        }
    }
}
