/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.widget

import mozilla.components.feature.search.R
import mozilla.components.feature.search.widget.AppSearchWidgetProvider.Companion.getLayout
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class AppSearchWidgetProviderTest {

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
            assertEquals(layoutSize, AppSearchWidgetProvider.getLayoutSize(dp))
        }
    }

    @Test
    fun testGetLargeLayout() {
        assertEquals(
            R.layout.mozac_search_widget_large,
            getLayout(SearchWidgetProviderSize.LARGE, showMic = false),
        )
        assertEquals(
            R.layout.mozac_search_widget_large,
            getLayout(SearchWidgetProviderSize.LARGE, showMic = true),
        )
    }

    @Test
    fun testGetMediumLayout() {
        assertEquals(
            R.layout.mozac_search_widget_medium,
            getLayout(SearchWidgetProviderSize.MEDIUM, showMic = false),
        )
        assertEquals(
            R.layout.mozac_search_widget_medium,
            getLayout(SearchWidgetProviderSize.MEDIUM, showMic = true),
        )
    }

    @Test
    fun testGetSmallLayout() {
        assertEquals(
            R.layout.mozac_search_widget_small_no_mic,
            getLayout(SearchWidgetProviderSize.SMALL, showMic = false),
        )
        assertEquals(
            R.layout.mozac_search_widget_small,
            getLayout(SearchWidgetProviderSize.SMALL, showMic = true),
        )
    }

    @Test
    fun testGetExtraSmall2Layout() {
        assertEquals(
            R.layout.mozac_search_widget_extra_small_v2,
            getLayout(
                SearchWidgetProviderSize.EXTRA_SMALL_V2,
                showMic = false,
            ),
        )
        assertEquals(
            R.layout.mozac_search_widget_extra_small_v2,
            getLayout(
                SearchWidgetProviderSize.EXTRA_SMALL_V2,
                showMic = true,
            ),
        )
    }

    @Test
    fun testGetExtraSmall1Layout() {
        assertEquals(
            R.layout.mozac_search_widget_extra_small_v1,
            getLayout(
                SearchWidgetProviderSize.EXTRA_SMALL_V1,
                showMic = false,
            ),
        )
        assertEquals(
            R.layout.mozac_search_widget_extra_small_v1,
            getLayout(
                SearchWidgetProviderSize.EXTRA_SMALL_V1,
                showMic = true,
            ),
        )
    }

    @Test
    fun testGetText() {
        assertEquals(
            testContext.getString(R.string.search_widget_text_long),
            AppSearchWidgetProvider.getText(
                SearchWidgetProviderSize.LARGE,
                testContext,
            ),
        )
        assertEquals(
            testContext.getString(R.string.search_widget_text_short),
            AppSearchWidgetProvider.getText(
                SearchWidgetProviderSize.MEDIUM,
                testContext,
            ),
        )
        assertNull(
            AppSearchWidgetProvider.getText(
                SearchWidgetProviderSize.SMALL,
                testContext,
            ),
        )
        assertNull(
            AppSearchWidgetProvider.getText(
                SearchWidgetProviderSize.EXTRA_SMALL_V1,
                testContext,
            ),
        )
        assertNull(
            AppSearchWidgetProvider.getText(
                SearchWidgetProviderSize.EXTRA_SMALL_V2,
                testContext,
            ),
        )
    }

    @Test
    fun `GIVEN voice search is disabled WHEN createVoiceSearchIntent is called THEN it returns null`() {
        val appSearchWidgetProvider: AppSearchWidgetProvider =
            mock()
        doReturn(false).`when`(appSearchWidgetProvider).shouldShowVoiceSearch(testContext)

        val result = appSearchWidgetProvider.createVoiceSearchIntent(testContext)

        assertNull(result)
    }
}
