/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import android.view.LayoutInflater
import androidx.test.core.app.ApplicationProvider
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import mozilla.components.browser.toolbar.MAX_URI_LENGTH
import mozilla.components.concept.tabstray.Tab
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class TabTrayViewHolderTest {

    @Test
    fun `extremely long URLs are truncated to prevent slowing down the UI`() {
        val view = LayoutInflater.from(ApplicationProvider.getApplicationContext()).inflate(
            R.layout.tab_tray_item, null, false)

        val tabViewHolder = spyk(TabTrayViewHolder(view) { null })
        every { tabViewHolder.updateBackgroundColor(false) } just Runs

        val extremelyLongUrl = "m".repeat(MAX_URI_LENGTH + 1)
        val tab = Tab(
            id = "123",
            url = extremelyLongUrl)
        tabViewHolder.bind(tab, false, mockk())

        assertEquals("m".repeat(MAX_URI_LENGTH), tabViewHolder.urlView?.text)
    }
}
