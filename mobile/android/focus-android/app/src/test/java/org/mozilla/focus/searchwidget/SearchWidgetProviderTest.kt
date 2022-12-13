/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.searchwidget

import android.app.PendingIntent
import android.content.Intent
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.utils.PendingIntentUtils
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mozilla.focus.activity.IntentReceiverActivity
import org.mozilla.focus.ext.components
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SearchWidgetProviderTest {

    private lateinit var searchWidgetProvider: SearchWidgetProvider

    @Before
    fun setup() {
        searchWidgetProvider = spy(SearchWidgetProvider())
    }

    @Test
    fun `GIVEN search widget provider WHEN onEnabled is called THEN searchWidgetInstalled from Settings should return true`() {
        searchWidgetProvider.onEnabled(testContext)

        assertEquals(testContext.components.settings.searchWidgetInstalled, true)
    }

    @Test
    fun `GIVEN search widget provider WHEN onDeleted is called THEN searchWidgetInstalled from Settings should return false`() {
        searchWidgetProvider.onDeleted(testContext, intArrayOf(1))

        assertEquals(testContext.components.settings.searchWidgetInstalled, false)
    }

    @Test
    fun `GIVEN search widget provider WHEN createTextSearchIntent is called THEN an PendingIntent should be return`() {
        val textSearchIntent = Intent(testContext, IntentReceiverActivity::class.java)
            .apply {
                this.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
                this.putExtra(IntentReceiverActivity.SEARCH_WIDGET_EXTRA, true)
            }
        val dummyPendingIntent = PendingIntent.getActivity(
            testContext,
            SearchWidgetProvider.REQUEST_CODE_NEW_TAB,
            textSearchIntent,
            PendingIntentUtils.defaultFlags or
                PendingIntent.FLAG_UPDATE_CURRENT,
        )

        assertEquals(searchWidgetProvider.createTextSearchIntent(testContext), dummyPendingIntent)
    }

    @Test
    fun `GIVEN search widget provider WHEN voiceSearchActivity is called THEN VoiceSearchActivity should be return`() {
        assertEquals(searchWidgetProvider.voiceSearchActivity(), VoiceSearchActivity::class.java)
    }
}
