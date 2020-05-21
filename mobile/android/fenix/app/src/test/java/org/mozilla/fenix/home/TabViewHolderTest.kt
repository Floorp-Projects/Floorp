/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import android.view.LayoutInflater
import io.mockk.mockk
import kotlinx.android.synthetic.main.tab_list_row.view.*
import mozilla.components.browser.state.state.MediaState
import mozilla.components.browser.toolbar.MAX_URI_LENGTH
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.sessioncontrol.TabSessionInteractor
import org.mozilla.fenix.home.sessioncontrol.viewholders.TabViewHolder

@RunWith(FenixRobolectricTestRunner::class)
class TabViewHolderTest {

    @Test
    fun `extremely long URLs are truncated to prevent slowing down the UI`() {
        val view = LayoutInflater.from(testContext).inflate(
            R.layout.tab_list_row, null, false)

        val interactor: TabSessionInteractor = mockk()
        val tabViewHolder = TabViewHolder(view, interactor)

        val extremelyLongUrl = "m".repeat(MAX_URI_LENGTH + 1)
        val tab = Tab(
            sessionId = "123",
            url = extremelyLongUrl,
            hostname = extremelyLongUrl,
            title = "test",
            mediaState = MediaState.State.NONE)
        tabViewHolder.bindSession(tab)

        assertEquals("m".repeat(MAX_URI_LENGTH), view.hostname.text)
    }
}
